# UDF Transport Technology Overview

This document summarizes technology choices for moving calls and tabular data
between a database engine and user-defined function (UDF) code. It is a
technology overview, not a protocol specification.

## Execution Boundaries

| Boundary | Description | Strengths | Tradeoffs |
| --- | --- | --- | --- |
| In-process ABI | UDF code runs in the database process and receives engine-owned values or vectors through an application binary interface. | Lowest transfer overhead and direct access to engine-native vectors. | A faulty UDF can affect the database process; no isolation or remote deployment boundary. |
| Side process or container | UDF code runs in a separate local process or container. | Isolates language runtimes and failures; supports independent dependencies and long-lived workers. | Requires IPC, serialization, lifecycle management, and resource limits. |
| Remote service | UDF code runs on another host or platform. | Independent scaling, deployment, and fault domain. | Adds network latency and requires authentication, encryption, retry, and failure semantics. |

## Transport and Serialization

| Transport | Typical serialization | Data movement | Appropriate use |
| --- | --- | --- | --- |
| Shared memory | Native in-memory vectors or custom binary layout | Zero-copy or low-copy local batches | Trusted local components that can share memory ownership and lifetime rules. |
| Unix-domain socket | Protobuf, FlatBuffers, Arrow IPC, or custom binary framing | Local bidirectional messages, inline batches, and optional FD-backed buffers | Isolated local workers where filesystem permissions can enforce endpoint access. |
| TCP stream with TLS | Protobuf, FlatBuffers, Arrow IPC, or custom binary framing | Persistent bidirectional messages and inline batches | Local-network or remote workers that need streaming, cancellation, and long-lived sessions. |
| Message middleware | Middleware-defined frames with binary or schema-based payloads | Asynchronous messages or request/reply | Decoupled local or distributed components where the middleware's delivery semantics are suitable. |
| Standard input/output pipes | Delimited text, JSON lines, or a binary stream format | Sequential local input and output | Trusted or tightly supervised executable UDFs and process pools. |
| HTTP(S) request/response | JSON, optionally compressed | Bounded request batches and responses | Interoperable remote scalar or batch calls with simple service integration. |
| RPC/gRPC | Protobuf by default; other encodings are possible | Unary RPC or streaming RPC | Typed remote interfaces that benefit from generated clients, streaming, and service contracts. |

## Untrusted Code and Pipes

Standard input/output pipes are a simple local process interface, not a security
boundary. When the executed UDF code is not trusted, it can emit logs, debug
output, malformed bytes, or partial writes that corrupt the protocol stream. It
can also stop reading input or draining output, filling pipe buffers and causing
both processes to block indefinitely.

Pipes also leave cancellation, process exit, timeout, and partial-result
semantics to the supervising process. Text formats add parsing, escaping, and
type-conversion overhead for large data transfers. A pipe by itself provides no
authentication, encryption, peer identity, or message isolation. Reused
processes can retain state or sensitive input between invocations.

For untrusted code, require all of the following in addition to the pipe
transport:

- Send diagnostics through a separate channel so they cannot corrupt protocol output.
- Use strict framing, input and output validation, and explicit byte limits.
- Supervise process lifetime with timeouts, cancellation, and defined handling for partial output.
- Apply process or container sandboxing, resource limits, restricted filesystem access, and restricted network access.
- Reset worker state before reuse, or terminate the worker after each invocation.

Use a framed socket or RPC protocol instead when execution requires authenticated
identity, multiplexing, structured cancellation, or remote deployment.

## Serialization Formats

| Format | Characteristics | Best fit | Main limitation |
| --- | --- | --- | --- |
| Native vectors | Engine-owned in-memory data structures. | In-process and shared-memory execution. | Couples UDF code to an engine ABI and memory-lifetime rules. |
| Custom binary | Application-defined message fields and typed values. | Controlled local protocols with narrow type requirements. | Requires explicit versioning, framing, and cross-language compatibility work. |
| Protobuf | Schema-defined binary messages with broad language support. | Control messages, metadata, and bounded row-oriented payloads. | Not a columnar table format; bulk data can require conversion and copying. |
| FlatBuffers | Schema-defined binary data designed for direct access. | Framed control messages and metadata where allocation and parsing overhead matter. | Requires careful schema evolution and buffer-lifetime handling. |
| Arrow IPC | Standardized columnar record batches and schemas. | High-throughput analytical table data. | Requires supported type mapping and explicit batch-size and memory-pressure policy. |
| JSON | Human-readable structured data. | Metadata, service integration, and small bounded requests. | Text encoding and row-oriented values are inefficient for large table transfers. |
| Delimited or text rows | Simple ordered fields separated by delimiters or line boundaries. | Small executable integrations and debugging-friendly workflows. | Escaping, type conversion, and parsing overhead limit throughput. |

