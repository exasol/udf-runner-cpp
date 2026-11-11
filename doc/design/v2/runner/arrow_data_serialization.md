# Specification: Arrow Schema and Record-Batch Serialization

## Purpose

This specification defines an injectable codec for converting Arrow schemas and record batches to and from protocol metadata and Arrow buffers, including codec-specific ownership and validation. Protocol schema layout, framing, buffer transport, and stream rules are defined in [the low-level protocol](../protocol/low_level/protocol.md) and [data-stream specification](../protocol/low_level/data_stream.md). Runner queueing, frame composition, flow control, batching, and socket lifecycle are defined in [context implementation](context_implementation.md); worker-facing message and ownership contracts are defined in [context interface](context_interface.md).

## Placement in the architecture

The codec is an internal dependency of the concrete background thread, as described in [context implementation](context_implementation.md). The background thread invokes it for both outbound serialization and inbound deserialization. The codec owns Arrow conversion; the background thread owns transport state. Injection provides a test seam for codec behavior.

## Codec interface

```cpp
class ArrowDataCodec
{
public:
    virtual ~ArrowDataCodec() = default;

    virtual Result<SerializedSchema> serialize_schema(
        const ArrowSchema& schema,
        SchemaOptions options) = 0;

    virtual Result<SerializedBatch> serialize_batch(
        ArrowArrayPtr&& array,
        BatchOptions options) = 0;

    virtual Result<DecodedSchema> deserialize_schema(
        const VerifiedDataSchema& metadata) = 0;

    virtual Result<DecodedBatch> deserialize_batch(
        const VerifiedRecordBatchMetadata& metadata,
        BufferReader& buffers,
        const DecodedSchema& schema) = 0;
};
```

`VerifiedDataSchema` and `VerifiedRecordBatchMetadata` are conceptual internal views produced by the verified-frame dispatcher; the codec does not re-verify or route complete frames.

The codec options carry only Arrow-specific inputs. `SchemaOptions` contains `has_group_id` and `has_row_id`; `BatchOptions` contains the matching Arrow schema, `has_group_id`, and `is_end_of_group`. A production codec is configured with `CodecLimits` from runner configuration:

```cpp
struct CodecLimits
{
    std::size_t max_buffer_bytes;
    std::size_t max_batch_bytes;
};
```

`DecodedSchema` owns the imported `ArrowSchemaPtr`, both correlation flags, and the flattened layout information needed to validate batches. Conceptually, `BatchOptions` contains `const ArrowSchema& schema`, `bool has_group_id`, and `bool is_end_of_group`; the exact C++ representation is an implementation choice.

