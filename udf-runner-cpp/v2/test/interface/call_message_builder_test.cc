#include <exasol/udf/v2/interface/message_builder/call_message_builder.hpp>

#include <gtest/gtest.h>

namespace exasol::udf::v2
{

TEST(CallMessageBuilderTest, AddPayloadIsNotImplemented)
{
    EXPECT_DEATH(
        {
            CallMessageBuilder builder;
            builder.add_payload("request", "{}");
        },
        "");
}

TEST(CallMessageBuilderTest, BuildsCompositeMessage)
{
    ArrowSchema schema{};
    ArrowArray array{};
    CallMessageBuilder builder;
    builder.open_call("nested")
        .data_schema(ArrowSchemaPtr(&schema), true, true)
        .next(1024, true, 42)
        .record_batch(ArrowArrayPtr(&array), true)
        .error("E_TEST", "failure")
        .close_call();

    ASSERT_TRUE(builder.message().open_call.has_value());
    ASSERT_TRUE(builder.message().data_schema.has_value());
    EXPECT_EQ(builder.message().data_schema->schema.get(), &schema);
    ASSERT_TRUE(builder.message().outbound_next.has_value());
    EXPECT_EQ(builder.message().outbound_next->byte_budget, 1024);
    ASSERT_TRUE(builder.message().record_batch.has_value());
    EXPECT_EQ(builder.message().record_batch->array.get(), &array);
    auto message = std::move(builder).take();
    EXPECT_TRUE(message.error.has_value());
    EXPECT_TRUE(message.close_call);
}

} // namespace exasol::udf::v2
