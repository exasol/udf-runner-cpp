# Context Implementation Design

## Purpose and boundary

This document describes the internal implementation of the worker-facing `Context` defined in
[context_interface.md](context_interface.md). It does not change the worker-facing API and does not expose queues,
eventfds, sockets, or background threads to the worker.

The context uses one background I/O thread and two one-producer/one-consumer queues:

```text
worker thread -- outbound queue --> background I/O thread -- socket --> peer
worker thread <-- inbound queue  -- background I/O thread <-- socket -- peer
```

The worker never performs socket I/O. The background thread is the sole owner of the connected socket.
One worker thread is the sole owner of the worker-facing context, its call objects, and its control-stream object;
concurrent worker operations on one context are outside the contract.

These are exactly two cross-thread queues. They are not one outbound and one inbound queue per call. The inbound
queue is a multiplexed queue containing envelopes for stream `0`, accepted calls, and calls that have not yet been
accepted. Per-call pending lists and the call registry described below are worker-owned dispatch state, not additional
producer/consumer queues.

## Queue ownership and direction

### Outbound queue

The outbound queue carries logical call and control messages from the worker to the background thread. A data message
retains its unsplit Arrow record batch until the background thread applies authoritative credit and creates wire
frames.

- producer: worker thread;
- consumer: background I/O thread;
- worker enqueue: non-blocking;
- background dequeue: batchable and non-blocking; the event loop blocks only on socket/eventfd readiness and its timer;
- order: FIFO;
- capacity: logically unbounded; allocation is limited only by available process memory;
- ownership: successful enqueue transfers the message and its Arrow ownership leases to the queue.

Protocol backpressure is provided by `Next` byte budgets. The worker must not block merely because the background
thread or socket is temporarily busy. A terminal context state rejects a new enqueue immediately.

### Inbound queue

The inbound queue carries verified messages and their storage leases from the background thread to the worker.

- producer: background I/O thread;
- consumer: worker thread;
- background enqueue: non-blocking;
- worker dequeue: blocking with an optional timeout;
- order: FIFO within each stream and according to the context dispatcher’s ordering contract;
- capacity: logically unbounded; the background thread never blocks waiting for worker consumption;
- ownership: successful enqueue transfers the verified storage lease to the queue.

The worker may dequeue one message or all currently available messages. Batch dequeue must preserve FIFO order and
must not hold queue synchronization while the worker processes the views.

### Multiplexing and logical stream dispatch

Every inbound queue item is an `InboundEnvelope` with at least:

```text
InboundEnvelope {
    StreamClass stream_class       // control or call
    StreamId stream_id             // 0 for control
    VerifiedMessage message        // specialized call or control message
}
```

The background thread is the only producer and appends envelopes in the order in which complete protocol messages
are accepted from the socket. It does not call worker code and does not block waiting for a particular call.

The worker-side dispatcher is the only consumer of the physical inbound queue. It drains a batch and routes each
envelope as follows:

- stream `0` goes to the context's control-stream pending list;
- an envelope for a nonzero `stream_id` goes to that stream's pending list;
- an envelope containing `open_call` creates an inbound-call entry in the context's unaccepted-call list;
- a message for a call that has not yet been accepted is retained with that stream entry, not returned by another call;
- a message for an already-closed stream is discarded as delayed traffic;
- a message for a never-created stream is a protocol error.

The worker-owned call registry keeps a closed-stream set keyed by `stream_id`. Closing a call records its stream ID
before releasing its pending sequence, so delayed messages can be discarded without confusing them with a new call;
stream IDs are never reused on a connection.

The dispatcher validates stream state before routing: `open_call` is legal only in the first call message, stream `0`
messages are decoded as control messages, and `close_connection` is handled as a context shutdown event rather than
being placed in the control-stream pending list. `close_call` remains part of the call's pending `CallMessageView`
so the worker can observe the call's final message before that call is marked closed.

