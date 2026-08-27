# New UDF Protocol Design Draft

Detailed protocol references in this directory:

- [low_level.md](low_level.md)
- [high_level_calls.md](high_level_calls.md)
- [high_level_payloads.md](high_level_payloads.md)

# General Information

| **Status** | Draft |

**Design Workflow Steps**: Brainstorming (Team + Architects) -> Write/Update Design -> First Feedback Round (Team + Architects) -> Incorporate Feedback -> Design Feedback Group

# Input From User Perspective

This design introduces a new UDF protocol for communication between `DB` and `UDFRunner`. The user-visible goal is to remove current protocol limitations that make UDF execution slower, harder to evolve, and difficult to secure for remote execution scenarios.

## Problem Statement

The current UDF protocol has several practical problems:

- It is synchronous and therefore inefficient for workloads that would benefit from independent send and receive activity.
- It is hard to evolve because changes to call metadata, callback metadata, import/export specification, and connection information tend to require recompilation.
- It forces a request/response pattern that creates unnecessary round trips. For example, small scalar-return batches can require four round trips.
- It uses serialization that is too slow for the intended workload, especially for table data.
- It only supports a single connection and therefore does not scale well to more advanced execution patterns such as multiple streams and `pquery`.
- It is not sufficiently secure for remote connections.

These problems matter most for containerized or remote UDF execution, where protocol overhead, security requirements, and extensibility all become first-order concerns.

## User Requirements

The new UDF protocol should satisfy the following requirements.

### Core Functional Requirements

- Support `Run`, `Function`, and `Script` execution modes.
- Support metadata exchange for protocol, script, call, and callback interactions.
- Support DB callback operations such as connection lookup, script lookup, and query execution.
- Support multiple logical streams per physical connection, with total ordering within a stream and partial ordering across streams.
- Support flow-controlled table transfer with explicit request semantics such as `Next(...)`.

### Performance Requirements

- Be faster than the current protocol for both small and large batches.
- Reduce unnecessary round trips, especially for scalar or small-batch results.
- Support asynchronous communication so each side can send and receive independently.
- Support efficient transport of table data with minimal copying where practical.
- Handle large rows and large values, including potentially unbounded binary values, without forcing unsafe or impractical buffering strategies.

### Extensibility Requirements

- Make it easy to add new calls.
- Make it easy to add new callbacks.
- Allow call and callback metadata to evolve without forcing frequent recompilation of the DB or UDF client base implementation.
- Keep the wire model open enough to support additional metadata fields and future protocol evolution.

### Deployment and Connectivity Requirements

- Support multiple connections for the `UDFRunner`.
- Support multiple streams per connection to enable more advanced execution and parallel-query scenarios.
- Scope each logical stream to one connection; use `(connection, stream_id)` as its identity and do not reuse an ID on that connection.
- Support both local and remote communication.
- Use Unix-domain stream sockets as the initial binding while preserving an extension path for TCP/TLS deployments.
- Be implementable in multiple programming languages used by UDF runtimes and clients.

### Operational Requirements

- Be reasonably easy to implement for the client side.
- Be reasonably easy to set up and operate.
- Avoid deadlock-prone communication patterns.
- Allow the system to detect stalled or unresponsive peers.

# Security Requirements

The new protocol changes a security-sensitive interface between the database and an external or semi-external execution environment. The design therefore needs explicit requirements for authentication, encryption, integrity, isolation, and availability.

## Trust Model

- Treat `UDFRunner` as untrusted input from the protocol perspective, even if it is started by Exasol-controlled infrastructure.
- Validate received metadata, stream control messages, schemas, and table-buffer references before use.
- Do not rely on a client to behave correctly with respect to message counts, flow control, or buffer ownership.

## Authentication and Authorization

- Remote connections must support authentication.
- The protocol must make it possible to distinguish authorized DB peers from unauthorized remote clients.
- Callback capabilities such as `get_connection`, `get_script`, and `execute_query` must remain DB-controlled capabilities and must not become arbitrary privilege-escalation channels for the client.
- Connection information returned by callbacks must be limited to the data required for the requested operation.

## Transport Security and Confidentiality

- Remote connections must support encryption, preferably TLS-based.
- TCP deployments must use TLS, endpoint authentication, and peer identity validation.
- Sensitive metadata and table data must be protected in transit against passive observation.
- The design should allow secure local and remote deployment modes without weakening remote security to fit local-only mechanisms.

