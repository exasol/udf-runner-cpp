#include <exasol/udf/v2/internal/call_message_builder_impl.hpp>
#include <exasol/udf/v2/internal/message_builder_factory.hpp>

#include <gtest/gtest.h>

#include <type_traits>

namespace exasol::udf::v2::internal
{

static_assert(std::is_abstract_v<interface::CallMessageBuilder>);

TEST(CallMessageBuilderTest, StoresTextAndBinaryPayloadsInOrder)
{
    auto builder = MessageBuilderFactory::create_call_builder(2);
    builder->add_payload("text", "value").add_payload("binary", "\x01\x02", true);

    const auto* builder_impl = dynamic_cast<const CallMessageBuilderImpl*>(builder.get());
    ASSERT_NE(builder_impl, nullptr);
    const auto& payloads = builder_impl->message().payloads();
    ASSERT_EQ(payloads.size(), 2);
    EXPECT_EQ(payloads[0].name, "text");
    EXPECT_EQ(payloads[0].value, "value");
    EXPECT_FALSE(payloads[0].binary);
    EXPECT_EQ(payloads[1].name, "binary");
    EXPECT_EQ(payloads[1].value, std::string("\x01\x02", 2));
    EXPECT_TRUE(payloads[1].binary);
}

TEST(CallMessageBuilderTest, BuildsCompositeMessage)
{
    ArrowSchema schema{};
    ArrowArray array{};
    auto builder = MessageBuilderFactory::create_opening_call_builder(2);
    builder->open_call("nested")
        .add_payload("request", "{}")
        .data_schema(interface::ArrowSchemaPtr(&schema), true, true)
        .reset(42)
        .record_batch(interface::ArrowArrayPtr(&array), true)
        .error("E_TEST", "failure")
        .close_call();

    const auto* builder_impl = dynamic_cast<const OpeningCallMessageBuilderImpl*>(builder.get());
    ASSERT_NE(builder_impl, nullptr);
    const auto& message = builder_impl->message();
    EXPECT_EQ(message.stream_id(), 2);
    EXPECT_TRUE(message.open_call().has_value());
    ASSERT_EQ(message.payloads().size(), 1);
    EXPECT_EQ(message.payloads()[0].name, "request");
    EXPECT_EQ(message.payloads()[0].value, "{}");
    EXPECT_FALSE(message.payloads()[0].binary);
    EXPECT_TRUE(message.data_schema().has_value());
    EXPECT_TRUE(message.reset_row_id().has_value());
    EXPECT_TRUE(message.record_batch().has_value());
    EXPECT_TRUE(message.error().has_value());
    EXPECT_TRUE(message.is_close_call());
}

TEST(CallMessageBuilderTest, BuildsMessageWithDefaultOptions)
{
    ArrowSchema schema{};
    ArrowArray array{};
    auto builder = MessageBuilderFactory::create_call_builder(7);
    builder->data_schema(interface::ArrowSchemaPtr(&schema))
        .data_schema(interface::ArrowSchemaPtr(&schema), true)
        .reset(0)
        .record_batch(interface::ArrowArrayPtr(&array));
    builder->error("E_TEST", "failure").close_call();

    const auto* builder_impl = dynamic_cast<const CallMessageBuilderImpl*>(builder.get());
    ASSERT_NE(builder_impl, nullptr);
    const auto& message = builder_impl->message();
    EXPECT_EQ(message.stream_id(), 7);
    ASSERT_TRUE(message.data_schema().has_value());
    EXPECT_TRUE(message.data_schema()->has_group_id);
    EXPECT_FALSE(message.data_schema()->has_row_id);
    ASSERT_TRUE(message.reset_row_id().has_value());
    EXPECT_EQ(*message.reset_row_id(), 0);
    ASSERT_TRUE(message.record_batch().has_value());
    EXPECT_FALSE(message.record_batch()->is_end_of_group);
    ASSERT_TRUE(message.error().has_value());
    EXPECT_EQ(message.error()->code(), "E_TEST");
    EXPECT_EQ(message.error()->message(), "failure");
    EXPECT_TRUE(message.is_close_call());
}

} // namespace exasol::udf::v2::internal
