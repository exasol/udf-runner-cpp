#pragma once

#include <exasol/udf/v2/internal/record_batch_send_queue_interface.hpp>

#include <deque>
#include <vector>

namespace exasol::udf::v2::internal
{

/// FIFO, byte-budgeted queue that returns zero-copy Arrow row slices.
class RecordBatchSendQueueImpl final : public RecordBatchSendQueueInterface
{
public:
    [[nodiscard]] arrow::Status enqueue_batch(
        const std::shared_ptr<const arrow::RecordBatch>& record_batch,
        std::span<const std::uint64_t> row_buffer_bytes,
        bool is_end_of_group) override;
    void add_send_budget(std::uint32_t budget_bytes) noexcept override;
    [[nodiscard]] std::optional<RecordBatchSlice> dequeue_slice(
        std::uint64_t max_frame_buffer_bytes) override;
    [[nodiscard]] bool empty() const noexcept override;
    [[nodiscard]] std::uint64_t available_budget() const noexcept override;

private:
    struct PendingBatch
    {
        std::shared_ptr<const arrow::RecordBatch> record_batch;
        std::vector<std::uint64_t> row_buffer_bytes;
        std::uint64_t remaining_buffer_bytes{};
        std::int64_t next_row{};
        bool is_end_of_group{};
        bool is_first_batch{};
    };

    std::deque<PendingBatch> pending_batches_;
    std::uint64_t available_budget_{};
    bool first_batch_enqueued_{};
};

} // namespace exasol::udf::v2::internal