## Binding-Specific Buffer Handoff

Inline byte transfer is portable across Unix-domain sockets and TCP/TLS. Unix-domain sockets can additionally pass
file descriptors for `memfd`-backed buffers and, where the platform permits it, GPU-memory handles. This requires
explicit descriptor correlation, ownership, lifetime, cleanup, integrity validation, and file-descriptor limits.

TCP/TLS cannot use Unix file-descriptor passing. A remote binding must therefore use inline buffers unless a future
portable out-of-band mechanism is specified and negotiated. The protocol's call, stream, and flow-control semantics
should remain unchanged across these choices.

## Data Movement and Flow Control

| Model | Behavior | Use when | Design requirements |
| --- | --- | --- | --- |
| Scalar request/response | One invocation returns one result. | Inputs are small and independent. | Correlate failures and define timeout and retry behavior. |
| Bounded batch RPC | One request carries multiple rows and returns matching results. | Remote calls benefit from amortizing invocation overhead. | Preserve row order or identifiers; cap rows and bytes per request. |
| Unidirectional stream | A producer sends an ordered sequence of messages or batches. | Results or input data have one primary direction. | Define end-of-stream, cancellation, and receiver backpressure. |
| Bidirectional stream | Both peers send control and data while a call remains active. | The UDF can request more data, send results incrementally, or make callbacks. | Separate control from data, avoid request/reply deadlocks, and define ordering per direction. |
| Credit/window flow control | The receiver grants a byte or batch budget before more data is sent. | Data volume is large or receiver capacity varies. | State the credit unit, initial window, replenishment rule, and behavior on cancellation. |
| Multiplexed logical streams | Independent calls share one physical connection; each stream belongs to that connection only. | Many concurrent calls need one connection per worker or instance. | Scope stream identifiers to the connection, avoid ID reuse on it, isolate failures, and prevent one stream from starving others. |

## Worker Lifecycle and Reliability

- Reuse long-lived workers when startup or model initialization is expensive; define when state is retained, reset, or discarded.
- Bound memory, CPU, process count, queue depth, batch size, and time spent without making progress.
- Treat cancellation, peer disconnect, worker crash, and partial response as explicit protocol states.
- For remote transports, use authenticated endpoints, encrypted connections, peer identity validation, and idempotency rules before retrying an invocation.
- Keep physical transport independent from call semantics so the same call and data model can run over local IPC or a secured network transport.

## Selection Guidance

1. Prefer in-process vectors only when the UDF is trusted and isolation is not required.
2. Prefer Arrow IPC or another columnar format for high-throughput analytical batches.
3. Prefer protobuf or FlatBuffers for typed control messages and metadata.
4. Prefer HTTP(S)/JSON for simple interoperable remote calls, not sustained high-volume table transfer.
5. Prefer a persistent bidirectional stream with explicit credit and multiplexing when calls require callbacks, incremental results, or overlapping control and data traffic.
6. Prefer pipes only for trusted or tightly supervised local code; pipes do not isolate untrusted UDFs.

## Terms

| Term | Meaning |
| --- | --- |
| ABI | A binary calling and memory-layout contract between compiled components. |
| IPC | Communication between processes on the same machine. |
| Batch | A bounded collection of rows or columns transferred as one unit. |
| Framing | Delimiting messages in a byte stream so the receiver can recover message boundaries. |
| Multiplexing | Carrying multiple independent logical streams on one physical connection; stream identity is `(connection, stream_id)`. |
| Flow control | Limiting data in flight to the capacity explicitly granted by the receiver. |
| Serialization | Encoding typed data into bytes for transport or storage. |
