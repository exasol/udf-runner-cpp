# UDF Protocol v2 Quiz: Questions

This quiz is for protocol onboarding and architecture review. Answer the questions before opening the
[answer key](quiz_answers.md). References identify the relevant specification sections, not the answers.

## Part A: Protocol foundations

1. What are the `Server` and `Client` transport-level roles, and what do those roles not restrict?

2. What are the three layers represented by the transport frame, control message, and record-batch metadata?

3. What does the 4-byte frame prefix contain, in which byte order, and what does it exclude?

4. What is the supported size limit for the serialized FlatBuffers `Frame`?

5. Which stream is reserved for connection-level control traffic, and can both endpoints use it?

6. What does the pair `(connection, stream_id)` identify?

## Part B: Connection, streams, and shutdown

7. Which endpoint owns odd stream IDs and which owns even stream IDs? What is special about stream `0`?

8. May a stream ID be reused after its logical stream closes?

9. When and where does the `Server` send `ServerCapabilities`?

10. What three values does `ServerCapabilities` advertise?

11. What endianness applies to FlatBuffers metadata, and what endianness value in `ServerCapabilities` describes?

12. Can high-level protocol identification be sent together with `ServerCapabilities`? If so, how?

13. What is the required exchange when either peer sends `CloseConnection`?

14. What happens if both peers send `CloseConnection` at the same time?

15. After `CloseConnection` is sent or received, which new traffic is forbidden?

16. Is an `Error` without `CloseCall` or `CloseConnection` necessarily terminal?

17. After an endpoint sends `CloseCall`, can it send a reply or any further message on that stream?

18. What must an endpoint do with a message that was already in transit and arrives after `CloseCall`?

## Part C: Calls and nested calls

19. What is the generic lifecycle of a call, and is a dedicated call-accept frame required?

20. How do locally opened and peer-opened calls differ in their initial state transitions?

21. How many bidirectional data streams can one call have?

22. What happens to a parent call while a nested call executes?

23. Which high-level calls are opened by `DB`, and which nested calls are opened by `UDFRunner`?

24. Which high-level call carries a bidirectional data stream, and which DB-opened operations do not?

25. When may `UDFRunner` open a nested call?

## Part D: Data streams and flow control

26. Who controls each outbound direction of a bidirectional data stream?

27. Must the two directions of one data stream use the same schema?

28. When must `DataSchema` be sent relative to the first record batch?

29. What does `Next(byte_budget, reset, row_id)` control, and what does it not control?

30. Which batch may be sent without a previously received `Next(...)`?

31. May a sender send less than the requested byte budget or send an indivisible batch larger than the remaining budget?

32. What is the meaning of `reset` and `row_id` when `reset` is `true`?

33. What happens to `row_id` when `reset` is `false`, and what is the FlatBuffers default if `row_id` is omitted?

34. Do reset hints affect batches that are already in flight?

35. How many `DataSchema` messages does one direction send, and how are later batches interpreted?

36. What is the difference between a group boundary marker and an end-of-stream marker?

37. Which `buffer_transport` mode is always supported and required for TCP/TLS?

38. Does the inline frame length include trailing buffer bytes? How are inline buffers ordered and sized?

## Part E: Metadata, correlation, and types

39. When may script, call, and column metadata be sent, and when must metadata not be sent?

40. What does the latest metadata instance do to the previous instance of the same metadata type or named value?

41. What are the high-level protocol identification payload names and values for this repository?

42. Are `input_columns` and `output_columns` forbidden in `call_metadata` by the current schema?

43. When both correlation flags are set, what are the first two fields in a `Run` data schema?

44. Are correlation columns identified by field names or by their positions?

45. What does a `UDFRunner` output `(group_id, row_id)` pair refer to?

46. What does `is_end_of_group = true` mean, and is it an end-of-stream signal?

47. What is the preferred encoding for a `DB` to `UDFRunner` row ID, and what is the compatible fallback?

48. How are unsigned 64-bit values represented in JSON payload bodies?

## Part F: Architecture-review scenarios

49. `DB` sends `CloseCall` and then receives a record batch that was already in flight. Should `DB` process the batch?

50. A sender has exhausted its current byte budget but has more batches ready. May it send the next batch immediately?

51. A receiver sends `Next(reset=true, row_id=500)` while an earlier batch is already in flight. Does the earlier batch move to row 500?

52. A `Run` batch has `has_group_id=true`, `is_end_of_group=true`, and zero rows. Does it complete a group?

53. A peer sends an `Error` without a close field. Must the connection be closed immediately?

54. A high-level implementation wants to send `high_level_name` and `high_level_version` during initialization. Must the low-level FlatBuffer schema be extended?

55. A TCP implementation selects `Memfd` buffer transport. Is that valid under the current protocol?

56. Which parts of the current design should be treated as open questions rather than settled normative behavior?

57. What is the purpose of the DB-opened `cleanup` call?

58. When may `DB` issue `cleanup`, and may it be repeated?

59. Does `cleanup` have a data stream, request payload, or normal result payload?

60. When may the UDFRunner send the normal `CloseCall` for `cleanup`, and how does it report cleanup failure?

61. After a normally closed `cleanup` call, may the connection be used for another call?
