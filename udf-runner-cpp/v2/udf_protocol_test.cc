#include "udf_protocol.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

namespace
{

bool verify_frame(const exasol::udf::v2::third_party::flatbuffers::FlatBufferBuilder& builder)
{
    exasol::udf::v2::third_party::flatbuffers::Verifier verifier(builder.GetBufferPointer(),
                                                                 builder.GetSize());
    return exasol::udf::protocol::VerifyFrameBuffer(verifier);
}

} // namespace

TEST(UdfProtocolTest, EncodesProtocolFrames)
{
    {
        exasol::udf::v2::third_party::flatbuffers::FlatBufferBuilder builder;
        const auto version      = exasol::udf::protocol::CreateVersion(builder, 2, 0);
        const auto capabilities = exasol::udf::protocol::CreateServerCapabilities(
            builder, version, exasol::udf::protocol::Endianness_Little, 4);
        const auto high_level_name = builder.CreateString("exasol.udf");
        const auto high_level_name_value =
            exasol::udf::protocol::CreateStringPayload(builder, high_level_name);
        const auto high_level_name_payload = exasol::udf::protocol::CreatePayload(
            builder, builder.CreateString("high_level_name"),
            exasol::udf::protocol::PayloadValue_StringPayload, high_level_name_value.Union());
        const auto high_level_version = builder.CreateString("2.0-dev");
        const auto high_level_version_value =
            exasol::udf::protocol::CreateStringPayload(builder, high_level_version);
        const auto high_level_version_payload = exasol::udf::protocol::CreatePayload(
            builder, builder.CreateString("high_level_version"),
            exasol::udf::protocol::PayloadValue_StringPayload, high_level_version_value.Union());
        const auto capability_payloads = builder.CreateVector(
            std::vector<
                exasol::udf::v2::third_party::flatbuffers::Offset<exasol::udf::protocol::Payload>>{
                high_level_name_payload, high_level_version_payload});
        const auto payloads = exasol::udf::protocol::CreatePayloads(builder, capability_payloads);
        const auto control_message = exasol::udf::protocol::CreateControlMessage(
            builder, payloads, 0, 0, 0,
            exasol::udf::protocol::ControlMessageValue_ServerCapabilities, capabilities.Union());
        const auto frame = exasol::udf::protocol::CreateFrame(builder, 0, control_message);
        builder.Finish(frame);

        assert(verify_frame(builder));
        const auto* decoded = exasol::udf::protocol::GetFrame(builder.GetBufferPointer());
        const auto* decoded_capabilities =
            decoded->control_message()->value_as_ServerCapabilities();
        assert(decoded->stream_id() == 0);
        assert(decoded_capabilities->supported_version()->major() == 2);
        assert(decoded_capabilities->supported_version()->minor() == 0);
        assert(decoded_capabilities->endianness() == exasol::udf::protocol::Endianness_Little);
        assert(decoded_capabilities->number_of_supported_workers() == 4);
        assert(decoded_capabilities->number_of_supported_workers() > 0);
        assert(decoded->control_message()->payloads()->payloads()->size() == 2);
        assert(decoded->control_message()->payloads()->payloads()->Get(0)->name()->str() ==
               "high_level_name");
        assert(decoded->control_message()
                   ->payloads()
                   ->payloads()
                   ->Get(0)
                   ->payload_as_StringPayload()
                   ->value()
                   ->str() == "exasol.udf");
        assert(decoded->control_message()->payloads()->payloads()->Get(1)->name()->str() ==
               "high_level_version");
        assert(decoded->control_message()
                   ->payloads()
                   ->payloads()
                   ->Get(1)
                   ->payload_as_StringPayload()
                   ->value()
                   ->str() == "2.0-dev");
    }

    {
        exasol::udf::v2::third_party::flatbuffers::FlatBufferBuilder builder;
        const auto call_name       = builder.CreateString("example");
        const auto open_call       = exasol::udf::protocol::CreateOpenCall(builder, call_name);
        const auto next            = exasol::udf::protocol::CreateNext(builder, 1024);
        const auto control_message = exasol::udf::protocol::CreateControlMessage(
            builder, 0, next, 0, 0, exasol::udf::protocol::ControlMessageValue_OpenCall,
            open_call.Union());
        const auto frame = exasol::udf::protocol::CreateFrame(builder, 7, control_message);
        builder.Finish(frame);

        assert(verify_frame(builder));
        const auto* decoded = exasol::udf::protocol::GetFrame(builder.GetBufferPointer());
        assert(decoded->stream_id() == 7);
        assert(decoded->data_record_batch_metadata() == nullptr);
        assert(decoded->control_message()->value_type() ==
               exasol::udf::protocol::ControlMessageValue_OpenCall);
        assert(decoded->control_message()->value_as_OpenCall()->call_name()->str() == "example");
        assert(decoded->control_message()->next()->byte_budget() == 1024);
    }

    {
        exasol::udf::v2::third_party::flatbuffers::FlatBufferBuilder builder;
        const std::vector<exasol::udf::protocol::FieldNode> nodes_data{{3, 1}};
        const std::vector<exasol::udf::protocol::Buffer> buffers_data{{8, 16}};
        const auto nodes                  = builder.CreateVectorOfStructs(nodes_data);
        const auto buffers                = builder.CreateVectorOfStructs(buffers_data);
        const auto variadic_buffer_counts = builder.CreateVector<int64_t>({2});
        const auto batch                  = exasol::udf::protocol::CreateDataRecordBatchMetadata(
            builder, exasol::udf::protocol::BufferTransport_Inline, true, 3, nodes, buffers,
            variadic_buffer_counts);
        const auto frame = exasol::udf::protocol::CreateFrame(builder, 7, 0, batch);
        builder.Finish(frame);

        assert(verify_frame(builder));
        const auto* decoded = exasol::udf::protocol::GetFrame(builder.GetBufferPointer());
        assert(decoded->control_message() == nullptr);
        assert(decoded->data_record_batch_metadata()->length() == 3);
        assert(decoded->data_record_batch_metadata()->nodes()->Get(0)->length() == 3);
        assert(decoded->data_record_batch_metadata()->nodes()->Get(0)->null_count() == 1);
        assert(decoded->data_record_batch_metadata()->buffers()->Get(0)->offset() == 8);
        assert(decoded->data_record_batch_metadata()->buffers()->Get(0)->length() == 16);
        assert(decoded->data_record_batch_metadata()->variadic_buffer_counts()->Get(0) == 2);
    }

    {
        exasol::udf::v2::third_party::flatbuffers::FlatBufferBuilder builder;
        const auto call_name       = builder.CreateString("first_batch");
        const auto open_call       = exasol::udf::protocol::CreateOpenCall(builder, call_name);
        const auto next            = exasol::udf::protocol::CreateNext(builder, 1024);
        const auto control_message = exasol::udf::protocol::CreateControlMessage(
            builder, 0, next, 0, 0, exasol::udf::protocol::ControlMessageValue_OpenCall,
            open_call.Union());
        const auto batch = exasol::udf::protocol::CreateDataRecordBatchMetadata(builder);
        const auto frame = exasol::udf::protocol::CreateFrame(builder, 7, control_message, batch);
        builder.Finish(frame);

        assert(verify_frame(builder));
        const auto* decoded = exasol::udf::protocol::GetFrame(builder.GetBufferPointer());
        assert(decoded->control_message()->value_as_OpenCall()->call_name()->str() ==
               "first_batch");
        assert(decoded->data_record_batch_metadata() != nullptr);
    }

    {
        exasol::udf::v2::third_party::flatbuffers::FlatBufferBuilder builder;
        const auto close_call       = exasol::udf::protocol::CreateCloseCall(builder);
        const auto close_connection = exasol::udf::protocol::CreateCloseConnection(builder);
        const auto close_control =
            exasol::udf::protocol::CreateCloseControlMessage(builder, close_call, close_connection);
        const auto error_code    = builder.CreateString("example_error");
        const auto error_message = builder.CreateString("example failure");
        const auto error = exasol::udf::protocol::CreateError(builder, error_code, error_message);
        const auto control_message = exasol::udf::protocol::CreateControlMessage(
            builder, 0, 0, 0, error, exasol::udf::protocol::ControlMessageValue_CloseControlMessage,
            close_control.Union());
        const auto frame = exasol::udf::protocol::CreateFrame(builder, 7, control_message);
        builder.Finish(frame);

        assert(verify_frame(builder));
        const auto* decoded       = exasol::udf::protocol::GetFrame(builder.GetBufferPointer());
        const auto* decoded_close = decoded->control_message()->value_as_CloseControlMessage();
        assert(decoded_close->close_call() != nullptr);
        assert(decoded_close->close_connection() != nullptr);
        assert(decoded->control_message()->error()->message()->str() == "example failure");
    }
}