`accept_call()` removes the next entry from the unaccepted-call list and returns a `Call` bound to its `stream_id`. Its
already received opening message remains at the head of that call's pending list. `Call::receive()` and
`ControlStream::receive()` first inspect their pending list, then drain the shared inbound queue and route a batch,
and only then wait. This is what gives each logical consumer stream-local semantics while preserving the two-queue
implementation constraint.

The dispatcher preserves FIFO order for every stream. Messages for different streams may be interleaved in the
physical queue and must not make an unrelated `receive()` return. A global readiness query is true when either the
shared inbound queue or any worker-owned pending list contains an unread message.

## Queue operations

Both queues provide the following conceptual operations:

```text
try_enqueue(message) -> bool
try_enqueue_batch(messages) -> bool
try_dequeue() -> optional<message>
try_dequeue_all_available() -> sequence<message>
```

`try_enqueue_batch()` and `try_dequeue_all_available()` are the batch operations used by the two sides. Enqueueing a
batch either transfers every item or transfers none; a partial transfer is not allowed. `try_dequeue_all_available()`
must be a single producer/consumer
drain, not a loop that repeatedly enters the queue through a blocking API. The returned sequence owns or references
items until the caller finishes processing them. A queue implementation may grow storage in segments, but its
observable capacity is unbounded and it must never make the worker wait for socket progress or make the background
thread wait for worker consumption.

The batch operation removes every item available when the operation begins. Items added concurrently are handled by a
subsequent drain. A batch is an optimization only; it must not alter ordering, ownership, close behavior, or error
visibility.

The queues are SPSC because each queue has exactly one producer and one consumer. If the public context later permits
multiple worker threads to send or receive concurrently, the queue contract must be revisited rather than silently
sharing an SPSC queue.

## Outbound notification with eventfd

The background thread must wait for both socket activity and outbound work. The context therefore owns one eventfd
used only to notify the background thread that the outbound queue may contain work.

The background wait set contains:

```text
connected socket
outbound eventfd
```

The eventfd uses counter semantics. Multiple worker enqueues may produce one coalesced wakeup; the queue, not the
eventfd counter, is the source of truth.

The eventfd is created non-blocking and close-on-exec. The worker uses an atomic `outbound_nonempty`/notification
state to avoid writing for every item: an enqueue that observes an empty queue changes the state and writes `1`.
Further enqueues may omit the write while the state is already signalled. An `EAGAIN` from a non-blocking eventfd
write means that a wakeup is already pending and is not an enqueue failure. Other eventfd errors are terminal. The
background thread drains the counter and clears the notification state only after it has drained the outbound queue;
it checks the queue again before clearing or blocking, closing the empty-to-wait race.

### Worker send path

1. Check the context terminal state.
2. For a data-bearing call message, wait for the call's worker send grant using the optional send deadline.
3. Validate and finalize the logical specialized call or control message builder. Do not split or serialize the data
   batch into wire frames on the worker thread.
4. Consume the worker grant by the whole Arrow buffer-byte size of the submitted batch. A positive grant admits the
   current batch even when that one submission overshoots the grant.
5. Enqueue the owned logical message without blocking.
6. Notify the outbound eventfd if this enqueue changed the queue from empty to non-empty.
7. Return to the worker.

If the context is already terminal, the operation throws the appropriate context exception and does not transfer
ownership.

### Worker-side data flow permission

The background thread owns authoritative `Next` credit and publishes a per-call worker send grant. The grant is a
shared atomic byte allowance plus a generation used for timed waiting; it is not another queue. The worker-side
`Call::send()` path behaves as follows:

1. Run synchronous worker-side dispatch before inspecting the grant.
2. If the grant is zero, wait for a positive grant, the optional send deadline, or terminal state. A timeout returns
   `timed_out` without consuming the builder or its Arrow ownership.
3. Measure the whole unsplit batch's Arrow buffer bytes and consume that amount from the worker grant. A positive grant
   admits the current batch even if this one submission overshoots it.