The codec serializes one physical batch. Logical-batch splitting and ordering remain the background-thread responsibilities defined in [context implementation](context_implementation.md#background-send-path).

## Serialized representations

These illustrative result shapes show ownership and buffer relationships; their exact C++ types are implementation choices.

```cpp
struct SerializedSchema
{
    ProtocolDataSchema metadata;
};

struct SerializedBatch
{
    ProtocolDataRecordBatchMetadata metadata;
    std::vector<BufferSegment> buffers;

    // Keeps Arrow-owned memory alive until all buffers are written.
    ArrowArrayPtr ownership;
};

struct BufferSegment
{
    const std::byte* data;
    std::size_t size;
};
```

These are codec results for the outer message composer. The protocol metadata values own their fields; the composer embeds them in the appropriate composite protocol frame and emits batch buffer spans in metadata order. `ownership` remains alive until those spans are fully sent or discarded during shutdown.

## Schema serialization

The codec validates that every field uses a supported layout, then converts the Arrow schema to protocol `DataSchema` metadata while preserving the layout and annotations required by [the data-stream schema rules](../protocol/low_level/data_stream.md#schema-and-first-batch-behavior). The returned metadata owns its fields and is ready for the message composer; the codec does not decide whether it shares a frame with the first batch or other call fields.

```text
serialize_schema(schema, options):
    validate schema structure and supported layouts
    convert fields recursively in Arrow order to owned protocol metadata
    preserve type parameters, nullability, extension metadata, and correlation flags as supplied
    return SerializedSchema{metadata}
```

The schema-prefix and flag rules are defined in [the data-stream specification](../protocol/low_level/data_stream.md#schema-and-first-batch-behavior).

## Batch serialization

The codec validates one physical Arrow array and builds its record-batch metadata and ordered buffer spans. The required field-node order, buffer order, and variadic-buffer semantics are defined in [the data-stream buffer-transfer rules](../protocol/low_level/data_stream.md#buffer-transfer). The initial layout support and rejection behavior are defined below.

Illustrative codec pseudocode:

```text
serialize_batch(array, options):
    if array is null:
        return InvalidInput

    if options.schema contains an unsupported layout:
        return UnsupportedArrowLayout

    validate array types, top-level length, child structure, and buffer layout against schema
    validate is_end_of_group against schema.has_group_id and array.length

    nodes = []
    buffers = []
    variable_buffer_counts = []

    visit_field(
        field = options.schema.root,
        array = array,
        nodes = nodes,
        buffers = buffers,
        variable_buffer_counts = variable_buffer_counts)

    metadata = FlatBufferBuilder.create_data_record_batch_metadata(
        length = array.length,
        nodes = nodes,
        buffers = describe(buffers),
        variadic_buffer_counts = variable_buffer_counts,
        buffer_transport = Inline,
        is_end_of_group = options.is_end_of_group)

    return SerializedBatch{
        metadata = move(metadata),
        buffers = buffers,
        ownership = move(array)
    }
```

The serializer does not copy buffer contents merely to create metadata; its spans reference Arrow-owned memory. The frame layout, inline-buffer order, and partial-write behavior are defined in [the low-level protocol](../protocol/low_level/protocol.md#message-layering) and [context implementation](context_implementation.md#background-send-path).

## Serialization ownership

The `ArrowArrayPtr&&` parameter is a reference to the caller's pointer; binding it does not itself move from the
pointer. The implementation must keep the caller's pointer intact while validating the array and constructing the
serialized metadata. It moves the pointer into `SerializedBatch::ownership` only after every serialization step has
succeeded. Thus, a failed `Result` leaves the caller's pointer non-null and owning the original array:

```text
result = codec.serialize_batch(move(array))

if result.success:
    background thread owns SerializedBatch
else:
    caller receives the original ArrowArrayPtr
```

Message validation and queue-acceptance ownership remain the [context interface's responsibility](context_interface.md#builder-and-view-implementation-contract).

## Deserialization

The frame decoder and message dispatcher own framing, FlatBuffer verification, stream routing, and composite-message handling as described in [the low-level protocol](../protocol/low_level/protocol.md#message-layering) and [context implementation](context_implementation.md#builder-wire-message-and-view-handoff). They pass verified Arrow metadata to the codec, which performs only the conversion below.

Schema deserialization:

```text
deserialize_schema(verified_schema_metadata):
    validate field and child structure
    create decoded schema state
    return schema state
```

Batch deserialization pseudocode:

```text
deserialize_batch(metadata, reader, schema):

    if metadata.nodes.count != schema.flattened_field_count():
        return SchemaMismatch

    if metadata.buffer_transport != Inline:
        return BufferTransportMismatch

    if schema contains an unsupported layout:
        return UnsupportedArrowLayout

    validate is_end_of_group against schema.has_group_id and metadata.length

    validate flattened node order and data types against the schema
    require every node length to be nonnegative and every null count to be in [0, node.length]
    validate child-array lengths and layout invariants
    for RunEndEncoded fields, require equal run_ends/values child lengths,
        strictly increasing positive run ends; if the parent is nonempty, the final run end equals parent length

    expected_buffers = calculate_buffer_count(schema, metadata)
    if metadata.buffers.count != expected_buffers:
        return BufferCountMismatch

    total_bytes = 0

    for descriptor in metadata.buffers:
        if descriptor.length != expected_length_for_layout(descriptor, metadata, schema):
            return BufferLengthInvalid
        if descriptor.length exceeds limits.max_buffer_bytes:
            return FrameTooLarge

        if checked_add(total_bytes, descriptor.length) overflows:
            return FrameTooLarge
        total_bytes += descriptor.length

        if total_bytes exceeds limits.max_batch_bytes:
            return FrameTooLarge

    storage = allocate_batch_storage(metadata.buffers, total_bytes)

    for descriptor in metadata.buffers:
        bytes = reader.read_exact(descriptor.length)
        if bytes.length != descriptor.length:
            return TruncatedBufferData

        storage.append_copy(bytes)

    array = import_arrow_array(
        schema = schema,
        nodes = metadata.nodes,
        buffers = storage)

    return DecodedBatch{
        array = move(array),
        storage = move(storage),
        is_end_of_group = metadata.is_end_of_group
    }
```

The batch is not published to the inbound queue until the outer frame has been verified by the dispatcher, all buffer bytes are received, and the Arrow C Data object is valid.

## Supported Arrow layouts

The layouts follow the [Arrow columnar format specification](https://arrow.apache.org/docs/format/Columnar.html). The initial codec supports this subset of Arrow C Data layouts:

| Layout | Buffer and child structure |
| --- | --- |
| Null | No data buffers or children |
| Boolean | Optional validity bitmap and value bitmap |
| Fixed-width primitive, including numeric, temporal, interval, decimal, and fixed-size binary | Optional validity bitmap and values buffer |
| Binary and UTF-8, including large variants | Optional validity bitmap, offsets buffer, and values buffer |
| Binary and UTF-8 view | Optional validity bitmap, view buffer, and the advertised variadic data buffers |
| Struct | Optional validity bitmap and one child array per field |
| List and large list | Optional validity bitmap, offsets buffer, and one child array |
| Fixed-size list | Optional validity bitmap and one child array |
| Map | List layout with its entries child represented as a struct |
| Run-end encoded | No parent buffers; two child arrays in `run_ends`, `values` order |

Nested supported layouts are handled recursively. Run-end encoded arrays require a signed 16-, 32-, or 64-bit `run_ends` child and a supported `values` child. The `exasol.udf.range_run` extension uses its specified signed 64-bit run ends and unsigned 64-bit values, as defined in [the range-run contract](../protocol/high_level/range_run.md#storage-type). Other extension types are supported only when their storage layout is in this table; extension names and metadata are preserved in the schema. Dictionary encoded arrays, unions, list-view layouts, and any other layout not listed here return `UnsupportedArrowLayout`. The serializer rejects these before producing metadata; the deserializer rejects them before allocating buffer storage or publishing a batch.

The codec validates group-boundary metadata using the [call interface's data-stream rules](context_interface.md#arrow-schemas-and-record-batches). The serializer computes exact node null counts; the deserializer rejects null counts outside `[0, node.length]`.

## Decoded buffer lifetime

`DecodedBatch` owns its `BufferStorage`, and the `ArrowArrayPtr` release state retains that storage for as long as any exported buffer pointer may be used. The release callback frees the array and codec storage exactly once. Transfer from the inbound queue to the worker-facing view follows the ownership rules in [context interface](context_interface.md#view-lifetime-and-verification).

## Buffer reader abstraction

The deserializer must not read directly from the socket. The initial receive implementation copies each returned
span into `BufferStorage` before requesting another span. `read_exact` returns a borrowed span valid until the next
call to the reader; the span must not be retained by the decoded array. This makes the decoded array's buffer pointers
refer only to storage owned by `DecodedBatch`, independently of socket or receive-buffer reuse.

```cpp
class BufferReader
{
public:
    virtual ~BufferReader() = default;

    virtual Result<std::span<const std::byte>> read_exact(
        std::size_t byte_count) = 0;
};
```

The initial implementation supports `Inline` only. The sender sets `buffer_transport = Inline`; the receiver rejects `Memfd`, `OutOfBand`, or any unknown value with `BufferTransportMismatch` before reading buffer bytes. The protocol defines reserved modes in [data-stream buffer transfer](../protocol/low_level/data_stream.md#buffer-transfer). The production reader is supplied by the background thread; tests can provide a memory-backed reader.

`read_exact(n)` succeeds only with a span of exactly `n` bytes, valid until the next reader call. It returns an I/O error on transport failure and `TruncatedBufferData` if the stream ends before `n` bytes arrive. The reader consumes the descriptor lengths in order and must retain any bytes read ahead for the next frame prefix. After the sum of the advertised lengths is consumed, frame decoding resumes at that prefix.

## Integration boundary

Frame composition, call-field combinations, flow control, logical-batch splitting, stream routing, and queue publication remain defined in [context implementation](context_implementation.md#builder-wire-message-and-view-handoff) and [context interface](context_interface.md#composite-call-messages). This table shows where the codec participates in each Arrow-bearing path:

| Message path | Outbound codec work | Inbound codec work |
| --- | --- | --- |
| Schema only | Serialize `DataSchema` metadata; composer emits no batch buffers | Deserialize schema metadata and update that direction's schema state |
| Schema with first batch | Serialize both components; composer puts them in one frame and emits batch buffers | Decode schema before the batch, then consume that batch's advertised buffers |
| Batch after schema announcement | Serialize batch metadata and buffers using the established schema | Decode batch using the established schema and consume its advertised buffers |
| Message with no Arrow fields, including control-stream traffic | Bypass the Arrow codec | Bypass the Arrow codec |

The composer preserves any other permitted fields in a composite message. A batch without an accompanying or previously announced schema is invalid under the [data-stream rules](../protocol/low_level/data_stream.md#schema-and-first-batch-behavior). For a logical batch split into physical frames, the schema accompanies only its first physical frame. The dispatcher publishes an inbound composite message only after every Arrow component has decoded successfully and all inline buffer bytes have been read.

## Error categories

The configured limits are a maximum individual buffer size and maximum aggregate batch buffer size. Both are checked with overflow-safe arithmetic before storage allocation. Allocation failure returns `AllocationFailure`. Invalid peer metadata and unsupported transports are returned to the background thread, which handles them as protocol errors under [the context implementation contract](context_implementation.md#receive-side-socket-processing). After outer frame verification, the codec should distinguish:

```text
InvalidArrowInput
InvalidSchema
SchemaMismatch
BufferCountMismatch
BufferLengthInvalid
BufferTransportMismatch
FrameTooLarge
TruncatedBufferData
AllocationFailure
UnsupportedArrowLayout
```

Local serialization errors are returned through the existing send contract.



