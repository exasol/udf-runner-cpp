#include "udf_protocol.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

int main() {
    {
        exasol::udf::v2::third_party::flatbuffers::FlatBufferBuilder builder;
        const auto call_name = builder.CreateString("example");
        const auto open_call = exasol::udf::protocol::CreateOpenCall(builder, call_name);
        const auto control_message = exasol::udf::protocol::CreateControlMessage(
            builder, 0, 0, open_call);
        const auto frame = exasol::udf::protocol::CreateFrame(
            builder, 7, control_message);
        builder.Finish(frame);

        assert(exasol::udf::protocol::VerifyFrameBuffer(
            builder.GetBufferPointer(), builder.GetSize()));
        const auto* decoded = exasol::udf::protocol::GetFrame(builder.GetBufferPointer());
        assert(decoded->stream_id() == 7);
        assert(decoded->data_record_batch() == nullptr);
        assert(decoded->control_message()->open_call()->call_name()->str() == "example");
    }

    {
        exasol::udf::v2::third_party::flatbuffers::FlatBufferBuilder builder;
        const std::vector<exasol::udf::protocol::FieldNode> nodes_data{{3, 1}};
        const std::vector<exasol::udf::protocol::Buffer> buffers_data{{8, 16}};
        const auto nodes = builder.CreateVectorOfStructs(nodes_data);
        const auto buffers = builder.CreateVectorOfStructs(buffers_data);
        const auto variadic_buffer_counts = builder.CreateVector<int64_t>({2});
        const auto batch = exasol::udf::protocol::CreateDataRecordBatch(
            builder, exasol::udf::protocol::BufferTransport_Inline, true, 3,
            nodes, buffers, variadic_buffer_counts);
        const auto frame = exasol::udf::protocol::CreateFrame(builder, 7, 0, batch);
        builder.Finish(frame);

        assert(exasol::udf::protocol::VerifyFrameBuffer(
            builder.GetBufferPointer(), builder.GetSize()));
        const auto* decoded = exasol::udf::protocol::GetFrame(builder.GetBufferPointer());
        assert(decoded->control_message() == nullptr);
        assert(decoded->data_record_batch()->length() == 3);
        assert(decoded->data_record_batch()->nodes()->Get(0)->length() == 3);
        assert(decoded->data_record_batch()->nodes()->Get(0)->null_count() == 1);
        assert(decoded->data_record_batch()->buffers()->Get(0)->offset() == 8);
        assert(decoded->data_record_batch()->buffers()->Get(0)->length() == 16);
        assert(decoded->data_record_batch()->variadic_buffer_counts()->Get(0) == 2);
    }

    {
        exasol::udf::v2::third_party::flatbuffers::FlatBufferBuilder builder;
        const auto call_name = builder.CreateString("first_batch");
        const auto open_call = exasol::udf::protocol::CreateOpenCall(builder, call_name);
        const auto control_message = exasol::udf::protocol::CreateControlMessage(
            builder, 0, 0, open_call);
        const auto batch = exasol::udf::protocol::CreateDataRecordBatch(builder);
        const auto frame = exasol::udf::protocol::CreateFrame(
            builder, 7, control_message, batch);
        builder.Finish(frame);

        assert(exasol::udf::protocol::VerifyFrameBuffer(
            builder.GetBufferPointer(), builder.GetSize()));
        const auto* decoded = exasol::udf::protocol::GetFrame(builder.GetBufferPointer());
        assert(decoded->control_message()->open_call()->call_name()->str() == "first_batch");
        assert(decoded->data_record_batch() != nullptr);
    }
}
