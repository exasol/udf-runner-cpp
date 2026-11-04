#include <exasol/udf/v2/internal/call_message_view_impl.hpp>
#include <exasol/udf/v2/internal/data_schema_view_impl.hpp>
#include <exasol/udf/v2/internal/message_builder_factory.hpp>
#include <exasol/udf/v2/internal/record_batch_view_impl.hpp>
#include "../common/protocol_test_utils.hpp"

#include <gtest/gtest.h>

#include <arrow/api.h>
#include <arrow/c/bridge.h>

#include <type_traits>
#include <utility>

namespace exasol::udf::v2::internal
{

using interface::ArrowArrayPtr;
using interface::ArrowSchemaPtr;

static_assert(std::is_abstract_v<interface::CallMessageView>);

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

TEST(CallMessageViewTest, ArrowDataCanRoundTripThroughInterface)
{
    arrow::Int64Builder id_builder;
    ASSERT_TRUE(id_builder.Append(7).ok());
    ASSERT_TRUE(id_builder.Append(11).ok());
    std::shared_ptr<arrow::Array> ids;
    ASSERT_TRUE(id_builder.Finish(&ids).ok());
    const auto schema = arrow::schema({arrow::field("id", arrow::int64())});
    const auto source = arrow::RecordBatch::Make(schema, 2, {ids});

    auto* received_schema = new ArrowSchema{};
    auto* received_array  = new ArrowArray{};
    ASSERT_TRUE(arrow::ExportRecordBatch(*source, received_array, received_schema).ok());
    DataSchemaViewImpl schema_view(ArrowSchemaPtr(received_schema), false, false);
    RecordBatchViewImpl batch_view(ArrowArrayPtr(received_array), true);
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

    auto* sent_schema = new ArrowSchema{};
    auto* sent_array  = new ArrowArray{};
    ASSERT_TRUE(arrow::ExportRecordBatch(*imported.ValueOrDie(), sent_array, sent_schema).ok());
    {
        auto outgoing = MessageBuilderFactory::create_call_builder(2);
        outgoing->data_schema(ArrowSchemaPtr(sent_schema), false, false)
            .record_batch(ArrowArrayPtr(sent_array), true);
    }
}

} // namespace exasol::udf::v2::internal
