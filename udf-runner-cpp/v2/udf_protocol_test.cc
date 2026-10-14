#include <exasol/udf/v2/udf_protocol.hpp>
#include <exasol/udf/v2/message_view/call_message_view.hpp>
#include <exasol/udf/v2/message_view/control_message_view.hpp>

#include <cassert>
#include <cstdint>
#include <memory>
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

TEST(UdfProtocolTest, DecodedFrameOwnsBufferForGeneratedViews)
{
    exasol::udf::v2::third_party::flatbuffers::FlatBufferBuilder builder;
    const auto version      = exasol::udf::protocol::CreateVersion(builder, 2, 0);
    const auto capabilities = exasol::udf::protocol::CreateServerCapabilities(
        builder, version, exasol::udf::protocol::Endianness_Little, 4);
    const auto error_code    = builder.CreateString("example_error");
    const auto error_message = builder.CreateString("example failure");
    const auto error = exasol::udf::protocol::CreateError(builder, error_code, error_message);
    const auto payload_name  = builder.CreateString("name");
    const auto payload_value = builder.CreateString("value");
    const auto payload_data  = exasol::udf::protocol::CreateStringPayload(builder, payload_value);
    const auto payload       = exasol::udf::protocol::CreatePayload(
        builder, payload_name, exasol::udf::protocol::PayloadValue_StringPayload,
        payload_data.Union());
    const auto payloads = exasol::udf::protocol::CreatePayloads(
        builder, builder.CreateVector(std::vector<exasol::udf::v2::third_party::flatbuffers::Offset<
                                          exasol::udf::protocol::Payload>>{payload}));
    const auto control_message = exasol::udf::protocol::CreateControlMessage(
        builder, payloads, 0, 0, error,
        exasol::udf::protocol::ControlMessageValue_ServerCapabilities, capabilities.Union());
    const auto frame = exasol::udf::protocol::CreateFrame(builder, 0, control_message);
    builder.Finish(frame);

    const std::vector<std::uint8_t> encoded(builder.GetBufferPointer(),
                                            builder.GetBufferPointer() + builder.GetSize());
    auto decoded = exasol::udf::protocol::DecodedFrame::decode(encoded);
    ASSERT_TRUE(decoded.has_value());

    const auto* control = decoded->frame().control_message();
    ASSERT_NE(control, nullptr);
    ASSERT_NE(control->value_as_ServerCapabilities(), nullptr);
    EXPECT_EQ(control->value_as_ServerCapabilities()->supported_version()->major(), 2);
    ASSERT_NE(control->payloads(), nullptr);
    EXPECT_EQ(control->payloads()->payloads()->Get(0)->name()->str(), "name");
    ASSERT_NE(control->error(), nullptr);
    EXPECT_EQ(control->error()->message()->str(), "example failure");

    const auto decoded_frame =
        std::make_shared<exasol::udf::protocol::DecodedFrame>(std::move(decoded.value()));
    const exasol::udf::v2::ControlMessageView control_view(decoded_frame);
    EXPECT_TRUE(control_view.has_server_capabilities());
    EXPECT_EQ(control_view.server_capabilities()->get().major, 2);
    ASSERT_TRUE(control_view.payloads());
    EXPECT_EQ(control_view.payloads()->get().payloads()->Get(0)->name()->str(), "name");
    EXPECT_TRUE(control_view.has_error());
    EXPECT_EQ(control_view.error()->get().message(), "example failure");
}