4. Enqueue the logical batch. The worker never slices it and does not wait for the background thread to finish
   transport-level splitting.
5. The background thread later reconciles the grant with authoritative remaining credit and notifies the worker when
   another batch may be submitted.

The worker grant is only an admission mechanism. The background thread remains authoritative for actual bytes sent,
row-boundary splitting, group-end flags, and waiting for additional `Next` credit.

### Background credit and split state

For every call data direction, the background thread keeps:

```text
authoritative_remaining_credit
pending_logical_batch_or_remainder
worker_send_grant
```

Receiving `Next` increases `authoritative_remaining_credit` and publishes a corresponding positive worker grant. When
the background consumes a logical batch, it charges each generated chunk against authoritative credit, not against the
worker's coarse reservation. If the logical batch is larger than the current credit, the background sends the largest
safe row-aligned prefix, permits at most the configured one-row oversubscription, and retains the remainder until more
credit arrives. After the logical batch is complete, it reconciles unused credit into `worker_send_grant` and notifies
the worker. A worker grant never authorizes the background to exceed actual protocol credit.

### Background send path

1. Wait for socket or eventfd readiness.
2. If eventfd is readable, drain its counter.
3. Drain all currently available outbound messages.
4. For each data-bearing logical message, apply the call's authoritative remaining credit and create one or more
   ordered wire frames. Split only at row boundaries; a group boundary sets `is_end_of_group` on the preceding frame.
5. Send the generated frames in FIFO order.
6. If the logical batch requires more credit, retain its unsent remainder and continue reading the socket until `Next`
   arrives.
7. Reconcile the call's worker send grant and notify the worker when another logical batch may be submitted.
8. Release each logical message only after all generated frames and Arrow ownership handoff are complete.

The socket is non-blocking. A message may require multiple writes. The background thread retains the current
serialized frame and its ownership lease until the complete frame is written; `EAGAIN` leaves it pending and adds
socket writable readiness to the wait set. While a frame is partially written, the loop may still read inbound
frames. A later outbound queue item cannot overtake the retained frame.

The event loop must recheck the outbound queue immediately before blocking again. This prevents a message enqueued
between queue inspection and the next wait from being stranded by a coalesced eventfd notification.

## Inbound notification with atomic wait

The background thread must not block while publishing a received message. The worker may block waiting for inbound
data, so the inbound queue uses a lightweight atomic notification mechanism rather than an eventfd.

Use an atomic availability generation together with `atomic_wait`/`atomic_notify_one` or `atomic_notify_all`:

```text
    inbound_generation: atomic<uint64>
```

The generation is a notification sequence, not the queue length and not the readiness result. After a successful
batch enqueue, the background thread increments it with release ordering and notifies. The worker snapshots the
generation only after checking its pending list, the shared inbound queue, and terminal state. It waits for the
generation to differ from that snapshot, then repeats all checks. Thus a notification that happens before the wait is
observed by the changed generation, and a spurious wakeup is harmless.

### Background receive path

1. Read and frame all currently available socket messages.
2. Verify each frame before creating a specialized view.
3. Build one `InboundEnvelope` per complete message.
4. Enqueue the envelopes as one batch without blocking.
5. Increment `inbound_generation` once for the batch.
6. Notify the worker.

The inbound queue is logically unbounded. The background thread never waits for user code to consume data; it transfers
ownership to the queue and continues processing socket readiness.

### Worker receive path

1. Check the stream's worker-owned pending list.
2. Drain all currently available physical inbound envelopes and route them to pending lists.
3. Check the requested stream's pending list again.
4. If a message exists, return its specialized view.
5. If the context is terminal, throw or return the terminal context result.
6. If no timeout remains, return timeout status.
7. Snapshot `inbound_generation` and wait until it changes or the deadline expires.
8. Repeat all checks after waking.

