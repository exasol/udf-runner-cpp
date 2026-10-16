#include <exasol/udf/v2/interface/message_view/call_message_view.hpp>

#include <gtest/gtest.h>

#include <arrow/api.h>
#include <arrow/c/bridge.h>

namespace exasol::udf::v2
{

TEST(CallMessageViewTest, EmptyViewReportsNoFields)
{
    CallMessageView view;
    EXPECT_FALSE(view.has_open_call());
    EXPECT_FALSE(view.open_call());
    EXPECT_FALSE(view.has_payloads());
    EXPECT_FALSE(view.payloads());
    EXPECT_FALSE(view.has_data_schema());
    EXPECT_EQ(std::move(view.data_schema()).take_schema(), nullptr);
    EXPECT_FALSE(view.has_next());
    EXPECT_FALSE(view.next());
    EXPECT_FALSE(view.has_record_batch());
    EXPECT_EQ(std::move(view.record_batch()).take_array(), nullptr);
    EXPECT_FALSE(view.has_error());
    EXPECT_FALSE(view.error());
    EXPECT_FALSE(view.has_close_call());
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

    ArrowSchema received_schema{};
    ArrowArray received_array{};
    ASSERT_TRUE(arrow::ExportRecordBatch(*source, &received_array, &received_schema).ok());
    DataSchemaView schema_view(DataSchema{ArrowSchemaPtr(&received_schema), false, false});
    RecordBatchView batch_view(RecordBatch{ArrowArrayPtr(&received_array), true});
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
}

} // namespace exasol::udf::v2
