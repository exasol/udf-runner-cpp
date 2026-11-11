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

    virtual Result<FlatBufferOffset<protocol::DataSchema>> serialize_schema(
        const arrow::Schema& schema,
        bool has_group_id,
        bool has_row_id,
        FlatBufferBuilder& builder) = 0;

    virtual Result<SerializedBatch> serialize_batch(
        std::shared_ptr<const arrow::RecordBatch> batch,
        bool has_group_id,
        bool is_end_of_group,
        FlatBufferBuilder& builder) = 0;

    virtual Result<std::unique_ptr<interface::DataSchemaView>> deserialize_schema(
        const protocol::DecodedFrame& frame) = 0;

    virtual Result<std::unique_ptr<interface::RecordBatchView>> deserialize_batch(
        const protocol::DecodedFrame& frame,
        BufferReader& buffers,
        const arrow::Schema& schema,
        bool has_group_id) = 0;
};
```

Conceptually, `FlatBufferBuilder` aliases `third_party::flatbuffers::FlatBufferBuilder` and `FlatBufferOffset<T>` aliases `third_party::flatbuffers::Offset<T>`. The message composer passes its builder to the codec so the schema and batch metadata offsets can be included in the same composite frame. `DecodedFrame` owns a verified frame; the codec extracts the Arrow metadata from it and does not re-verify or route the frame.

The background thread imports accepted Arrow C Data objects into normal Arrow C++ objects before outbound codec calls. On receive, the codec converts the verified frame metadata and inline buffers into Arrow C++ objects, then exports them into the worker-facing C Data views. These C++ and FlatBuffers types remain internal and do not cross the public API boundary. A production codec is configured with `CodecLimits` from runner configuration:

```cpp
struct CodecLimits
{
    std::size_t max_buffer_bytes;
    std::size_t max_batch_bytes;
};
```

The codec serializes one physical batch. Logical-batch splitting and ordering remain the background-thread responsibilities defined in [context implementation](context_implementation.md#background-send-path).

## Serialized representations

These illustrative result shapes show ownership and buffer relationships; their exact C++ types are implementation choices.

```cpp
struct SerializedBatch
{
    FlatBufferOffset<protocol::DataRecordBatchMetadata> metadata;
    std::vector<BufferSegment> buffers;

    // Keeps Arrow-owned buffers alive until all spans are written.
    std::shared_ptr<const arrow::RecordBatch> ownership;
};

struct BufferSegment
{
    const std::byte* data;
    std::size_t size;
};
```

`serialize_schema` returns a builder-owned offset directly. `SerializedBatch` adds the ordered non-owning buffer spans and retains the Arrow `RecordBatch` that owns their storage. The outer composer embeds the returned offsets in the composite protocol frame; the batch owner remains alive until every span is sent or discarded during shutdown.

## Schema serialization

The codec validates that every field uses a supported layout, then converts the Arrow schema to protocol `DataSchema` metadata in the caller's `FlatBufferBuilder`, preserving the layout and annotations required by [the data-stream schema rules](../protocol/low_level/data_stream.md#schema-and-first-batch-behavior). It returns the metadata offset; the composer decides whether it shares a frame with the first batch or other call fields.

```text
serialize_schema(schema, has_group_id, has_row_id, builder):
    validate schema structure and supported layouts
    convert fields recursively in Arrow order using builder
    preserve type parameters, nullability, extension metadata, and correlation flags as supplied
    return CreateDataSchema(builder, schema_offset, has_group_id, has_row_id)