The queue, worker-owned pending lists, and terminal state must be checked both before and after waiting. A
notification is only a hint; the queues and terminal state are authoritative. The same algorithm implements
`receive_status()` without waiting and `wait_for_message(timeout)` with a deadline. A timeout applies to waiting for
the requested condition only; it does not remove or discard messages.

## Background I/O loop

### Background-thread ownership

The background thread owns all transport-side state:

- the connected non-blocking socket;
- the frame decoder and receive buffer;
- the current partially written outbound frame, if any;
- the pending keepalive deadline;
- the outbound eventfd;
- the protocol connection state;
- the outbound queue consumer and inbound queue producer roles.

The worker never accesses these objects directly. The background thread must not call worker code, wait for a worker
lock, or block on either queue. Its only blocking operation is the event-loop wait for socket/eventfd readiness or the
keepalive deadline.

### Event-loop initialization

The background thread performs the following initialization before entering the loop:

1. Set the connected socket to non-blocking mode.
2. Create or adopt the close-on-exec, non-blocking outbound eventfd.
3. Initialize the frame decoder, receive buffer, write state, and protocol state.
4. Register the socket and eventfd with the chosen readiness mechanism (`poll`, `ppoll`, `epoll`, or an equivalent).
5. Initialize the next keepalive deadline.
6. Publish that the context is ready for worker operations.

The socket is always watched for readable/error/hangup events. It is watched for writable events only when the
background thread has a pending frame or the outbound queue has been drained into its write state. Enabling writable
readiness permanently would cause a busy loop on a normally writable socket. The wait timeout is the earlier of the
next keepalive deadline and any implementation-defined fairness deadline.

### Event-loop cycle

The background thread owns the following logical loop:

```text
while context is active:
    drain any immediately available outbound queue items into write state
    update socket writable interest from write state
    wait for socket readiness or outbound eventfd

    process eventfd readiness
    process keepalive deadline
    process socket error/hangup/EOF
    process socket readable readiness
    process socket writable readiness

    recheck terminal state, queues, and write state
```

The exact event ordering is:

1. Drain the eventfd if it is readable. Reading until `EAGAIN` consumes coalesced worker notifications; the eventfd
   value is never interpreted as a message count.
2. If the keepalive deadline has expired and the connection is still active, create an internal stream-0 `KeepAlive`
   frame and append it to the background-owned write list. Advance the deadline. Keepalives do not use the
   worker-to-background SPSC queue and do not require another thread.
3. Process socket error/hangup flags. A pending protocol or transport error takes precedence over ordinary writes.
4. Read until the non-blocking socket returns `EAGAIN`, EOF, or an error. Each complete frame is validated and added to
   one inbound batch. Incomplete bytes remain in the receive buffer for the next cycle.
5. Publish the inbound batch to the physical inbound queue, increment `inbound_generation`, and notify the worker.
6. Drain the outbound queue into the background-owned write list. This is a batch operation and preserves FIFO order.
7. Write until the socket returns `EAGAIN` or the write list is empty. A partial frame remains at the head of the
   write list; later frames cannot overtake it.
8. If a peer `CloseConnection` was received, stop ordinary reads and writes, enqueue/send the required close reply if
   possible, and perform peer shutdown.
9. Recheck the outbound queue and write list before rebuilding the wait set. If work appeared, process it without
   blocking in the readiness wait.

The read and write drains are bounded by configured per-cycle byte/message budgets. If a peer is continuously readable
or the worker continuously enqueues, the event loop must periodically return to the readiness wait so neither direction
starves the other.

### Receive-side socket processing

Socket reads append bytes to a background-owned receive buffer. When the buffer has multiple writable regions, the
background thread uses `readv` or the transport equivalent to fill those regions in one system call. It falls back to
a scalar read when vectorized input is unavailable or not beneficial. Vectorized input is only a transfer
optimization; framing still determines message boundaries and one read may contain part of one frame, one frame, or
many frames.

After each read or vectorized read, the decoder repeatedly performs:

