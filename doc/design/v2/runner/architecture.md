# Runner v2 Architecture

## Layering

```text
API namespace: caller-facing C++ contracts
        |
        v
Internal namespace: sockets, acceptor, worker pool, factory, context manager, context
        |
        v
Protocol v2
```

Related diagrams:

- [layered_architecture.svg](layered_architecture.svg)
- [component_relationships.svg](component_relationships.svg)

Dependencies point downward only. The API namespace contains only project-owned caller-facing contracts. The Internal
namespace may use C++ standard-library types, exceptions, and third-party libraries such as Arrow C++.

Third-party symbols must not leak from the Internal namespace through the API namespace. A dependency is allowed at
the API boundary only when it is vendored into an owned project namespace or communicated through a well-known ABI.
The current well-known ABI exception is the Arrow C Data Interface: `ArrowArray` and `ArrowSchema` may cross the
boundary under its release and ownership contract; Arrow C++ types remain internal.

## Components

### Runner

`Runner` is the composition root. A runner user transfers ownership of a `WorkerFactory` to a production factory.
The production factory constructs a `Runner` with that factory, transport configuration, limits, a `SocketAcceptor`,
a worker pool, and a `ContextManager`. `Runner` validates and connects those components, starts and stops them, and
destroys them in dependency-safe order after all work has completed.

Unit tests use the same construction seam to create a `Runner` with owned fakes, stubs, or instrumented component
implementations. The production factory is responsible only for choosing concrete production implementations; the
runner remains responsible for their configuration and composition.

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

The runner user initially owns `WorkerFactory` and transfers it to the production factory, which transfers ownership
to `Runner`. The factory is used by reusable worker-pool resources to obtain a worker callable or worker object for
an accepted descriptor. `Runner` destroys the factory only after stopping and joining all worker activity.

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
runner user --moves WorkerFactory--> production factory
                                      |
                                      v
                         Runner owns factory, acceptor, pool, and context manager
                                      |
SocketAcceptor -> worker pool -> Worker(fd) -> ContextManager.create(fd) -> protocol Context
                                                                         |
                                                          Socket bytes + Arrow C data values
```

Arrow record batches and schemas may cross the API boundary as `ArrowArray` and `ArrowSchema` under the Arrow C Data
Interface. Arrow C++ objects remain Internal-namespace implementation details and are released according to that
interface's ownership rules.
