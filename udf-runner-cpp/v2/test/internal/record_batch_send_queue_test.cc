#include <exasol/udf/v2/internal/record_batch_send_queue_impl.hpp>

#include <arrow/api.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace exasol::udf::v2::internal
{
namespace
{

    arrow::Result<std::shared_ptr<const arrow::RecordBatch>> make_record_batch(
        const std::int64_t num_rows)
    {
        std::vector<std::int64_t> values;
        values.reserve(static_cast<std::size_t>(num_rows));
        for (std::int64_t row = 0; row < num_rows; ++row)
        {
            values.push_back(row);
        }

        arrow::Int64Builder builder;
        ARROW_RETURN_NOT_OK(builder.AppendValues(values));
        std::shared_ptr<arrow::Array> values_array;
        ARROW_RETURN_NOT_OK(builder.Finish(&values_array));

        auto schema = arrow::schema({arrow::field("value", arrow::int64())});
        return arrow::RecordBatch::Make(std::move(schema), num_rows, {std::move(values_array)});
    }

    std::unique_ptr<RecordBatchSendQueueInterface> make_queue()
    {
        return std::make_unique<RecordBatchSendQueueImpl>();
    }

    struct LargeBatchBudgetScenario
    {
        const char* name;
        std::uint32_t budget_bytes;
        std::uint64_t frame_target_bytes;
        std::vector<std::uint64_t> expected_first_slice_bytes;
        bool should_empty_after_first_grant;
    };

    class LargeBatchBudgetTest : public testing::TestWithParam<LargeBatchBudgetScenario>
    {
    };

} // namespace

TEST(RecordBatchSendQueueTest, FirstBatchCanBeSlicedWithoutCreditAndKeepsGroupEndOnFinalSlice)
{
    auto batch_result = make_record_batch(3);
    ASSERT_TRUE(batch_result.ok()) << batch_result.status();
    auto batch                 = std::move(batch_result).ValueOrDie();
    const auto original_buffer = batch->column(0)->data()->buffers.at(1);
    const std::vector<std::uint64_t> row_buffer_bytes{3, 4, 5};
    auto queue = make_queue();

    queue->add_send_budget(2);
    ASSERT_TRUE(queue->enqueue_batch(batch, row_buffer_bytes, true).ok());
    auto first_slice = queue->dequeue_slice(6);
    ASSERT_TRUE(first_slice.has_value());
    EXPECT_EQ(first_slice->record_batch->num_rows(), 2);
    EXPECT_EQ(first_slice->buffer_bytes, 7);
    EXPECT_FALSE(first_slice->is_end_of_group);
    EXPECT_EQ(first_slice->record_batch->column(0)->data()->buffers.at(1), original_buffer);
    EXPECT_EQ(queue->available_budget(), 2);

    auto final_slice = queue->dequeue_slice(6);
    ASSERT_TRUE(final_slice.has_value());
    EXPECT_EQ(final_slice->record_batch->num_rows(), 1);
    EXPECT_EQ(final_slice->buffer_bytes, 5);
    EXPECT_TRUE(final_slice->is_end_of_group);
    EXPECT_TRUE(queue->empty());
    EXPECT_EQ(queue->available_budget(), 2);
}

TEST(RecordBatchSendQueueTest, LaterBatchWaitsForCreditAndIncludesAtMostOneCrossingRow)
{
    auto batch_result = make_record_batch(3);
    ASSERT_TRUE(batch_result.ok()) << batch_result.status();
    auto batch = std::move(batch_result).ValueOrDie();
    auto queue = make_queue();

    const std::vector<std::uint64_t> initial_row_bytes{1};
    auto initial_batch_result = make_record_batch(1);
    ASSERT_TRUE(initial_batch_result.ok()) << initial_batch_result.status();
    auto initial_batch = std::move(initial_batch_result).ValueOrDie();
    ASSERT_TRUE(queue->enqueue_batch(initial_batch, initial_row_bytes, false).ok());
    ASSERT_TRUE(queue->dequeue_slice(64).has_value());

    const std::vector<std::uint64_t> row_buffer_bytes{4, 4, 4};
    ASSERT_TRUE(queue->enqueue_batch(batch, row_buffer_bytes, true).ok());
    EXPECT_FALSE(queue->dequeue_slice(64).has_value());

    queue->add_send_budget(5);
    auto first_credit_slice = queue->dequeue_slice(64);
    ASSERT_TRUE(first_credit_slice.has_value());
    EXPECT_EQ(first_credit_slice->record_batch->num_rows(), 2);
    EXPECT_EQ(first_credit_slice->buffer_bytes, 8);
    EXPECT_FALSE(first_credit_slice->is_end_of_group);
    EXPECT_EQ(queue->available_budget(), 0);
    EXPECT_FALSE(queue->dequeue_slice(64).has_value());

    queue->add_send_budget(4);
    auto final_slice = queue->dequeue_slice(64);
    ASSERT_TRUE(final_slice.has_value());
    EXPECT_EQ(final_slice->record_batch->num_rows(), 1);
    EXPECT_EQ(final_slice->buffer_bytes, 4);
    EXPECT_TRUE(final_slice->is_end_of_group);
    EXPECT_TRUE(queue->empty());
}

TEST(RecordBatchSendQueueTest, ZeroRowBatchCanBeDequeuedWithoutCredit)
{
    auto batch_result = make_record_batch(0);
    ASSERT_TRUE(batch_result.ok()) << batch_result.status();
    auto batch = std::move(batch_result).ValueOrDie();
    auto queue = make_queue();

    ASSERT_TRUE(queue->enqueue_batch(batch, {}, true).ok());
    auto slice = queue->dequeue_slice(64);
    ASSERT_TRUE(slice.has_value());
    EXPECT_EQ(slice->record_batch->num_rows(), 0);
    EXPECT_EQ(slice->buffer_bytes, 0);
    EXPECT_TRUE(slice->is_end_of_group);
    EXPECT_TRUE(queue->empty());
}

TEST(RecordBatchSendQueueTest, RejectsMismatchedRowByteCountsWithoutRetainingBatch)
{
    auto batch_result = make_record_batch(2);
    ASSERT_TRUE(batch_result.ok()) << batch_result.status();
    auto batch = std::move(batch_result).ValueOrDie();
    const std::vector<std::uint64_t> row_buffer_bytes{1};
    auto queue = make_queue();

    const auto status = queue->enqueue_batch(batch, row_buffer_bytes, false);

    EXPECT_FALSE(status.ok());
    EXPECT_TRUE(queue->empty());
    EXPECT_EQ(batch.use_count(), 1);
}

TEST(RecordBatchSendQueueTest, AccumulatesSendBudget)
{
    auto queue = make_queue();
    queue->add_send_budget(10);
    queue->add_send_budget(20);

    EXPECT_EQ(queue->available_budget(), 30);
}

TEST_P(LargeBatchBudgetTest, DrainsLargeBatchAcrossBudgetGrantsWithoutLosingRows)
{
    const auto& scenario = GetParam();
    auto queue           = make_queue();

    auto initial_batch_result = make_record_batch(1);
    ASSERT_TRUE(initial_batch_result.ok()) << initial_batch_result.status();
    auto initial_batch = std::move(initial_batch_result).ValueOrDie();
    const std::vector<std::uint64_t> initial_row_buffer_bytes{8};
    ASSERT_TRUE(queue->enqueue_batch(initial_batch, initial_row_buffer_bytes, false).ok());
    ASSERT_TRUE(queue->dequeue_slice(64).has_value());

    auto batch_result = make_record_batch(10);
    ASSERT_TRUE(batch_result.ok()) << batch_result.status();
    auto batch = std::move(batch_result).ValueOrDie();
    const std::vector<std::uint64_t> row_buffer_bytes(10, 8);
    ASSERT_TRUE(queue->enqueue_batch(batch, row_buffer_bytes, true).ok());

    std::vector<std::uint64_t> observed_slice_bytes;
    std::vector<std::int64_t> observed_rows;
    std::vector<bool> observed_group_end_flags;
    const auto drain_available_slices = [&](const std::uint64_t frame_target_bytes) {
        while (auto slice = queue->dequeue_slice(frame_target_bytes))
        {
            observed_slice_bytes.push_back(slice->buffer_bytes);
            observed_group_end_flags.push_back(slice->is_end_of_group);
            const auto values =
                std::static_pointer_cast<arrow::Int64Array>(slice->record_batch->column(0));
            for (std::int64_t row = 0; row < values->length(); ++row)
            {
                observed_rows.push_back(values->Value(row));
            }
        }
    };

    queue->add_send_budget(scenario.budget_bytes);
    drain_available_slices(scenario.frame_target_bytes);
    ASSERT_EQ(observed_slice_bytes, scenario.expected_first_slice_bytes);
    EXPECT_EQ(queue->empty(), scenario.should_empty_after_first_grant);

    if (!queue->empty())
    {
        queue->add_send_budget(40);
        queue->add_send_budget(40);
        drain_available_slices(scenario.frame_target_bytes);
    }

    ASSERT_TRUE(queue->empty());
    ASSERT_EQ(observed_rows.size(), 10);
    for (std::int64_t row = 0; row < 10; ++row)
    {
        EXPECT_EQ(observed_rows.at(static_cast<std::size_t>(row)), row);
    }

    ASSERT_FALSE(observed_group_end_flags.empty());
    EXPECT_TRUE(observed_group_end_flags.back());
    for (std::size_t slice = 0; slice + 1 < observed_group_end_flags.size(); ++slice)
    {
        EXPECT_FALSE(observed_group_end_flags.at(slice));
    }
}

INSTANTIATE_TEST_SUITE_P(
    CreditAndFrameScenarios,
    LargeBatchBudgetTest,
    testing::Values(LargeBatchBudgetScenario{"no_credit", 0, 24, {}, false},
                    LargeBatchBudgetScenario{"less_than_one_row", 1, 24, {8}, false},
                    LargeBatchBudgetScenario{"budget_limit_plus_crossing_row", 16, 24, {24}, false},
                    LargeBatchBudgetScenario{"frame_limit_plus_crossing_row", 17, 16, {24}, false},
                    LargeBatchBudgetScenario{
                        "credit_spans_multiple_frames", 80, 24, {32, 32, 16}, true}),
    [](const testing::TestParamInfo<LargeBatchBudgetScenario>& info) { return info.param.name; });

} // namespace exasol::udf::v2::internal
