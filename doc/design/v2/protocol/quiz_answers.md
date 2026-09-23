# UDF Protocol v2 Quiz: Answers

These answers correspond to [quiz_questions.md](quiz_questions.md). The linked documents are the authoritative
protocol sources.

## Part A: Protocol foundations

1. `Server` accepts the transport connection and `Client` initiates it. These roles define connection establishment
   and stream ownership only; they do not restrict which side may open a call. See [low-level protocol](low_level/protocol.md#roles).

2. The transport frame provides length framing, the control message carries control attributes, and record-batch
   metadata describes one batch whose raw buffers follow separately for inline transport. See [message layering](low_level/protocol.md#message-layering).

3. It contains the byte length of the serialized FlatBuffer `Frame` as a 4-byte little-endian `uint32`. It excludes
   the prefix itself and any trailing inline buffer bytes. See [message layering](low_level/protocol.md#message-layering).

4. The supported serialized `Frame` limit is 2 GiB, matching the documented standard FlatBuffers builder limit. See
   [message layering](low_level/protocol.md#message-layering).

5. `stream_id = 0` is the control stream. Both endpoints may use it for control traffic. See [control stream](low_level/protocol.md#control-stream).

6. It identifies one logical stream on one connection. The same numeric stream ID on another connection is a
   different stream. See [stream ownership](low_level/protocol.md#stream-ownership).

## Part B: Connection, streams, and shutdown

7. Odd IDs belong to the `Client`, even IDs belong to the `Server`, and stream `0` is the shared control stream. See
   [stream ownership](low_level/protocol.md#stream-ownership).

8. No. A stream ID is unique for the lifetime of its connection and is not reused after the logical stream closes.
   See [stream ownership](low_level/protocol.md#stream-ownership).

9. The `Server` sends it during initialization on control stream `0`. See [server capabilities](low_level/protocol.md#servercapabilities).

10. It advertises protocol version, native endianness for data buffers outside the FlatBuffers frame, and the positive
    number of supported workers. See [server capabilities](low_level/protocol.md#servercapabilities).

11. FlatBuffers frame metadata is always little-endian. The advertised endianness describes the server's native
    endianness for data buffers outside the FlatBuffers frame. See [server capabilities](low_level/protocol.md#servercapabilities).

12. Yes. `Payloads(...)` may accompany `ServerCapabilities` in the same `ControlMessage` and frame, allowing a
    high-level protocol to add `high_level_name` and `high_level_version`. See [control stream](low_level/protocol.md#control-stream)
    and [protocol identification](high_level/payloads.md#protocol-identification).

13. The receiver replies with `CloseConnection`, optionally with its own `Error`, and then closes the transport. The
    initiator closes the transport after receiving the reply. See [close semantics](low_level/protocol.md#close-semantics).

14. Each peer treats the received `CloseConnection` as the reply and sends no additional close message. See [close semantics](low_level/protocol.md#close-semantics).

15. Neither peer opens a stream or sends ordinary call, data, or control traffic. Active non-control streams end when
    the transport closes. See [close semantics](low_level/protocol.md#close-semantics).

16. No. It is a non-terminal diagnostic for the enclosing stream; processing may continue if the error is recoverable.
    See [close semantics](low_level/protocol.md#close-semantics).

17. No. Sending or receiving `CloseCall` terminates local participation; the sender does not send an acknowledgement
    or any further call-scoped message. See [close semantics](low_level/protocol.md#close-semantics).

18. It must ignore the late message while continuing to process a received `CloseCall` if applicable. See [close semantics](low_level/protocol.md#close-semantics).

## Part C: Calls and nested calls

19. A call is opened with an open message, remains active for call-scoped traffic and optional nested calls, and ends
    with a close message. No dedicated accept frame is required; receiving the first call traffic makes a locally
    opened call active. See [call abstraction](low_level/call_lifecycle.md#call-abstraction) and [state models](low_level/call_lifecycle.md#endpoint-state-models).

20. A locally opened call sends `CallOpen` and waits for first peer traffic. A peer-opened call becomes active when
    the local endpoint receives `CallOpen`. See [endpoint state models](low_level/call_lifecycle.md#endpoint-state-models).

21. At most one bidirectional data stream. See [call abstraction](low_level/call_lifecycle.md#call-abstraction).

22. It remains active while the nested call executes. Nested calls use their own connection-scoped stream ID. See
    [nested calls](low_level/call_lifecycle.md#nested-calls).

23. `DB` opens `Run`, Function operations, and `cleanup`. `UDFRunner` opens nested `get_connection` and `get_script`
    calls. See
    [high-level call families](high_level/calls.md#call-families).

24. `Run` has a bidirectional data stream. Function operations and `cleanup` have no attached data stream in the
    current model. See [call families](high_level/calls.md#call-families).

25. Only while handling an active `Run` or Function call. See [nested calls](high_level/calls.md#nested-calls).

## Part D: Data streams and flow control

26. Each endpoint controls its own outbound direction: `Client` controls Client-to-Server output and `Server` controls
    Server-to-Client output. See [data-stream model](low_level/data_stream.md#model).

27. No. Each direction announces and reuses its own schema; the two directions may differ. See [schema behavior](low_level/data_stream.md#schema-and-first-batch-behavior).

28. Before the first batch, or in the same `Frame` as the first batch. See [schema behavior](low_level/data_stream.md#schema-and-first-batch-behavior).

29. It is a receiver-to-sender flow-control hint for preferred batch byte size and optional seeking. It does not select
    a batch, pause, cancel, or otherwise directly control the sender. See [`Next`](low_level/data_stream.md#nextbyte_budget-reset-row_id).

30. Only the first batch in a direction. Later batches require usable budget from a previously received `Next(...)`.
    See [data-stream rules](low_level/data_stream.md#rules).

31. Yes. A sender may send less than the hint, and an indivisible batch may exceed the remaining budget. See
    [data-stream rules](low_level/data_stream.md#rules).

32. `reset=true` requests a seek for batches not already in flight; `row_id` identifies the position from which the
    sender resumes. See [`Next`](low_level/data_stream.md#nextbyte_budget-reset-row_id).

33. With `reset=false`, `row_id` is ignored. If omitted, FlatBuffers supplies the scalar schema default `0` through
    the generated accessor, and default-valued scalar fields may be omitted on the wire. See [`Next`](low_level/data_stream.md#nextbyte_budget-reset-row_id)
    and the [FlatBuffers schema defaults documentation](https://flatbuffers.dev/schema/).

34. No. In-flight batches continue unaffected and may refer to different positions. See [data-stream rules](low_level/data_stream.md#rules).

35. Exactly one per direction. Later batches reuse the previously announced schema. See [schema behavior](low_level/data_stream.md#schema-and-first-batch-behavior).

36. `is_end_of_group` identifies whether the trailing group in a batch is complete. It is not an end-of-stream marker;
    generic stream completion is a separate low-level open question. See [group and row semantics](high_level/correlation.md#group-and-row-id-semantics).

37. `Inline` is supported by every transport binding and is the only permitted mode on TCP/TLS. See [buffer transport](low_level/data_stream.md#buffer-transfer).

38. No. The length covers only the serialized `Frame`. Inline buffers follow immediately in `DataRecordBatchMetadata.buffers`
    order, and each `Buffer.length` specifies the exact number of bytes. See [buffer transport](low_level/data_stream.md#buffer-transfer).

## Part E: Metadata, correlation, and types

39. Metadata may be sent on control stream `0` before a call, between calls, or with `OpenCall`; it must not be sent
    during an active call. See [metadata lifecycle](high_level/payloads.md#metadata-lifecycle).

40. The latest received instance replaces the previous instance of the same metadata type or named value. See
    [metadata lifecycle](high_level/payloads.md#metadata-lifecycle).

41. `high_level_name = exasol.udf` and `high_level_version = 2.0-dev`, both as `StringPayload` values. See
    [protocol identification](high_level/payloads.md#protocol-identification).

42. No. The current `call_metadata` schema no longer rejects either field. Column definitions remain described by the
    separate `column_metadata` contract. See [call metadata](high_level/payloads.md#call-metadata) and the [call metadata schema](../../../../udf-runner-cpp/v2/json_schema/call_metadata.schema.json).

43. The first field is the group ID and the second is the row ID; user data starts at position 2. See [correlation columns](high_level/correlation.md#correlation-columns).

44. Positions. The first and second fields are reserved according to the flags, regardless of field names. See [correlation columns](high_level/correlation.md#correlation-columns).

45. It references the inbound `(group_id, row_id)` pair from `DB` to `UDFRunner`; it is not a newly allocated output
    identifier. This applies to both `RETURNS` and `EMITS`. See [group and row semantics](high_level/correlation.md#group-and-row-id-semantics).

46. It means no later batch contains the trailing group. It does not mean the stream has ended. An empty batch does not
    complete a group. See [group and row semantics](high_level/correlation.md#group-and-row-id-semantics).

47. The preferred encoding is the `exasol.udf.range_run` extension array; plain unsigned 64-bit is the compatible
    fallback. See [preferred encodings](high_level/correlation.md#preferred-encodings).

48. They are decimal strings so JSON implementations do not lose precision. See [payload contracts](high_level/payloads.md#call-metadata).

## Part F: Architecture-review scenarios

49. No. It should ignore the late batch because the batch was received after local participation on that stream ended.
    See [close semantics](low_level/protocol.md#close-semantics).

50. No. It must wait for another usable `Next(...)` budget, except for the direction's first batch. See [data-stream rules](low_level/data_stream.md#rules).

51. No. The in-flight batch is unaffected; the reset applies only to batches not already in flight. See [`Next`](low_level/data_stream.md#nextbyte_budget-reset-row_id).

52. No. An empty batch does not complete a group. See [group and row semantics](high_level/correlation.md#group-and-row-id-semantics).

53. No. The error is non-terminal by itself; the peer may continue if the error is recoverable. See [close semantics](low_level/protocol.md#close-semantics).

54. No. The high-level values can be sent as `Payloads(...)` alongside `ServerCapabilities` in the same control
    message and frame. See [control stream](low_level/protocol.md#control-stream).

55. No. `Memfd` and `OutOfBand` are reserved for future Unix-domain-socket modes; TCP/TLS must use `Inline`. See
    [buffer transport](low_level/data_stream.md#buffer-transfer).

56. Examples include exact `reset` semantics beyond the current seek-hint rule, generic stream-completion semantics,
    future `ExecuteScript`/`execute_query` calls, possible pquery-style calls, and future table-prefetch declarations.
    These are listed in [data-stream open questions](low_level/data_stream.md#open-questions), [high-level call open questions](high_level/calls.md#forward-looking-ideas-still-open),
    and [deferred calls](high_level/payloads.md#deferred-calls).

57. It gives the UDFRunner an explicit opportunity to release resources it still retains from completed calls or
    nested calls. See [the cleanup call](high_level/calls.md#cleanup).

58. `DB` may issue it between calls after one or more preceding calls have completed. It may repeat the call after a
    later group of calls. See [the cleanup call](high_level/calls.md#cleanup).

59. No. `cleanup` has no attached data stream, request payload, or normal result payload. See [the cleanup payload contract](high_level/payloads.md#cleanup-call).

60. The UDFRunner sends normal `CloseCall` only after cleanup has completed. It reports failure with `CloseCall` carrying
    `Error`. See [the cleanup payload contract](high_level/payloads.md#cleanup-call).

61. Yes. A normally closed `cleanup` call leaves the connection available for later calls. See [the cleanup payload contract](high_level/payloads.md#cleanup-call).
