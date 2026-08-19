# Runner v2 Architecture

## Layering

```text
Header-only C++ facade
        |
        v
Stable C ABI: opaque handles, vtables, status/error values, Arrow C Data Interface
        |
        v
Private C++ implementation: sockets, acceptor, worker pool, factory, context manager, context
```

Related diagrams:

- [layered_architecture.svg](layered_architecture.svg)
- [component_relationships.svg](component_relationships.svg)

Dependencies point downward only. The private implementation may use C++ standard-library types, exceptions, and
the Arrow C++ library. None of those types cross the C ABI. The header-only facade depends only on the C ABI headers,
the C++ standard library, and the Arrow C Data Interface declarations.

## Components

### Socket

`Socket` represents one connected byte stream. It owns the accepted descriptor and provides the operations required
by the protocol transport: receive bytes, send bytes, observe closure, and close. The initial implementation uses a
Unix-domain stream descriptor. The abstraction must not expose Unix-specific address types to the protocol context.

The socket owns its descriptor after construction. Closing or destroying the socket makes the descriptor unusable;
ownership must not be duplicated implicitly.

### SocketAcceptor

`SocketAcceptor` owns the listening endpoint, performs bind/listen setup, and accepts connected sockets. It submits
each accepted descriptor to the worker scheduler. Accept failures are classified as retryable, shutdown-related, or
fatal and must not silently become worker failures.

The initial acceptor listens on a Unix-domain socket. The interface leaves endpoint configuration and accepted-socket
creation abstract so a TCP/TLS acceptor can be added later.

### WorkerFactory and worker pool

The runner does not decide how workers are constructed. An external owner injects a `WorkerFactory`. The factory is
used by reusable worker-pool resources to obtain a worker callable or worker object for an accepted descriptor.

The pool is reusable, but a worker invocation handles one accepted connection. A worker must not retain a protocol
context or descriptor after its invocation returns. Pool sizing, queue limits, and shutdown policy are explicit runner
configuration rather than hidden global state.

### Worker

A worker receives one accepted descriptor, asks the `ContextManager` to create a protocol context, runs that context
until normal close, peer disconnect, cancellation, or error, and then releases the context and descriptor. The worker
does not parse protocol messages outside the context boundary.

### ContextManager

`ContextManager` converts an owned connected descriptor into a connection-scoped `Context`. It centralizes context
construction, protocol dependencies, limits, cancellation, and cleanup. Context creation failure must close or reclaim
the descriptor according to the documented ownership handoff.

### Context

`Context` is the runner-side interface to protocol v2. It owns one connection and exposes the operations needed by the
protocol implementation: framing, control-stream initialization, stream dispatch, call/data handling, callbacks,
keepalive, and close/error processing.

The context is not an application callback object and is not shared between worker invocations. It is the sole owner
of protocol state for its connection and must enforce the validation, ordering, flow-control, and close rules from the
protocol documents.

## Data and dependency flow

```text
external WorkerFactory
          |
SocketAcceptor -> worker pool -> Worker(fd)
                                  |
                                  v
                         ContextManager.create(fd)
                                  |
                                  v
                         protocol Context
                                  |
                    Socket bytes + Arrow C data values
```

Arrow record batches and schemas cross the public boundary as `ArrowArray` and `ArrowSchema`. Arrow C++ objects are
implementation details and are released according to the Arrow C Data Interface ownership rules.
