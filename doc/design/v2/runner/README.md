# UDF Runner v2 Design

This directory describes the runner-side architecture for protocol v2. It is a design document set; it does not
define an implementation or commit to a concrete C++ class layout.

The runner is organized into three layers:

1. The private C++ implementation owns sockets, accepting, worker scheduling, and protocol contexts.
2. The C ABI exposes stable opaque handles, callback/vtable contracts, status values, and Arrow C Data Interface
   values.
3. A header-only C++ facade provides RAII and typed C++ adapters on top of the C ABI.

## Documents

- [architecture.md](architecture.md) defines the layers and component responsibilities.
- [lifecycle.md](lifecycle.md) defines connection, worker, context, and shutdown lifecycles.
- [abi.md](abi.md) defines the C ABI and the header-only C++ facade contract.

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
