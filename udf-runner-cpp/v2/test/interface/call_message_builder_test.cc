#include <exasol/udf/v2/internal/call_message_builder_impl.hpp>
#include <exasol/udf/v2/internal/message_builder_factory.hpp>

#include <gtest/gtest.h>

#include <type_traits>

namespace exasol::udf::v2::internal
{

static_assert(std::is_abstract_v<interface::CallMessageBuilder>);

TEST(CallMessageBuilderTest, AddPayloadIsNotImplemented)
{
    EXPECT_DEATH(
        {
            auto builder = MessageBuilderFactory::create_opening_call_builder(2);
            builder->add_payload("request", "{}");
        },
        "");
}

TEST(CallMessageBuilderTest, BuildsCompositeMessage)
{
    ArrowSchema schema{};
    ArrowArray array{};
    auto builder = MessageBuilderFactory::create_opening_call_builder(2);
    builder->open_call("nested")
        .data_schema(interface::ArrowSchemaPtr(&schema), true, true)
        .next(1024, true, 42)
        .record_batch(interface::ArrowArrayPtr(&array), true)
        .error("E_TEST", "failure")
        .close_call();

    const auto* builder_impl = dynamic_cast<const OpeningCallMessageBuilderImpl*>(builder.get());
    ASSERT_NE(builder_impl, nullptr);
    const auto view = builder_impl->encoding_view();
    EXPECT_EQ(view.stream_id, 2);
    EXPECT_TRUE(view.open_call.has_value());
    EXPECT_TRUE(view.data_schema.has_value());
    EXPECT_TRUE(view.next.has_value());
    EXPECT_TRUE(view.record_batch.has_value());
    EXPECT_TRUE(view.error.has_value());
    EXPECT_TRUE(view.close_call);
}

TEST(CallMessageBuilderTest, BuildsMessageWithDefaultOptions)
{
    ArrowSchema schema{};
    ArrowArray array{};
    auto builder = MessageBuilderFactory::create_call_builder(7);
    builder->data_schema(interface::ArrowSchemaPtr(&schema))
        .data_schema(interface::ArrowSchemaPtr(&schema), true)
        .next(2048)
        .next(1024, true)
        .record_batch(interface::ArrowArrayPtr(&array));
    builder->error("E_TEST", "failure").close_call();

    const auto* builder_impl = dynamic_cast<const CallMessageBuilderImpl*>(builder.get());
    ASSERT_NE(builder_impl, nullptr);
    const auto view = builder_impl->encoding_view();
    EXPECT_EQ(view.stream_id, 7);
    ASSERT_TRUE(view.data_schema.has_value());
    EXPECT_TRUE(view.data_schema->has_group_id);
    EXPECT_FALSE(view.data_schema->has_row_id);
    ASSERT_TRUE(view.next.has_value());
    EXPECT_EQ(view.next->byte_budget, 1024);
    EXPECT_TRUE(view.next->reset);
    EXPECT_EQ(view.next->row_id, 0);
    ASSERT_TRUE(view.record_batch.has_value());
    EXPECT_FALSE(view.record_batch->is_end_of_group);
    ASSERT_TRUE(view.error.has_value());
    EXPECT_EQ(view.error->code(), "E_TEST");
    EXPECT_EQ(view.error->message(), "failure");
    EXPECT_TRUE(view.close_call);
}

} // namespace exasol::udf::v2::internal
