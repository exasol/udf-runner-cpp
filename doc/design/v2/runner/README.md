# UDF Runner v2 Design

This directory describes the runner-side architecture for protocol v2. It is a design document set; it does not
define an implementation or commit to a concrete C++ class layout.

The runner is organized into two source-level namespaces:

1. The API namespace contains the caller-facing C++ contracts.
2. The Internal namespace owns sockets, accepting, worker scheduling, protocol contexts, and implementation
   dependencies.

The API namespace must not expose third-party symbols from the Internal namespace. A dependency may cross this
boundary only when it is vendored into an owned project namespace or uses a well-known interoperable ABI, such as the
Arrow C Data Interface.

## Documents

- [architecture.md](architecture.md) defines the namespace boundary and component responsibilities.
- [lifecycle.md](lifecycle.md) defines connection, worker, context, and shutdown lifecycles.

Mermaid `.mmd` files are the diagram sources of truth. Matching `.svg` files are rendered views linked from the
documents.

## Relationship to the protocol

The runner implements the protocol described in the [v2 protocol design](../README.md). In particular:

- [low_level.md](../protocol/low_level.md) defines framing, streams, transport bindings, and close behavior.
- [call_lifecycle.md](../protocol/call_lifecycle.md) defines the generic call abstraction.
- [high_level_calls.md](../protocol/high_level_calls.md) defines `Run`, Function operations, and callbacks.
- [high_level_payloads.md](../protocol/high_level_payloads.md) defines JSON and named payload contracts.

The runner `Context` is the implementation boundary for those protocol rules. This design does not redefine their
wire format.

## Initial transport

The first concrete transport is a Unix-domain stream socket. The socket and acceptor contracts are intentionally
transport-neutral enough to support a future TCP/TLS binding without changing the protocol context contract.