```

The schema-prefix and flag rules are defined in [the data-stream specification](../protocol/low_level/data_stream.md#schema-and-first-batch-behavior).

## Batch serialization

The codec validates one physical Arrow `RecordBatch` and builds its record-batch metadata and ordered buffer spans. The required field-node order, buffer order, and variadic-buffer semantics are defined in [the data-stream buffer-transfer rules](../protocol/low_level/data_stream.md#buffer-transfer). The initial layout support and rejection behavior are defined below.

Illustrative codec pseudocode:

```text
serialize_batch(batch, has_group_id, is_end_of_group, builder):
    if batch is null:
        return InvalidInput

    if batch.schema contains an unsupported layout:
        return UnsupportedArrowLayout

    validate batch arrays against batch.schema
    validate is_end_of_group against has_group_id and batch.num_rows

    nodes = []
    buffers = []
    variable_buffer_counts = []

    visit_batch_columns(
        batch = batch,
        nodes = nodes,
        buffers = buffers,
        variable_buffer_counts = variable_buffer_counts)

    metadata = create_data_record_batch_metadata(
        builder = builder,
        length = batch.num_rows,
        nodes = nodes,
        buffers = describe(buffers),
        variadic_buffer_counts = variable_buffer_counts,
        buffer_transport = Inline,
        is_end_of_group = is_end_of_group)

    return SerializedBatch{
        metadata = metadata,
        buffers = buffers,
        ownership = move(batch)
    }
```

The serializer does not copy buffer contents merely to create metadata; its spans reference Arrow-owned memory. The frame layout, inline-buffer order, and partial-write behavior are defined in [the low-level protocol](../protocol/low_level/protocol.md#message-layering) and [context implementation](context_implementation.md#background-send-path).

## Serialization ownership

The codec takes a shared Arrow `RecordBatch` owner by value. On failure, the caller retains its own shared owner; on success, `SerializedBatch` retains the batch while any returned span references its buffers:

```text
result = codec.serialize_batch(batch, has_group_id, is_end_of_group, builder)

if result.success:
    background thread owns SerializedBatch
else:
    caller retains its shared_ptr<arrow::RecordBatch>
```

Message validation and queue-acceptance ownership remain the [context interface's responsibility](context_interface.md#builder-and-view-implementation-contract).

## Deserialization

The frame decoder and message dispatcher own framing, FlatBuffer verification, stream routing, and composite-message handling as described in [the low-level protocol](../protocol/low_level/protocol.md#message-layering) and [context implementation](context_implementation.md#builder-wire-message-and-view-handoff). They pass the verified `DecodedFrame` to the codec, which performs only the Arrow conversion below.

Schema deserialization:

```text
deserialize_schema(frame):
    extract DataSchema from frame
    validate field and child structure
    import protocol schema into arrow::Schema
    export schema to Arrow C Data
    return make_unique<DataSchemaViewImpl>(exported_schema, correlation flags)
```

Batch deserialization pseudocode:

```text
deserialize_batch(frame, reader, schema, has_group_id):
    metadata = frame.frame().data_record_batch_metadata()

    if metadata.nodes.count != flattened_field_count(schema):
        return SchemaMismatch

    if metadata.buffer_transport != Inline:
        return BufferTransportMismatch

    if schema contains an unsupported layout:
        return UnsupportedArrowLayout

    validate is_end_of_group against has_group_id and metadata.length

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

    record_batch = make_arrow_record_batch(
        schema = schema,
        nodes = metadata.nodes,
        buffers = storage)

    exported_array = export_record_batch_as_arrow_c_array(record_batch)
    return make_unique<RecordBatchViewImpl>(exported_array, metadata.is_end_of_group)
```

The returned view owns an Arrow C Data array whose release state retains the `RecordBatch` and buffer storage. The batch is not published to the inbound queue until the outer frame has been verified by the dispatcher, all buffer bytes are received, and the exported Arrow C Data object is valid.

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

`RecordBatchView` owns the `ArrowArrayPtr`; its release state retains the Arrow `RecordBatch` and `BufferStorage` for as long as any exported buffer pointer may be used. The release callback frees the array and codec storage exactly once. View ownership follows [the context interface](context_interface.md#view-lifetime-and-verification).

## Buffer reader abstraction

The deserializer must not read directly from the socket. The initial receive implementation copies each returned
span into `BufferStorage` before requesting another span. `read_exact` returns a borrowed span valid until the next
call to the reader; the span must not be retained by the exported Arrow C Data array. This makes its buffer pointers
refer only to storage owned by the `RecordBatchView` release state, independently of socket or receive-buffer reuse.

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



