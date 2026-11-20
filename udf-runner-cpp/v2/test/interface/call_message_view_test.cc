#include "../common/protocol_test_utils.hpp"
#include <exasol/udf/v2/internal/call_message_builder_impl.hpp>
#include <exasol/udf/v2/internal/call_message_view_impl.hpp>
#include <exasol/udf/v2/internal/data_schema_view_impl.hpp>
#include <exasol/udf/v2/internal/record_batch_view_impl.hpp>

#include <gtest/gtest.h>

#include <arrow/api.h>
#include <arrow/c/bridge.h>

#include <memory>
#include <type_traits>
#include <utility>

namespace exasol::udf::v2::internal
{

using interface::ArrowArrayPtr;
using interface::ArrowSchemaPtr;

static_assert(std::is_abstract_v<interface::CallMessageView>);
static_assert(std::is_abstract_v<interface::OpenCallView>);
static_assert(std::is_abstract_v<interface::PayloadsView>);
static_assert(std::is_abstract_v<interface::PayloadView>);
static_assert(std::is_abstract_v<interface::PayloadValueView>);
static_assert(std::is_abstract_v<interface::ResetView>);
static_assert(std::is_abstract_v<interface::ErrorView>);

TEST(CallMessageViewTest, FrameWithoutOptionalFieldsStillHasStreamId)
{
    CallMessageViewImpl view(test::make_empty_decoded_frame(7));
    EXPECT_EQ(view.stream_id(), 7);
    EXPECT_FALSE(view.open_call());
    EXPECT_FALSE(view.payloads());
    EXPECT_FALSE(view.data_schema());
    EXPECT_FALSE(view.reset());
    EXPECT_FALSE(view.record_batch());
    EXPECT_FALSE(view.error());
    EXPECT_FALSE(view.close_call());
}

TEST(CallMessageViewTest, RejectsNullDecodedFrame)
{
    EXPECT_DEATH(
        {
            const CallMessageViewImpl view(nullptr);
            static_cast<void>(view);
        },
        "");
}

TEST(CallMessageViewTest, ImportsAndBuildsOutgoingArrowDataThroughInterface)
{
    arrow::Int64Builder id_builder;
    ASSERT_TRUE(id_builder.Append(7).ok());
    ASSERT_TRUE(id_builder.Append(11).ok());
    std::shared_ptr<arrow::Array> ids;
    ASSERT_TRUE(id_builder.Finish(&ids).ok());
    const auto schema = arrow::schema({arrow::field("id", arrow::int64())});
    const auto source = arrow::RecordBatch::Make(schema, 2, {ids});

    ArrowSchemaPtr received_schema{new ArrowSchema{}};
    ArrowArrayPtr received_array{new ArrowArray{}};
    ASSERT_TRUE(
        arrow::ExportRecordBatch(*source, received_array.get(), received_schema.get()).ok());
    DataSchemaViewImpl schema_view(std::move(received_schema), false, false);
    RecordBatchViewImpl batch_view(std::move(received_array), true);
    auto schema_owner = std::move(schema_view).take_schema();
    auto array_owner  = std::move(batch_view).take_array();
    ASSERT_NE(schema_owner, nullptr);
    ASSERT_NE(array_owner, nullptr);

    auto imported = arrow::ImportRecordBatch(array_owner.get(), schema_owner.get());
    ASSERT_TRUE(imported.ok());
    ASSERT_EQ(imported.ValueOrDie()->num_rows(), 2);
    const auto imported_ids =
        std::static_pointer_cast<arrow::Int64Array>(imported.ValueOrDie()->column(0));
    EXPECT_EQ(imported_ids->Value(0), 7);
    EXPECT_EQ(imported_ids->Value(1), 11);

    ArrowSchemaPtr sent_schema{new ArrowSchema{}};
    ArrowArrayPtr sent_array{new ArrowArray{}};
    ASSERT_TRUE(
        arrow::ExportRecordBatch(*imported.ValueOrDie(), sent_array.get(), sent_schema.get()).ok());
    auto outgoing = std::make_unique<CallMessageBuilderImpl>(2);
    outgoing->data_schema(std::move(sent_schema), false, false)
        .record_batch(std::move(sent_array), true);

    const auto& outgoing_message = outgoing->message();
    ASSERT_TRUE(outgoing_message.data_schema().has_value());
    ASSERT_TRUE(outgoing_message.record_batch().has_value());
    EXPECT_EQ(outgoing_message.stream_id(), 2);
    ASSERT_NE(outgoing_message.data_schema()->schema().get(), nullptr);
    ASSERT_NE(outgoing_message.record_batch()->array().get(), nullptr);
    EXPECT_NE(outgoing_message.data_schema()->schema().get()->release, nullptr);
    EXPECT_NE(outgoing_message.record_batch()->array().get()->release, nullptr);
    EXPECT_EQ(outgoing_message.record_batch()->array().get()->length, 2);
    EXPECT_TRUE(outgoing_message.record_batch()->is_end_of_group());
}

} // namespace exasol::udf::v2::internal