```text
while receive_buffer contains a complete frame:
    remove exactly one frame
    validate frame length and protocol version
    verify FlatBuffer structure and required fields
    decode stream id and message kind
    if the call message contains Next:
        update authoritative credit and worker send grant before outbound processing
    create verified storage lease
    append InboundEnvelope to current inbound batch
```

If the buffer contains an incomplete frame, the loop retains it without publishing a partial message. A frame-size
limit, malformed framing, failed FlatBuffer verification, invalid stream transition, or invalid Arrow C-interface
import publishes a protocol error and stops normal I/O. EOF with no protocol close is handled as an unexpected peer
close/transport error according to the protocol policy.

The inbound batch is published only after all its envelopes are fully verified. The logically unbounded inbound queue
accepts the batch without blocking and takes ownership of its storage leases.

`Next` remains visible in the inbound `CallMessageView` through a restricted `NextView` containing `reset` and
`row_id`, even though the background thread has already consumed `byte_budget` for authoritative credit accounting. If
a composite message contains both `Next` and other call fields, the credit update occurs before the background thread
processes pending outbound data, while the complete composite message is still delivered to the worker unchanged
except that the internal byte budget is not exposed.

### Send-side socket processing

The outbound queue contains logical messages and their ownership leases. The background thread converts them into
finalized, framed messages before moving the resulting frame regions into its write list. When multiple complete
frames are ready, it uses `writev` or the transport equivalent to write several frame regions in one system call. It
then performs non-blocking writes:

```text
while write_list is not empty:
    result = socket.writev(write_list.remaining_iovecs())
    if result == complete:
        release all completed message leases
        remove all completed messages
    if result == partial:
        advance the first incomplete iovec and remove completed iovecs
        wait for writable readiness
        break
    if result == would_block:
        wait for writable readiness
        break
    if result == error:
        publish transport failure
        release all write leases
        stop
```

The background thread may read while a write is blocked. It must not reorder iovecs or dequeue a later message ahead of
the current partial frame. Once a worker close message has been accepted into the outbound queue, no later worker
message can be accepted, so the close message remains the final outbound protocol message. If vectorized writes are
unavailable, the same write state is processed with scalar writes.

### Wait-set and shutdown rules

Before each readiness wait, the background thread:

1. drains all currently available outbound queue items;
2. checks for a pending partial write;
3. checks the eventfd/queue notification race;
4. constructs the wait set with the socket, eventfd, and socket writable interest when needed;
5. waits until an event, a shutdown request, or an implementation-defined bounded wait interval occurs.

After the wait returns, it never assumes that readiness still exists: every operation handles `EAGAIN` and rechecks
state. A worker enqueue racing with wait-set construction is safe because the enqueue either observes an outstanding
eventfd notification or writes a new notification, and the queue is checked again before sleeping.

On any terminal condition, the background thread follows one shutdown path:

```text
record first terminal cause
publish terminal state with release ordering
stop accepting ordinary queue items
notify inbound waiters
release or finish pending transport-owned items according to close policy
close socket and eventfd ownership
mark background thread stopped
```

The context destructor joins this thread before destroying either physical queue or any storage arena used by queued
messages.

When socket input and outbound work are ready simultaneously, terminal socket errors take precedence, then inbound
messages are framed and published, then outbound messages are sent. The loop must recheck the close state, the
outbound queue, and pending partial writes before waiting again. EOF is a peer close and follows the peer-close
sequence; a malformed frame is a protocol failure.

The background thread never invokes a worker-facing builder, view, `Call`, or worker callback. It may construct
internal FlatBuffer frames from logical outbound envelopes after they leave the queue. Views are created only from
verified bytes and are consumed by the worker after the inbound envelope has crossed the queue boundary.

## Builder, wire-message, and view handoff

The worker-facing specialized builders are not themselves queue items. The handoff is:

```text
CallMessageBuilder / ControlMessageBuilder
    -> validate state and field combinations
    -> logical OutboundEnvelope
    -> physical outbound queue
    -> background credit accounting and data-batch splitting
    -> finish FlatBuffer StreamMessage frame(s)
    -> background socket writer
```

