# Low-Level Context Stream Management

## Purpose and ownership

This document specifies how the low-level context manages logical streams for
one protocol-v2 connection. It complements
[context_implementation.md](context_implementation.md), which defines the
cross-thread queues and background I/O loop, and
[context_interface.md](context_interface.md), which defines the worker-facing
`Context`, `Call`, and `ControlStream` contracts.

The low-level context is the sole owner of all logical stream state. It owns
the control stream, allocates its locally owned nonzero stream IDs, and
maintains a registry from every live call stream ID to its worker-visible
`Call` state. The background component owns transport work but does not own
`Call` or `ControlStream` instances and does not dispatch to worker-facing
objects.

## Context role and stream-ID allocation

Wire-level stream-ID ownership and allocation rules have one source of truth:
the protocol's [Stream Ownership section](../protocol/low_level/protocol.md#stream-ownership).
This section specifies how the runner supplies and uses an endpoint role; it
does not restate those wire rules.

`ContextManager` must provide a `Client` or `Server` role when it creates a
context. `ContextImpl` passes that immutable role to `StreamRegistry`, which
uses it for local ID allocation and inbound ownership validation. The role is
fixed for the connection lifetime. The construction path is described in
[context_implementation.md](context_implementation.md#endpoint-role-configuration).

When reserving a local ID, `StreamRegistry` advances the sequence assigned to
its configured role. A reservation remains consumed if its opening builder is
abandoned or opening fails. If the sequence is exhausted, allocation fails
locally. During dispatch, the registry uses the role to determine which
endpoint's high-water mark applies to an inactive ID; the validation order is
specified below.

Once a call exists, either peer may send permitted call messages on its
stream. Stream ownership determines who opens the stream, not who may send
later messages on it.

## Queue messages and stream identity

Queue messages do not carry a second, separate `stream_id` field. The outbound
builder's internal message state is bound to the stream ID when the context,
call, or control stream creates it; the background thread uses that ID when it
builds the wire `Frame`. An inbound view exposes the ID from its verified
frame through `stream_id()`, and the context dispatcher uses that value to
route it. This keeps one authoritative ID in each message instead of copying
it into queue metadata.

```text
OutboundMessage = variant<unique_ptr<CallMessageBuilderBase>,
                          unique_ptr<ControlMessageBuilder>>
InboundMessage  = variant<unique_ptr<CallMessageView>,
                          unique_ptr<ControlMessageView>>
```

An outbound builder whose type does not match its bound stream ID is an
internal error. The inbound message-type and stream-ID requirements are listed
in the [validation matrix](#message-type-and-stream-id-validation).

The background component uses the ID only to encode or decode the wire message
and select connection-local transport state. It does not need access to the
context's call registry.

## Inbound dispatch

The dispatcher-to-thread wait, drain, and ownership sequence is described in
[context_implementation.md](context_implementation.md#dispatcher-interaction-with-the-background-thread).

The background thread is the only producer of the physical inbound queue. It
appends messages in the order in which complete protocol messages are accepted
from the socket. It does not call worker code and does not block for a
particular call.

The low-level context's dispatcher is the only consumer of that queue. It
drains a batch and obtains each message's stream ID from its view. For every
message, it applies these checks in order:

### Message-type and stream-ID validation

| Inbound message | Valid stream ID | Additional restriction |
| --- | --- | --- |
| `ControlMessageView` | Exactly `0` | A control view on a nonzero stream is a protocol error. |
| `CallMessageView` | Any nonzero ID | A call view on stream `0` is a protocol error. |
| `CloseConnection` | `0`, inside a `ControlMessageView` | It is invalid in a call message or on a nonzero stream. |
| `OpenCall` | Nonzero, inside a `CallMessageView` | It is valid only as the first message on a newly created peer-owned stream. |

The view-to-stream mismatches in this table are protocol errors, including a
`CallMessageView` on stream `0` and a `ControlMessageView` on a nonzero stream.

1. **Validate the stream ID and message kind.** Apply the view-type,
   stream-ID, and control-only-field rules in the matrix above. Stream `0` is
   reserved for control traffic; call views require a nonzero ID.
2. **Handle control and active streams.** Route valid stream `0` messages to
   the control pending list. An active call may carry permitted traffic in
   either direction, regardless of which endpoint opened it. An active call
   accepts messages except that `open_call` after its first message is a
   protocol error. Enqueue a valid view containing `close_call` and mark that
   the peer close was received. Once the peer close has been received, discard
   later views for that active call, including the rest of the current batch.
   A local outbound close does not stop delivery of already-in-flight peer
   messages.
3. **Classify an inactive nonzero stream using its owner and high-water mark.**
   The context role identifies whether the ID is local-owned or peer-owned;
   consult the corresponding high-water mark:
   - For a local-owned ID at or below the local high-water mark, discard a
     non-`open_call` view as delayed traffic; it may be debug-logged. An
     `open_call` on any local-owned ID is a protocol error, and a local-owned
     ID above the local high-water mark is also a protocol error.
   - For a peer-owned ID at or below the peer high-water mark, discard a
     non-`open_call` view as delayed traffic; it may be debug-logged. An
     `open_call` there is an attempt to reuse an ID and is a protocol error.
     A peer-owned ID above the peer high-water mark can create a stream only
     with a valid first `open_call`; other traffic is a protocol error.
   For example, a `Server` receiving a non-opening view on inactive stream `2`
   discards it as delayed traffic if its local high-water mark is at least `2`,
   and fails the connection otherwise. An `open_call` on stream `2` is invalid
   for a `Server` in either case because that ID is locally owned.
4. **Route, discard, or fail.** A valid peer opening creates an inbound-call
   entry in the unaccepted-call list and advances the peer high-water mark.
   Delayed messages are discarded; any protocol error fails the connection.

A message for a call that has not yet been accepted remains with that stream
entry and is not returned by another call. `close_connection` is handled as a
context shutdown event rather than being placed in the control-stream pending
list.

### Call close ordering

When a `CallMessageView` contains `close_call`, the dispatcher appends the
complete view to that call's pending sequence and marks `peer_close_received`.
It discards subsequent call views for that stream, including later views in
the same drained batch, while preserving messages already queued before the
close-bearing view. This also applies when the opening view contains both
`open_call` and `close_call`: the stream remains in the unaccepted-call order,
and `accept_call()` still returns its `Call` handle.

After a local close is accepted, inbound messages continue to be routed until
a peer close is received. `Call::receive()` returns those messages in FIFO
order; when it returns the peer's close-bearing view, the `Call` becomes closed
and the registry releases the stream state and pending sequence. If no peer
close arrives, this state remains pending across receive timeouts until
cancellation or connection termination. Worker-facing send and receive
results are defined in
[context_interface.md](context_interface.md#closecall-delivery-and-receive-ordering).

`accept_call()` removes the next entry from the unaccepted-call list and
returns a `Call` bound to its `stream_id`. Its already received opening message
remains at the head of that call's pending list. `Call::receive()` and
`ControlStream::receive()` first inspect their pending list, then drain and
route a shared inbound batch, and only then wait. This provides stream-local
consumer semantics while retaining one shared inbound queue.

The dispatcher preserves FIFO order within every stream. Messages for distinct
streams may be interleaved in the physical queue and must not make an unrelated
`receive()` return. A global readiness query is true when either the shared
inbound queue or any context-owned pending list contains an unread message.

## Outbound routing

`open_call()` reserves the next locally owned stream ID, creates and registers
the `Call` state, and enqueues the opening builder, which is already bound to
that ID. It rolls back the registration if enqueue fails; the reserved ID is
still not reused.

`Call::send()` creates a builder bound to its context-managed call ID and
enqueues it. `ControlStream` creates builders bound to ID `0`; neither
worker-facing abstraction exposes numeric IDs to worker code. Outbound
`close_call` acceptance and its send-side state transition are defined in the
[worker-facing contract](context_interface.md#outbound-closecall).
An accepted `open_call()` opening message that also contains `close_call`
initializes the call with its local send side already closed.

## Close and ID lifetime

The context keeps an active-call registry and one high-water mark for each
endpoint role. The local high-water mark advances when a stream ID is reserved;
the peer high-water mark advances when a valid opening is admitted to the
context's unaccepted-call list. An accepted local `CloseCall` marks the state
`ClosePending` and seals outbound sends but retains the stream for inbound
traffic until the peer close is received. On peer `CloseCall`, the context
retains the state until the terminal view is returned, then releases its active
state and pending messages without changing either high-water mark. Non-opening
traffic for an inactive ID at or below its owner's high-water mark is delayed
traffic and is discarded. An `open_call` on a local-owned ID or on a peer-owned
ID at or below the peer high-water mark is a protocol error. An inactive ID
above its owner's high-water mark is also a protocol error unless it is a
valid opening on a peer-owned ID. No per-closed-stream ID set is needed, and
monotonic allocation prevents a closed ID from being reused.

Connection close transitions every live stream to its terminal result and wakes
its waiters.

## Implementation sketches

The following pseudocode illustrates how the context can implement the rules
above. The container types and error-reporting API are illustrative; the
requirements in the preceding sections remain authoritative.

### Proposed class design

This sketch separates the worker-facing context, its logical stream state, and
the background-thread boundary. The class names and method placement are
proposals; the ownership and behavior specified above are normative.

```mermaid
classDiagram
direction LR

class Context {
    <<abstract>>
}
class Call {
    <<abstract>>
}
class ControlStream {
    <<abstract>>
}
class EndpointRole {
    <<enumeration>>
    Client
    Server
}
class ContextManager {
    -EndpointRole endpoint_role_
    +create(connected_socket) Context
}
class ContextImpl {
    -shared_ptr~ContextState~ state_
    -ControlStreamImpl control_stream_
    +accept_call()
    +open_call(opening_message)
}
class ContextState {
    -unique_ptr~BackgroundThreadInterface~ background_thread_
    -StreamRegistry stream_registry_
    -ContextLifecycle lifecycle_
    +accept_call()
    +open_call(opening_message)
    +receive_call_message(stream_id, deadline)
    +receive_control_message(deadline)
    +send_call_message(stream_id, builder)
    +send_control_message(builder)
    +dispatch_inbound_batch()
    +shutdown()
}
class StreamRegistry {
    -EndpointRole endpoint_role_
    -StreamId next_local_stream_id_
    -bool local_id_space_exhausted_
    -StreamId local_high_water_mark_
    -StreamId peer_high_water_mark_
    -Map active_calls_
    -Queue unaccepted_call_ids_
    -Queue control_pending_
    +reserve_local_stream_id()
    +dispatch(view) DispatchResult
    +find_call_state(stream_id) CallState pointer
    +take_call_message(stream_id) optional~CallMessageView~
    +take_control_message() optional~ControlMessageView~
    +mark_local_close_accepted(stream_id)
    +accept_next_call()
    +close_call(stream_id)
}
class CallState {
    -StreamId stream_id_
    -Queue pending_messages_
    -bool local_close_accepted_
    -bool peer_close_received_
    -optional terminal_result_
}
class CallImpl {
    -weak_ptr~ContextState~ state_
    -StreamId stream_id_
    +send(builder)
    +receive()
}
class ControlStreamImpl {
    -weak_ptr~ContextState~ state_
    +send(builder)
    +receive()
}
class BackgroundThreadInterface {
    <<abstract>>
    +wait_until(deadline)
    +enqueue_outbound_message(message)
    +enqueue_outbound_messages(batch)
    +dequeue_inbound_messages()
    +request_shutdown()
}
class BackgroundThreadImpl {
    -OutboundQueue outbound_queue_
    -InboundQueue inbound_queue_
    -IoThread io_thread_
    -ConnectionTransport transport_
}
class OutboundMessage
class InboundMessage
class DispatchResult {
    <<enumeration>>
    Routed
    Discarded
    ProtocolError
    ConnectionShutdown
}

ContextImpl ..|> Context
CallImpl ..|> Call
ControlStreamImpl ..|> ControlStream
ContextManager --> EndpointRole : configured with
ContextManager ..> ContextImpl : constructs with role
ContextImpl *-- ContextState : owns strong reference
ContextState *-- StreamRegistry : owns
ContextState --> StreamRegistry : initializes with role
ContextImpl *-- ControlStreamImpl : exposes stream 0
ContextState --> BackgroundThreadInterface : owns
ContextState ..> CallImpl : creates handles
CallImpl ..> ContextState : weak reference; delegates using stream ID
ControlStreamImpl ..> ContextState : weak reference; delegates using ID 0
StreamRegistry *-- "0..*" CallState : tracks active and unaccepted calls
BackgroundThreadImpl ..|> BackgroundThreadInterface
BackgroundThreadInterface ..> OutboundMessage : accepts
BackgroundThreadInterface ..> InboundMessage : returns
StreamRegistry ..> DispatchResult : reports
```

`StreamRegistry` owns logical stream IDs, high-water marks, per-call pending
messages, the unaccepted-call order, and control-stream pending messages.
`ContextImpl` owns a strong reference to `ContextState`. `ContextState` owns
the registry and background-thread interface and is the registry's only
caller. Registry operations run on the context's worker thread; the registry
does not wait, perform I/O, or access the background-thread interface. The
background thread only moves complete messages through the two queues; it
never reads or mutates registry state.

`CallImpl` and `ControlStreamImpl` store `weak_ptr<ContextState>` handles; a
call also stores its stream ID, while the control stream uses ID `0`. They do
not retain `ContextImpl` or `CallState`. This prevents a call handle from
keeping the connection alive or dereferencing a destroyed context. Each
operation locks the weak pointer and delegates to `ContextState`; if the state
has expired or is closed, it returns the existing `ContextClosed` result.
`ContextState` resolves stream state through `StreamRegistry` and performs the
send, receive, and dispatch operations:

```text
CallImpl::receive(timeout):
    state = state_.lock()
    if state is absent or closed:
        return ContextClosed
    deadline = make_deadline(timeout)
    return state.receive_call_message(stream_id, deadline)

CallImpl::send(builder, timeout):
    state = state_.lock()
    if state is absent or closed:
        return ContextClosed
    return state.send_call_message(stream_id, move(builder), timeout)

ControlStreamImpl::receive(timeout):
    state = state_.lock()
    if state is absent or closed:
        return ContextClosed
    return state.receive_control_message(timeout)

ControlStreamImpl::send(builder):
    state = state_.lock()
    if state is absent or closed:
        return ContextClosed
    return state.send_control_message(move(builder))
```

`CallImpl::receive()` passes its stream ID to `ContextState`, which looks up the
corresponding `CallState` and consumes only that call's pending messages.
`ControlStreamImpl::receive()` follows the same path for the control pending
queue. If the requested queue is empty, `ContextImpl` dispatches the shared
inbound queue through `StreamRegistry` and waits through
`BackgroundThreadInterface`; messages for other streams remain in their own
queues. The complete worker receive loop, including timeout and notification
handling, is specified in
[context_implementation.md](context_implementation.md#worker-receive-path).

The worker-visible opening-message and close-delivery behavior is specified in
[context_interface.md](context_interface.md#closecall-delivery-and-receive-ordering).
In particular, a peer-opened call's queued opening view is returned by its
first receive, while a locally opened call's first receive waits for the peer.

`ContextImpl` creates `ContextState` and passes it to `CallImpl` as a weak
reference whenever it creates a call handle. The registry creates `CallState`
when `open_call()` registers a local stream or when dispatch accepts a peer
opening. The state remains in the registry while
the call is unaccepted and after `accept_call()` creates a `CallImpl` handle.
Returning a handle does not move state into it. The registry removes the state
after the peer's terminal close view is delivered, or when connection
termination cleans up all calls. After a peer close, a still-live call handle
resolves to `PeerClosed`; after context shutdown or destruction, locking its
weak state fails or finds a closed state and returns `ContextClosed`. Handles
therefore may outlive the context as inert objects without extending the
connection lifetime. `BackgroundThreadImpl` is the production adapter; tests
can provide another implementation of `BackgroundThreadInterface`.

Reserve a local ID before creating the opening builder. Once reserved, the ID
and local high-water mark stay consumed even if builder creation or enqueueing
fails:

```text
StreamRegistry::reserve_local_stream_id():
    if local_id_space_exhausted:
        return local_failure

    id = next_local_stream_id
    local_high_water_mark = id
    if id > UINT64_MAX - 2:
        local_id_space_exhausted = true
    else:
        next_local_stream_id = id + 2
    return id
```

The dispatcher validates the message/view combination first. It then handles
control and active streams. For an inactive nonzero stream, it selects the
high-water mark according to the context's endpoint role and distinguishes
delayed traffic from invalid stream creation. Its decision flow can be
sketched as:

```text
StreamRegistry::dispatch(view):
    id = view.stream_id()
    active = active_calls.contains(id)

    if not matches_view_type_and_stream_id_rules(view, id):
        return ProtocolError

    if id == 0:
        if view.contains_close_connection():
            return ConnectionShutdown
        else:
            control_pending.push(view)
        return Routed

    if active:
        if active_calls[id].peer_close_received:
            discard(view)  // after CloseCall, including the rest of this batch
            return Discarded
        if view.contains_open_call():
            return ProtocolError
        active_calls[id].pending.push(view)
        if view.contains_close_call():
            active_calls[id].peer_close_received = true
        return Routed

    if has_local_endpoint_parity(id):
        if view.contains_open_call():
            return ProtocolError  // peer cannot open a locally owned stream
        if id <= local_high_water_mark:
            debug_log_delayed(view)
            discard(view)
            return Discarded
        return ProtocolError  // the local endpoint never reserved this ID

    if id <= peer_high_water_mark:
        if view.contains_open_call():
            return ProtocolError  // stream ID reuse
        debug_log_delayed(view)
        discard(view)              // delayed peer-owned traffic
        return Discarded

    if not view.contains_open_call():
        return ProtocolError

    peer_high_water_mark = id
    active_calls[id] = CallState(
        pending = [view],
        local_close_accepted = false,
        peer_close_received = view.contains_close_call())
    unaccepted_call_ids.push(id)
    return Routed
```

### Background-thread handoff

The `BackgroundThreadInterface` methods, ownership results, and wait events
are defined in
[context_implementation.md](context_implementation.md#low-level-context-boundary).
These sketches show where the stream registry participates in that handoff.

The context drains complete inbound batches and applies the dispatcher above;
the background thread does not inspect the registry or route to worker calls:

```text
ContextImpl::dispatch_inbound_batch():
    for view in background_thread.dequeue_inbound_messages():
        outcome = stream_registry.dispatch(view)
        if outcome == ProtocolError:
            fail_connection_and_request_background_shutdown()
            return
        if outcome == ConnectionShutdown:
            publish_terminal_result_and_request_background_shutdown()
            return
```

For a blocking receive, the context checks the requested stream's pending list,
drains and dispatches a batch, and calls `wait_until(deadline)` only if the
list and inbound queue are empty. `InboundAvailable` restarts the drain;
`ShutdownRequested` publishes the terminal result; `DeadlineExpired` returns a
timeout. The complete receive-loop and notification-race behavior is described
in [context_implementation.md](context_implementation.md#dispatcher-interaction-with-the-background-thread).

An outbound builder already carries the ID assigned by its context object. A
successful enqueue accepts the logical message, not confirmation that the
socket has received it:

```text
ContextImpl::send_call_message(stream_id, caller_builder&&):
    dispatch_available_inbound_messages()
    state = stream_registry.find(stream_id)
    if state.peer_close_received:
        return PeerClosed
    if state.local_close_accepted:
        return ProtocolError

    closes_call = caller_builder.contains_close_call()
    validate caller_builder is bound to stream_id
    message = wrap_as_outbound_message(move(caller_builder))
    if not background_thread.enqueue_outbound_message(message):
        // false leaves message unchanged; recover the builder for the caller.
        caller_builder = extract_call_builder(move(message))
        return enqueue_failure

    if closes_call:
        stream_registry.mark_local_close_accepted(stream_id)
    return accepted
```

The close state and queue-acceptance result form one worker-visible acceptance
transaction. The single worker thread cannot interleave another `Call::send()`
between successful enqueue and the `local_close_accepted` update. Since the
outbound queue is FIFO, all messages accepted before the close remain ahead of
it; the state update rejects every later send.

The production background component consumes accepted messages, uses each
builder's bound stream ID when encoding its wire frame, and writes frames using
its connection-owned transport state. In the other direction, it decodes
complete frames into verified views, appends those views to the inbound queue,
and signals `InboundAvailable`. This component-level flow is illustrative;
the low-level context remains responsible for stream validation and routing.

When a call closes, remove its active state and pending sequence without
changing either high-water mark. Later non-opening traffic for that ID is
discarded as delayed traffic, regardless of which endpoint originally opened
the stream. The dispatch flow above rejects invalid openings and IDs beyond
the high-water mark for their owner.

## Invariants

This design retains exactly two cross-thread queues: one worker-to-background
outbound queue and one background-to-worker inbound queue. The call registry,
unaccepted-call list, per-role high-water marks, and per-stream pending
messages are low-level-context-owned dispatch state, not background-thread
queues.