TEST(UdfProtocolTest, FrameBackedCallViewPopulatesHighLevelFields)
{
    exasol::udf::v2::third_party::flatbuffers::FlatBufferBuilder builder;
    const auto call_name     = builder.CreateString("example");
    const auto open_call     = exasol::udf::protocol::CreateOpenCall(builder, call_name);
    const auto error_code    = builder.CreateString("example_error");
    const auto error_message = builder.CreateString("example failure");
    const auto error = exasol::udf::protocol::CreateError(builder, error_code, error_message);
    const auto control_message = exasol::udf::protocol::CreateControlMessage(
        builder, 0, 0, 0, error, exasol::udf::protocol::ControlMessageValue_OpenCall,
        open_call.Union());
    const auto frame = exasol::udf::protocol::CreateFrame(builder, 7, control_message);
    builder.Finish(frame);

    const std::vector<std::uint8_t> encoded(builder.GetBufferPointer(),
                                            builder.GetBufferPointer() + builder.GetSize());
    auto decoded = exasol::udf::protocol::DecodedFrame::decode(encoded);
    ASSERT_TRUE(decoded.has_value());

    const auto decoded_frame =
        std::make_shared<exasol::udf::protocol::DecodedFrame>(std::move(decoded.value()));
    const exasol::udf::v2::CallMessageView call_view(decoded_frame);
    EXPECT_TRUE(call_view.has_open_call());
    EXPECT_EQ(call_view.open_call()->get().call_name, "example");
    EXPECT_TRUE(call_view.has_error());
    EXPECT_EQ(call_view.error()->get().code(), "example_error");

    exasol::udf::v2::third_party::flatbuffers::FlatBufferBuilder batch_builder;
    const auto batch       = exasol::udf::protocol::CreateDataRecordBatchMetadata(batch_builder);
    const auto batch_frame = exasol::udf::protocol::CreateFrame(batch_builder, 7, 0, batch);
    batch_builder.Finish(batch_frame);
    const std::vector<std::uint8_t> encoded_batch(
        batch_builder.GetBufferPointer(),
        batch_builder.GetBufferPointer() + batch_builder.GetSize());
    auto decoded_batch = exasol::udf::protocol::DecodedFrame::decode(encoded_batch);
    ASSERT_TRUE(decoded_batch.has_value());
    const auto decoded_batch_frame =
        std::make_shared<exasol::udf::protocol::DecodedFrame>(std::move(decoded_batch.value()));
    const exasol::udf::v2::CallMessageView batch_view(decoded_batch_frame);
    EXPECT_FALSE(batch_view.has_record_batch());
    EXPECT_EQ(batch_view.record_batch().array(), nullptr);

    exasol::udf::v2::third_party::flatbuffers::FlatBufferBuilder close_builder;
    const auto close_call       = exasol::udf::protocol::CreateCloseCall(close_builder);
    const auto close_connection = exasol::udf::protocol::CreateCloseConnection(close_builder);
    const auto close_control    = exasol::udf::protocol::CreateCloseControlMessage(
        close_builder, close_call, close_connection);
    const auto close_message = exasol::udf::protocol::CreateControlMessage(
        close_builder, 0, 0, 0, 0, exasol::udf::protocol::ControlMessageValue_CloseControlMessage,
        close_control.Union());
    const auto close_frame = exasol::udf::protocol::CreateFrame(close_builder, 7, close_message);
    close_builder.Finish(close_frame);
    const std::vector<std::uint8_t> encoded_close(
        close_builder.GetBufferPointer(),
        close_builder.GetBufferPointer() + close_builder.GetSize());
    auto decoded_close = exasol::udf::protocol::DecodedFrame::decode(encoded_close);
    ASSERT_TRUE(decoded_close.has_value());
    const auto decoded_close_frame =
        std::make_shared<exasol::udf::protocol::DecodedFrame>(std::move(decoded_close.value()));
    EXPECT_TRUE(exasol::udf::v2::CallMessageView(decoded_close_frame).close_call());
    EXPECT_TRUE(exasol::udf::v2::ControlMessageView(decoded_close_frame).close_connection());
}