The builder retains Arrow C-interface ownership until logical finalization and outbound enqueue succeed. On a
validation, send-grant timeout, terminal-state, or queue failure, ownership remains with the worker or the builder's
error result according to the interface contract. After successful enqueue, the worker must not mutate or release the
transferred Arrow objects. The background thread owns any Arrow slices and generated wire frames until the complete
logical batch has been sent or discarded during shutdown.

The receive handoff is:

```text
socket bytes
    -> frame boundary and size checks
    -> FlatBuffer verification
    -> generated wire view
    -> CallMessageView / ControlMessageView
    -> InboundEnvelope
    -> worker dispatcher and logical pending list
```

Wire `DataSchema` metadata is converted to the worker-facing `DataSchema` view with its Arrow schema and the
`has_group_id`/`has_row_id` flags. Wire record-batch metadata is converted to the worker-facing `RecordBatch` view
with its Arrow array and `is_end_of_group` flag. The transport does not serialize Arrow arrays or schemas as
FlatBuffers; their Arrow C-interface ownership and lifetime are carried by the message lease.

## Storage and views

The receive path creates a verified immutable storage lease for each received frame. The lease contains:

- the complete frame bytes;
- the generated FlatBuffer `Frame` and `StreamMessage` views;
- any Arrow schema and array ownership leases attached to the message.

`CallMessageView` and `ControlMessageView` retain this lease. Receiving later messages does not invalidate an earlier
view. The inbound queue owns the lease until dequeue; the worker-owned view owns it afterward.

Malformed FlatBuffers are never placed in the inbound queue. They publish a protocol error and transition the context
to a terminal state.

## Terminal state

Use one shared atomic context state visible to the worker and background thread:

```text
Active
ClosingByWorker
ClosingByPeer
Failed
Closed
```

The terminal state is out-of-band. It must not rely on a poison message because user code may never consume that
message.

After a background transport or protocol failure:

1. record the terminal error;
2. stop socket I/O;
3. reject all new outbound enqueues;
4. wake all worker receive waiters;
5. close the socket;
6. make later worker send and receive operations throw the corresponding context exception.

The exceptions distinguish at least:

```text
ContextClosed
ContextTransportError
ContextProtocolError
```

Timeout remains a normal non-terminal receive result. It is not converted into a context exception.

State transitions are one-way:

```text
Active
  -> ClosingByWorker  (worker close, cancel, or destruction)
  -> ClosingByPeer    (peer close or EOF)
  -> Failed           (transport/protocol/queue failure)

ClosingByWorker / ClosingByPeer / Failed
  -> Closed           (socket and background thread have stopped)
```

The first terminal cause wins. A later close or transport error may be recorded as cleanup information but must not
replace the original error returned to the worker. State publication uses release ordering; every worker operation
acquires the state before touching a queue and checks it again after a wait or queue operation.

The context startup sequence is also part of the ownership contract: construct the queues and eventfd, connect and
register the socket, publish `Active`, and only then start accepting worker operations. If thread startup or socket
registration fails, construction fails without exposing a partially active context.

## Worker-initiated close

When the worker sends `ControlMessageBuilder.close_connection()` through the control stream:

1. atomically transition to `ClosingByWorker`; only one closer performs the sequence;
2. reject further worker sends and receives;
3. drain and release all messages currently present in the physical inbound queue and all worker-owned pending lists;
4. enqueue `CloseConnection` as the final outbound message, including any required close/error fields;
5. allow previously queued outbound messages to send in FIFO order;
6. send `CloseConnection` completely, including any partial-write continuation;
7. stop socket polling, close the socket, release remaining outbound items, and transition to `Closed`.

The close message is queued behind messages already accepted by the outbound queue. No worker operation can enqueue
behind it because the state check rejects operations after step 1. If sending the close message fails, the context still
transitions to `Closed` and retains the transport failure as the close diagnostic.

