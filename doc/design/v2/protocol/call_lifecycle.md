# UDF Protocol v2: Generic Call Lifecycle

This document describes the generic `Call` abstraction shared by all protocol interactions. It intentionally
excludes wire-level framing details and command-specific behavior.

## Scope

This document covers:

- the generic `Call` abstraction
- normalized message labels
- generic call state transitions
- nested calls as a protocol mechanism

Related diagram:

- [call_lifecycle.svg](call_lifecycle.svg)

## Call Abstraction

A `Call` is the protocol's unit of interaction between two endpoints.

- a call is opened with an open message
- a call is closed with a close message
- named payloads or other call-scoped metadata may flow while the call is active
- a call may spawn nested calls while remaining active
- a call may have at most one bidirectional data stream attached to it

Either side may open a call. Transport-level client/server roles do not restrict call initiation.
Each call's stream is identified by `(connection, stream_id)`; the same numeric ID on another connection identifies
a different stream.

## Normalized Documentation Labels

The current docs use these normalized labels to describe call behavior without freezing final wire names:

- `CallOpen(name, metadata)`
- `first call-scoped message`
- `CallClose`, optionally with `Error`
- `Payloads(...)`

These are documentation aliases, not a second schema.

## Endpoint State Models

The state models use one local endpoint's perspective. A locally opened call and a peer-opened call have separate
opening paths; the peer runs the complementary model.

### Locally Opened Call

| State | Meaning |
| --- | --- |
| `LocallyOpenedIdle` | The local endpoint has not opened the call. |
| `WaitingForPeerCallTraffic` | The local endpoint sent `CallOpen(...)` and awaits the first call-scoped message. |
| `LocallyOpenedActive` | The call is active at the local endpoint. |
| `LocallyOpenedClosed` | The local endpoint has handled `CallClose`. |

| From | Local event | To | Notes |
| --- | --- | --- | --- |
| `LocallyOpenedIdle` | Send `CallOpen(name, metadata)` | `WaitingForPeerCallTraffic` | Either endpoint may open a call. |
| `WaitingForPeerCallTraffic` | Receive first call traffic | `LocallyOpenedActive` | No dedicated accept frame is required. |
| `LocallyOpenedActive` | Handle call traffic | `LocallyOpenedActive` | Send or receive `Payloads(...)` and nested `CallOpen(...)` traffic. |
| `LocallyOpenedActive` | Handle `CallClose` | `LocallyOpenedClosed` | Sending or receiving it closes the call locally. |

### Peer-Opened Call

| State | Meaning |
| --- | --- |
| `PeerOpenedIdle` | The local endpoint has not received a call open. |
| `PeerOpenedActive` | The call is active at the local endpoint. |
| `PeerOpenedClosed` | The local endpoint has handled `CallClose`. |

| From | Local event | To | Notes |
| --- | --- | --- | --- |
| `PeerOpenedIdle` | Receive `CallOpen(name, metadata)` | `PeerOpenedActive` | Acceptance may be explicit or inferred later. |
| `PeerOpenedActive` | Handle call traffic | `PeerOpenedActive` | Send or receive `Payloads(...)` and nested `CallOpen(...)` traffic. |
| `PeerOpenedActive` | Handle `CallClose` | `PeerOpenedClosed` | Sending or receiving it closes the call locally. |

## Nested Calls

Nested calls are a generic mechanism, not a special-case callback transport:

- the parent call stays active while nested calls execute
- nested calls use their own connection-scoped `stream_id`
- ordering and priority between a parent call and its nested calls are not fully defined beyond the scheduling policy
  documented in the high-level calls document

## Relationship To Data Streams

This document only records that a call may carry at most one bidirectional data stream. Detailed data-stream rules are
part of the low-level protocol.

## Open Questions

- precise command-selection fields carried in opening metadata
- stronger ordering guarantees between a call and the nested calls it spawns
