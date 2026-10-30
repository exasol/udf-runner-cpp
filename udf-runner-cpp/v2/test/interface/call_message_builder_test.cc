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

    const auto view = static_cast<const OpeningCallMessageBuilderImpl&>(*builder).encoding_view();
    EXPECT_EQ(view.stream_id, 2);
    EXPECT_TRUE(view.open_call.has_value());
    EXPECT_TRUE(view.data_schema.has_value());
    EXPECT_TRUE(view.next.has_value());
    EXPECT_TRUE(view.record_batch.has_value());
    EXPECT_TRUE(view.error.has_value());
    EXPECT_TRUE(view.close_call);
}

} // namespace exasol::udf::v2::internal