TEST(UdfProtocolTest, FrameBackedViewsExposeFlatbufferFields)
{
    exasol::udf::v2::third_party::flatbuffers::FlatBufferBuilder builder;
    const auto call_name     = builder.CreateString("example");
    const auto open_call     = exasol::udf::protocol::CreateOpenCall(builder, call_name);
    const auto next          = exasol::udf::protocol::CreateNext(builder, 2048, true, 42);
    const auto error_code    = builder.CreateString("example_error");
    const auto error_message = builder.CreateString("example failure");
    const auto error = exasol::udf::protocol::CreateError(builder, error_code, error_message);
    const auto payload_name  = builder.CreateString("name");
    const auto payload_value = builder.CreateString("value");
    const auto payload_data  = exasol::udf::protocol::CreateStringPayload(builder, payload_value);
    const auto payload       = exasol::udf::protocol::CreatePayload(
        builder, payload_name, exasol::udf::protocol::PayloadValue_StringPayload,
        payload_data.Union());
    const auto payloads = exasol::udf::protocol::CreatePayloads(
        builder, builder.CreateVector(std::vector<exasol::udf::v2::third_party::flatbuffers::Offset<
                                          exasol::udf::protocol::Payload>>{payload}));
    const auto control_message = exasol::udf::protocol::CreateControlMessage(
        builder, payloads, next, 0, error, exasol::udf::protocol::ControlMessageValue_OpenCall,
        open_call.Union());
    const auto frame = exasol::udf::protocol::CreateFrame(builder, 7, control_message);
    builder.Finish(frame);

    const std::vector<std::uint8_t> encoded(builder.GetBufferPointer(),
                                            builder.GetBufferPointer() + builder.GetSize());
    auto decoded = exasol::udf::protocol::DecodedFrame::decode(encoded);
    ASSERT_TRUE(decoded.has_value());
    const auto decoded_frame =
        std::make_shared<exasol::udf::protocol::DecodedFrame>(std::move(decoded.value()));

    const exasol::udf::v2::CallMessageView call_view(decoded_frame);
    EXPECT_TRUE(call_view.has_payloads());
    EXPECT_EQ(call_view.payloads()->get().payloads()->Get(0)->name()->str(), "name");
    EXPECT_TRUE(call_view.has_next());
    EXPECT_TRUE(call_view.next()->get().reset);
    EXPECT_EQ(call_view.next()->get().row_id, 42);
    ASSERT_TRUE(call_view.flatbuffer_error());
    EXPECT_EQ(call_view.flatbuffer_error()->get().message()->str(), "example failure");

    const exasol::udf::v2::ControlMessageView control_view(decoded_frame);
    EXPECT_TRUE(control_view.has_payloads());
    EXPECT_EQ(control_view.payloads()->get().payloads()->Get(0)->name()->str(), "name");
    ASSERT_TRUE(control_view.flatbuffer_error());
    EXPECT_EQ(control_view.flatbuffer_error()->get().code()->str(), "example_error");
    EXPECT_FALSE(control_view.keep_alive());
    EXPECT_FALSE(control_view.flatbuffer_server_capabilities());
}

TEST(UdfProtocolTest, EmptyAndDataOnlyFramesProduceEmptyViews)
{
    const exasol::udf::v2::CallMessageView empty_call_view(
        std::shared_ptr<const exasol::udf::protocol::DecodedFrame>{});
    const exasol::udf::v2::ControlMessageView empty_control_view(
        std::shared_ptr<const exasol::udf::protocol::DecodedFrame>{});
    EXPECT_FALSE(empty_call_view.has_open_call());
    EXPECT_FALSE(empty_control_view.has_server_capabilities());

    exasol::udf::v2::third_party::flatbuffers::FlatBufferBuilder builder;
    const auto batch = exasol::udf::protocol::CreateDataRecordBatchMetadata(builder);
    const auto frame = exasol::udf::protocol::CreateFrame(builder, 1, 0, batch);
    builder.Finish(frame);
    const std::vector<std::uint8_t> encoded(builder.GetBufferPointer(),
                                            builder.GetBufferPointer() + builder.GetSize());
    auto decoded = exasol::udf::protocol::DecodedFrame::decode(encoded);
    ASSERT_TRUE(decoded.has_value());
    const auto decoded_frame =
        std::make_shared<exasol::udf::protocol::DecodedFrame>(std::move(decoded.value()));
    const exasol::udf::v2::CallMessageView call_view(decoded_frame);
    const exasol::udf::v2::ControlMessageView control_view(decoded_frame);
    EXPECT_FALSE(call_view.has_open_call());
    EXPECT_FALSE(control_view.has_server_capabilities());
    EXPECT_FALSE(control_view.has_payloads());
    EXPECT_FALSE(control_view.payloads());
}