## Integrity Requirements

- The DB must be protected against malicious or malformed stream, batch, and metadata input.
- Buffer-transfer mechanisms must preserve data integrity after handoff. If a mechanism allows a client to mutate data after the DB starts consuming it, the design must either prevent that behavior or treat the mechanism as unsuitable.
- Any out-of-band buffer transport must preserve the integrity relationship between control metadata and the referenced payload.

## Availability and Resource-Safety Requirements

- The protocol must avoid deadlocks caused by both sides blocking on send operations.
- The design must bound or control resource usage such as send buffers, file descriptors, and out-of-band buffer handles.
- The protocol must support keepalive or heartbeat behavior so the DB can detect stalled peers and terminate unhealthy sessions.
- The design must ensure one side cannot force the other into unbounded buffering by sending more data than was requested.

## Segregation and Deployment Constraints

- Local-only transports such as shared memory are not sufficient for the general design because the protocol must also support remote connections.
- If multiple transport implementations are allowed, the security properties of each must be documented separately.
- Any local optimization such as `memfd` or shared-memory-style transfer must be evaluated against accidental mutation, crash risk, and cleanup behavior.
- File-descriptor-based buffer handoff is limited to Unix-domain sockets and must fall back to inline buffers when it is unavailable.

## Legislative and Standards Alignment

- The design should align with the existing company expectation that remote communication uses standard, reviewable transport-security mechanisms rather than custom cryptography.
- Security controls should be chosen so they can be verified and tested consistently across supported runtime languages.

# How Other Databases Solve That Problem

TODO: Link the technology overview, details and selection reasoning

# Solution Approaches

At a high level, the existing material suggests at least two families of approaches:

- use a higher-level framework that already supports RPC with streaming, such as Arrow Flight
- implement the protocol on a lower-level transport such as sockets or a messaging library

The initial implementation binds the protocol to Unix-domain stream sockets. The framed protocol remains transport
independent so a future TCP/TLS binding can carry the same byte stream. Unix-only file-descriptor handoff remains
an optional local optimization rather than a requirement for all bindings.

# Proposition For Exasol

## Design

Exasol should introduce a new UDF protocol with the following high-level properties:

- `DB` acts as the client and `UDFRunner` acts as the server.
- The protocol supports `Run`, `Function`, and `Script` call types.
- The protocol supports multiple logical streams with partial ordering across streams and total ordering within each stream.
- Table transfer is explicitly flow controlled, with `Next(...)`-style requests authorizing batch transfer.
- The protocol supports callback interactions from `UDFRunner` to `DB`, including connection lookup, script lookup, and query execution.
- The protocol includes `ServerCapabilities`, framing, stream identifiers, and keepalive behavior.

The first implementation uses Unix-domain stream sockets. A future TCP/TLS binding uses the same framing and must
provide authenticated, encrypted peer communication. `Inline` buffers work on every binding; Unix-domain sockets
may later add file-descriptor transfer for `memfd` buffers or GPU-memory handles.

## Security risks

The main security risks introduced or highlighted by this design are:

- exposing a remote protocol surface between `DB` and `UDFRunner`
- transporting sensitive metadata and possibly credentials across that surface
- handling untrusted or malformed batch metadata and buffer references
- deadlock or denial-of-service risk from incorrect flow control or blocked send paths
- resource leakage or cleanup failures for buffer transports such as file-descriptor-backed mechanisms

These risks are manageable only if authentication, encryption, validation, flow control, and health-check behavior are part of the protocol design rather than optional implementation details.

## Design Decisions & Limitations

Current draft decisions:

- use the new protocol draft as a transport-agnostic design artifact first
- bind the first implementation to Unix-domain stream sockets
- preserve TCP/TLS as a future transport binding with inline buffer transfer only
- reserve Unix-domain file-descriptor handoff for optional `memfd` and GPU-memory optimizations
- treat remote security as mandatory, not optional
- treat extensibility of call/callback metadata as a first-class requirement

Current draft limitations:

- no final metadata serialization format has been selected for all message classes
- handling of very large values is acknowledged as a requirement but not yet fully designed
- no capability negotiation or descriptor-to-batch correlation is defined for optional Unix FD handoff

## Open Questions

