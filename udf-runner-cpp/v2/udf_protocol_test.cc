#include "udf_protocol.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

int main() {
    {
        exasol::udf::v2::third_party::flatbuffers::FlatBufferBuilder builder;
        const auto call_name = builder.CreateString("example");
        const auto open_call = exasol::udf::protocol::CreateOpenCall(builder, call_name);
        const auto next = exasol::udf::protocol::CreateNext(builder, 1024);
        const auto control_message = exasol::udf::protocol::CreateControlMessage(
            builder, 0, next, 0, 0,
            exasol::udf::protocol::ControlMessageValue_OpenCall, open_call.Union());
        const auto frame = exasol::udf::protocol::CreateFrame(
            builder, 7, control_message);
        builder.Finish(frame);

        assert(exasol::udf::protocol::VerifyFrameBuffer(
            builder.GetBufferPointer(), builder.GetSize()));
        const auto* decoded = exasol::udf::protocol::GetFrame(builder.GetBufferPointer());
        assert(decoded->stream_id() == 7);
        assert(decoded->data_record_batch() == nullptr);
        assert(decoded->control_message()->value_type() ==
               exasol::udf::protocol::ControlMessageValue_OpenCall);
        assert(decoded->control_message()->value_as_OpenCall()->call_name()->str() == "example");
        assert(decoded->control_message()->next()->byte_budget() == 1024);
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
        const auto next = exasol::udf::protocol::CreateNext(builder, 1024);
        const auto control_message = exasol::udf::protocol::CreateControlMessage(
            builder, 0, next, 0, 0,
            exasol::udf::protocol::ControlMessageValue_OpenCall, open_call.Union());
        const auto batch = exasol::udf::protocol::CreateDataRecordBatch(builder);
        const auto frame = exasol::udf::protocol::CreateFrame(
            builder, 7, control_message, batch);
        builder.Finish(frame);

        assert(exasol::udf::protocol::VerifyFrameBuffer(
            builder.GetBufferPointer(), builder.GetSize()));
        const auto* decoded = exasol::udf::protocol::GetFrame(builder.GetBufferPointer());
        assert(decoded->control_message()->value_as_OpenCall()->call_name()->str() == "first_batch");
        assert(decoded->data_record_batch() != nullptr);
    }

    {
        exasol::udf::v2::third_party::flatbuffers::FlatBufferBuilder builder;
        const auto close_call = exasol::udf::protocol::CreateCloseCall(builder);
        const auto close_connection = exasol::udf::protocol::CreateCloseConnection(builder);
        const auto close_control = exasol::udf::protocol::CreateCloseControlMessage(
            builder, close_call, close_connection);
        const auto error_code = builder.CreateString("example_error");
        const auto error_message = builder.CreateString("example failure");
        const auto error = exasol::udf::protocol::CreateError(
            builder, error_code, error_message);
        const auto control_message = exasol::udf::protocol::CreateControlMessage(
            builder, 0, 0, 0, error,
            exasol::udf::protocol::ControlMessageValue_CloseControlMessage,
            close_control.Union());
        const auto frame = exasol::udf::protocol::CreateFrame(
            builder, 7, control_message);
        builder.Finish(frame);

        assert(exasol::udf::protocol::VerifyFrameBuffer(
            builder.GetBufferPointer(), builder.GetSize()));
        const auto* decoded = exasol::udf::protocol::GetFrame(builder.GetBufferPointer());
        const auto* decoded_close = decoded->control_message()->value_as_CloseControlMessage();
        assert(decoded_close->close_call() != nullptr);
        assert(decoded_close->close_connection() != nullptr);
        assert(decoded->control_message()->error()->message()->str() == "example failure");
    }
}