TEST(UdfProtocolTest, RejectsEmptyAndInvalidFrames)
{
    EXPECT_FALSE(exasol::udf::protocol::DecodedFrame::decode({}).has_value());
    const std::vector<std::uint8_t> invalid{0, 1, 2, 3};
    EXPECT_FALSE(exasol::udf::protocol::DecodedFrame::decode(invalid).has_value());
}

TEST(UdfProtocolTest, ViewsHandleMissingOptionalFlatbufferFields)
{
    exasol::udf::v2::third_party::flatbuffers::FlatBufferBuilder call_builder;
    const auto open_call       = exasol::udf::protocol::CreateOpenCall(call_builder);
    const auto error           = exasol::udf::protocol::CreateError(call_builder);
    const auto control_message = exasol::udf::protocol::CreateControlMessage(
        call_builder, 0, 0, 0, error, exasol::udf::protocol::ControlMessageValue_OpenCall,
        open_call.Union());
    const auto frame = exasol::udf::protocol::CreateFrame(call_builder, 0, control_message);
    call_builder.Finish(frame);
    const std::vector<std::uint8_t> encoded_call(
        call_builder.GetBufferPointer(), call_builder.GetBufferPointer() + call_builder.GetSize());
    auto decoded_call = exasol::udf::protocol::DecodedFrame::decode(encoded_call);
    ASSERT_TRUE(decoded_call.has_value());
    const auto call_frame =
        std::make_shared<exasol::udf::protocol::DecodedFrame>(std::move(decoded_call.value()));
    const exasol::udf::v2::CallMessageView call_view(call_frame);
    EXPECT_TRUE(call_view.has_open_call());
    EXPECT_TRUE(call_view.open_call()->get().call_name.empty());
    ASSERT_TRUE(call_view.error());
    EXPECT_TRUE(call_view.error()->get().code().empty());
    EXPECT_TRUE(call_view.error()->get().message().empty());

    exasol::udf::v2::third_party::flatbuffers::FlatBufferBuilder capabilities_builder;
    const auto capabilities = exasol::udf::protocol::CreateServerCapabilities(capabilities_builder);
    const auto capabilities_control = exasol::udf::protocol::CreateControlMessage(
        capabilities_builder, 0, 0, 0, 0,
        exasol::udf::protocol::ControlMessageValue_ServerCapabilities, capabilities.Union());
    const auto capabilities_frame =
        exasol::udf::protocol::CreateFrame(capabilities_builder, 0, capabilities_control);
    capabilities_builder.Finish(capabilities_frame);
    const std::vector<std::uint8_t> encoded_capabilities(
        capabilities_builder.GetBufferPointer(),
        capabilities_builder.GetBufferPointer() + capabilities_builder.GetSize());
    auto decoded_capabilities = exasol::udf::protocol::DecodedFrame::decode(encoded_capabilities);
    ASSERT_TRUE(decoded_capabilities.has_value());
    const auto capabilities_decoded_frame = std::make_shared<exasol::udf::protocol::DecodedFrame>(
        std::move(decoded_capabilities.value()));
    const exasol::udf::v2::ControlMessageView capabilities_view(capabilities_decoded_frame);
    ASSERT_TRUE(capabilities_view.server_capabilities());
    EXPECT_EQ(capabilities_view.server_capabilities()->get().major, 0);
    EXPECT_EQ(capabilities_view.server_capabilities()->get().minor, 0);
    ASSERT_TRUE(capabilities_view.flatbuffer_server_capabilities());
    EXPECT_EQ(capabilities_view.flatbuffer_server_capabilities()->get().supported_version(),
              nullptr);

    exasol::udf::v2::third_party::flatbuffers::FlatBufferBuilder close_builder;
    const auto close_control = exasol::udf::protocol::CreateCloseControlMessage(close_builder);
    const auto close_message = exasol::udf::protocol::CreateControlMessage(
        close_builder, 0, 0, 0, 0, exasol::udf::protocol::ControlMessageValue_CloseControlMessage,
        close_control.Union());
    const auto close_frame = exasol::udf::protocol::CreateFrame(close_builder, 0, close_message);
    close_builder.Finish(close_frame);
    const std::vector<std::uint8_t> encoded_close(
        close_builder.GetBufferPointer(),
        close_builder.GetBufferPointer() + close_builder.GetSize());
    auto decoded_close = exasol::udf::protocol::DecodedFrame::decode(encoded_close);
    ASSERT_TRUE(decoded_close.has_value());
    const auto close_decoded_frame =
        std::make_shared<exasol::udf::protocol::DecodedFrame>(std::move(decoded_close.value()));
    EXPECT_FALSE(exasol::udf::v2::CallMessageView(close_decoded_frame).close_call());
    EXPECT_FALSE(exasol::udf::v2::ControlMessageView(close_decoded_frame).close_connection());
}