No messages are delivered to the worker after worker-initiated close. Any later worker send or receive throws
`ContextClosed`.

The worker should process messages before requesting close if it needs their contents. The close operation’s inbound
drain is a final cleanup drain, not an opportunity to deliver more messages.

## Context destruction

If the context lifetime ends before the worker explicitly sends `CloseConnection`:

1. atomically transition to `ClosingByWorker` if still `Active`;
2. reject new worker operations;
3. drain worker-visible inbound state so no view outlives the context accidentally;
4. attempt to enqueue `CloseConnection` if the socket is still usable;
5. wake the background thread through the eventfd;
6. let the background thread send the close message if possible, with a bounded shutdown policy;
7. request socket shutdown if the background thread is blocked in a system call;
8. join the background thread before destroying the eventfd, queues, or storage arena;
9. release remaining queue items and Arrow ownership leases.

If the socket has already failed, the close attempt is skipped and the recorded transport failure is retained for
diagnostics.

## Peer-initiated close

When the background thread receives `CloseConnection`:

1. atomically transition to `ClosingByPeer`;
2. stop accepting ordinary inbound and outbound traffic;
3. discard or release ordinary outbound messages that have not reached the socket;
4. send the required `CloseConnection` response if the transport is still usable;
5. close the socket;
6. transition to `Closed`;
7. wake all worker waiters;
8. make later worker send and receive operations throw `ContextClosed`.

The peer close is not delivered as ordinary call traffic after connection shutdown begins.

## Ownership and cleanup rules

- Successful outbound enqueue transfers message and Arrow ownership to the outbound queue.
- Failed outbound enqueue leaves ownership with the worker.
- Successful inbound enqueue transfers view storage ownership to the inbound queue.
- Dequeued inbound views retain their storage independently of the queue.
- Queue destruction releases all remaining message and Arrow ownership exactly once.
- The background thread is the sole socket closer during normal context operation.
- Context destruction joins or otherwise safely terminates the background thread before destroying queue state.
- A received Arrow C-interface object is released exactly once, either when its view/lease is destroyed, when an
  undelivered queue item is discarded during shutdown, or when a failed enqueue returns ownership to the producer.
- The background thread may retain one partially written outbound frame; that frame is released on successful write,
  transport failure, or shutdown, exactly once.
- A generated FlatBuffer accessor is never used after its storage lease is released.

## Required queue and wakeup invariants

The implementation must preserve these invariants in tests and code review:

1. There are exactly two cross-thread queues: one worker-to-background outbound queue and one background-to-worker
   multiplexed inbound queue.
2. Neither queue blocks its producer. The worker never waits for outbound socket progress, and the background thread
   never waits for worker consumption.
3. One worker thread performs all context-side dispatch and queue consumption for a connection.
4. Both queue consumers can remove all currently available items in one batch while preserving FIFO order.
5. An outbound enqueue cannot be stranded between an eventfd drain and the next socket wait.
6. An inbound notification cannot be lost between the worker's readiness check and its atomic wait.
7. Queue items outlive the queue operation through explicit ownership leases; no view points into a recycled buffer.
8. Once close or failure is published, no ordinary message is delivered after the close boundary and no new worker
   message can be queued.
9. Every receive status check is non-consuming; every receive operation consumes exactly one logical message from its
   selected stream.

## Failure and concurrency scenarios

The implementation design must cover:

- worker send while the background thread is blocked in socket wait;
- multiple worker sends coalesced into one eventfd wakeup;
- enqueue racing with the background thread’s next wait;
- background receive while the worker is not receiving;
- worker receive racing with inbound notification;
- timeout racing with message arrival;
- socket failure while outbound messages remain queued;
- socket failure while the worker waits for inbound data;
- worker send or receive after terminal state is published;
- worker close while both queues contain messages;
- peer close while outbound messages remain queued;
- context destruction before explicit worker close;
- user code that does not react to a protocol error or close message.
