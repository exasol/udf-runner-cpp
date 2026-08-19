# Runner v2 Lifecycle

## Startup

1. The external owner creates transport configuration, a `WorkerFactory`, a `ContextManager`, and runner limits.
2. The runner creates a `SocketAcceptor` and a reusable worker pool.
3. The acceptor binds and listens on the configured Unix-domain endpoint.
4. The runner starts accepting connections and dispatching descriptors.

The runner must reject invalid endpoint configuration before starting worker threads. Startup failure leaves no live
listener or worker-owned descriptor.

## Accepted connection

Related diagrams:

- [connection_lifecycle.svg](connection_lifecycle.svg)
- [descriptor_ownership.svg](descriptor_ownership.svg)

```text
SocketAcceptor
    | accept()
    v
owned connected fd
    | enqueue
    v
reusable worker-pool resource
    | invoke injected WorkerFactory product
    v
Worker(fd)
    | ContextManager.create(fd)
    v
Context(connection)
    | initialize control stream and run protocol
    v
normal close / peer disconnect / cancellation / error
```

The descriptor has exactly one owner at each handoff:

- the acceptor owns it until successful submission;
- the worker owns it while creating the context;
- the context owns it after successful creation;
- the context closes or transfers it during final teardown.

Failed queue submission, worker creation, or context creation must close the descriptor exactly once.

## Worker and context behavior

The worker creates a fresh context for every accepted descriptor. The context then:

1. performs protocol initialization and advertises or validates capabilities as required by the endpoint role;
2. receives and dispatches framed stream messages;
3. manages calls, data streams, callbacks, keepalive, flow control, and validation;
4. handles normal close, error close, cancellation, and peer disconnect;
5. releases all protocol and transport resources before returning.

Worker-pool resources may be reused after the worker returns, but connection-scoped protocol state may not be reused.

## Shutdown

Shutdown has three phases:

1. Stop accepting new descriptors and wake a blocked accept operation.
2. Stop queueing work and request cancellation for queued and active workers.
3. Wait for active contexts to close or reach the configured shutdown deadline, then reclaim remaining resources.

The runner must close the listening socket before reporting that accepting has stopped. It must not destroy injected
factory or context-manager dependencies until all workers have returned.

## Failure cases

| Failure | Required behavior |
| --- | --- |
| Accept failure | Retry transient failures; terminate cleanly for shutdown; surface fatal failures. |
| Queue full | Apply the configured admission policy; do not leak the accepted descriptor. |
| Worker creation failure | Close the descriptor and report the failure through runner diagnostics. |
| Context creation failure | Reclaim the descriptor and return the creation error. |
| Protocol validation error | Let the context perform protocol error/close handling, then terminate the connection if required. |
| Peer disconnect | Let the context release all connection state and return normally unless cleanup fails. |
| Worker exception | Convert it at the ABI boundary to a status/error and guarantee descriptor/context cleanup. |

## Concurrency requirements

- A context is single-owner unless the protocol implementation explicitly documents internal concurrency.
- Worker-pool resources may run concurrently, but no connection-scoped stream state is shared between contexts.
- The acceptor, queue, pool, and context manager must define their thread-safety guarantees in the C++ and C ABI
  contracts.
- Cancellation and shutdown must be safe when they race with accept, queue submission, context creation, or peer close.