- Which metadata formats should be used for protocol metadata, call metadata, and callback metadata?
- How should very large values be accessed safely if they do not fit well into the normal batch-transfer path?
- Which local-only optimizations, if any, are worth supporting in addition to a remote-safe baseline?
- How should Unix file descriptors be correlated with batch metadata, validated, owned, and released?
- Which GPU-memory handle types, if any, are safe to support through Unix descriptor passing?

## Technical Design

The current technical direction from the repo material is:

- a `ServerCapabilities` startup message and framed message protocol
- Unix-domain stream sockets as the initial byte-stream binding, with a future TCP/TLS binding using the same framing
- a connection-scoped `stream_id` model where `0` is the per-connection control stream, odd/even ownership is assigned to the generic Client and Server roles, and IDs are not reused per connection
- composite `StreamMessage` fields so related control data, such as a close and `Error`, can share one frame
- unilateral `CloseCall` and two-way `CloseConnection` termination, with `Error` indicating abnormal closure
- explicit control messages for metadata, start/call behavior, `Next`, data, callbacks, and heartbeat
- table transfer based on Arrow-compatible batch metadata plus inline buffers on all bindings; Unix FD handoff is optional

Detailed technical design is still required for optional Unix FD handoff and final message-format decisions.

## Documentation Input

Likely documentation targets:

- developer-facing protocol documentation for UDF runtime implementers
- architecture/operation documentation for remote UDF deployments
- security documentation for authentication, encryption, and trust assumptions
- internal implementation notes for stream control, callbacks, and failure handling

## Checklist: Changed Behavior & Side Effects

_**Always review all entries in the list**, if you reviewed check the first checkbox and if it applies, check the second checkbox and describe how it applies._
_Feel free to add new rows, if there is any side effect to be considered._

| **Item** | **Does not apply** | **Applies (please describe how)** |
| --- | --- | --- |
| **Changed behavior** |  | Yes. This is a protocol redesign for UDF execution and changes communication behavior between `DB` and `UDFRunner`. |
| Does this have a **medium or high risk** of introducing new bugs? (e.g. when it touches critical code, or many existing code paths) |  | Yes. It affects core execution, streaming, callback, and remote-communication behavior. |
| Does this introduce **security risks**? If so make them explicit, explain the reasoning, mitigation and further plans. |  | Yes. New/changed remote communication, credential handling, and untrusted payload processing require explicit security design and validation. |
| Potentially affects security (e.g. new or changed public interface, communication channels, new/changed protocols, etc.)? If yes, consider in the design and test plan.  |  | Yes. This is a new/changed protocol surface. |
| Profiling/Auditing changes/additions needed? (new query elements/combinations => new tests recommended) |  | Possibly. Protocol events, callbacks, and failure/timeout behavior may need observability updates. |
| Any other side effects or anything special about this feature? |  | Performance, deadlock behavior, multi-stream scheduling, and remote deployment characteristics all change. |
| 3rd party components were updated or introduced? |  | Flatbuffer and Arrow |

## Test Plan

High-level verification required for the final design and implementation:

| **Test Short Description** | **Assignee** |
| --- | --- |
| Validate protocol correctness for `Run`, `Function`, and `Script`, including callbacks and stream ordering. | TODO |
| Verify remote-security behavior: authentication, encryption, malformed-input handling, and unauthorized access rejection. | TODO |
| Benchmark small-batch and large-batch performance against the current protocol, including deadlock/resource-stress scenarios. | TODO |

## Possible problems caused by the new code

| **Possible Problems** | **Symptoms** | **How to Identify** | **Mitigation** |
| --- | --- | --- | --- |
| Deadlock or stalled communication | Hung UDF execution, blocked send/receive loops, timeout-triggered termination | Inspect protocol logs, thread state, and keepalive/flow-control traces | Prioritize receive on `UDFRunner`, prioritize callback handling on `DB`, add timeout and health-check coverage |
| Malformed or malicious batch metadata / buffer references | Crashes, rejected requests, memory-safety issues, corrupted results | Validate protocol input paths, fuzz metadata parsing, monitor error patterns | Strict validation, bounds checks, defensive parsing, reject invalid payload references |
| Poor small-batch performance despite redesign | No measurable improvement or worse latency than current protocol | Benchmark scalar and small-batch workloads against baseline | Revisit batching strategy, framing, callback round trips, and transport selection |
| Resource leakage in out-of-band buffer transport | File-descriptor growth, memory pressure, cleanup failures | Observe FD counts, memory usage, and long-running worker behavior | Constrain handle lifetime, add cleanup guarantees, prefer safer transport defaults |
