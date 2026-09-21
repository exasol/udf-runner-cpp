# UDF Runner v2 Design

This directory describes the runner-side architecture for protocol v2. It is a design document set; it does not
define an implementation or commit to a concrete C++ class layout.

The runner is organized into two source-level namespaces:

1. The API namespace contains the caller-facing C++ contracts.
2. The Internal namespace owns sockets, accepting, worker scheduling, protocol contexts, and implementation
   dependencies.

`Runner` is the composition root for those components. A runner user transfers a `WorkerFactory` to a production
factory, which constructs `Runner` and its remaining owned components. Unit tests use the same construction seam with
owned test doubles.

The API namespace must not expose third-party symbols from the Internal namespace. A dependency may cross this
boundary only when it is vendored into an owned project namespace or uses a well-known interoperable ABI, such as the
Arrow C Data Interface.

## Documents

- [architecture.md](architecture.md) defines the namespace boundary and component responsibilities.
- [lifecycle.md](lifecycle.md) defines connection, worker, context, and shutdown lifecycles.
- [context_interface.md](context_interface.md) defines the worker-facing low-level context, call, and control-stream
  contract.
- [context_implementation.md](context_implementation.md) defines the internal queues, background I/O thread,
  notification, batching, and shutdown design.

Mermaid `.mmd` files are the diagram sources of truth. Matching `.svg` files are rendered views linked from the
documents.

## Relationship to the protocol

The runner implements the protocol described in the [v2 protocol design](../README.md). In particular:

- [protocol.md](../protocol/low_level/protocol.md) defines framing, streams, transport bindings, and close behavior.
- [call_lifecycle.md](../protocol/low_level/call_lifecycle.md) defines the generic call abstraction.
- [calls.md](../protocol/high_level/calls.md) defines `Run`, Function operations, and callbacks.
- [payloads.md](../protocol/high_level/payloads.md) defines JSON and named payload contracts.

The runner `Context` is the implementation boundary for those protocol rules. This design does not redefine their
wire format.

## Initial transport

The first concrete transport is a Unix-domain stream socket. The socket and acceptor contracts are intentionally
transport-neutral enough to support a future TCP/TLS binding without changing the protocol context contract.