TEST(UdfProtocolTest, FrameBackedControlViewDecodesCapabilitiesAndKeepAlive)
{
    exasol::udf::v2::third_party::flatbuffers::FlatBufferBuilder builder;
    const auto version      = exasol::udf::protocol::CreateVersion(builder, 2, 2);
    const auto capabilities = exasol::udf::protocol::CreateServerCapabilities(
        builder, version, exasol::udf::protocol::Endianness_Big, 8);
    const auto control_message = exasol::udf::protocol::CreateControlMessage(
        builder, 0, 0, 0, 0, exasol::udf::protocol::ControlMessageValue_ServerCapabilities,
        capabilities.Union());
    const auto frame = exasol::udf::protocol::CreateFrame(builder, 0, control_message);
    builder.Finish(frame);

    const std::vector<std::uint8_t> encoded(builder.GetBufferPointer(),
                                            builder.GetBufferPointer() + builder.GetSize());
    auto decoded = exasol::udf::protocol::DecodedFrame::decode(encoded);
    ASSERT_TRUE(decoded.has_value());
    const auto decoded_frame =
        std::make_shared<exasol::udf::protocol::DecodedFrame>(std::move(decoded.value()));
    const exasol::udf::v2::ControlMessageView view(decoded_frame);

    EXPECT_TRUE(view.has_server_capabilities());
    EXPECT_EQ(view.server_capabilities()->get().major, 2);
    EXPECT_EQ(view.server_capabilities()->get().minor, 2);
    EXPECT_EQ(view.server_capabilities()->get().number_of_supported_workers, 8);
    ASSERT_TRUE(view.flatbuffer_server_capabilities());
    EXPECT_EQ(view.flatbuffer_server_capabilities()->get().endianness(),
              exasol::udf::protocol::Endianness_Big);

    exasol::udf::v2::third_party::flatbuffers::FlatBufferBuilder keep_alive_builder;
    const auto keep_alive         = exasol::udf::protocol::CreateKeepAlive(keep_alive_builder);
    const auto keep_alive_control = exasol::udf::protocol::CreateControlMessage(
        keep_alive_builder, 0, 0, 0, 0, exasol::udf::protocol::ControlMessageValue_KeepAlive,
        keep_alive.Union());
    const auto keep_alive_frame =
        exasol::udf::protocol::CreateFrame(keep_alive_builder, 0, keep_alive_control);
    keep_alive_builder.Finish(keep_alive_frame);
    const std::vector<std::uint8_t> keep_alive_encoded(
        keep_alive_builder.GetBufferPointer(),
        keep_alive_builder.GetBufferPointer() + keep_alive_builder.GetSize());
    auto decoded_keep_alive = exasol::udf::protocol::DecodedFrame::decode(keep_alive_encoded);
    ASSERT_TRUE(decoded_keep_alive.has_value());
    const auto keep_alive_view = std::make_shared<exasol::udf::protocol::DecodedFrame>(
        std::move(decoded_keep_alive.value()));
    const exasol::udf::v2::ControlMessageView keep_alive_control_view(keep_alive_view);
    ASSERT_TRUE(keep_alive_control_view.keep_alive());
}
