#include <exasol/udf/v2/internal/record_batch_send_queue_impl.hpp>

#include <algorithm>
#include <limits>

namespace exasol::udf::v2::internal
{

arrow::Status RecordBatchSendQueueImpl::enqueue_batch(
    const std::shared_ptr<const arrow::RecordBatch>& record_batch,
    const std::span<const std::uint64_t> row_buffer_bytes,
    const bool is_end_of_group)
{
    if (!record_batch)
    {
        return arrow::Status::Invalid("Cannot enqueue a null record batch");
    }

    if (row_buffer_bytes.size() != static_cast<std::uint64_t>(record_batch->num_rows()))
    {
        return arrow::Status::Invalid(
            "Row buffer byte counts must match the record batch row count");
    }

    std::uint64_t total_buffer_bytes = 0;
    for (const auto row_buffer_bytes_value : row_buffer_bytes)
    {
        if (row_buffer_bytes_value > std::numeric_limits<std::uint64_t>::max() - total_buffer_bytes)
        {
            return arrow::Status::Invalid("Record batch row buffer byte count overflows uint64_t");
        }
        total_buffer_bytes += row_buffer_bytes_value;
    }

    pending_batches_.push_back(
        {.record_batch           = record_batch,
         .row_buffer_bytes       = {row_buffer_bytes.begin(), row_buffer_bytes.end()},
         .remaining_buffer_bytes = total_buffer_bytes,
         .next_row               = 0,
         .is_end_of_group        = is_end_of_group,
         .is_first_batch         = !first_batch_enqueued_});
    first_batch_enqueued_ = true;
    return arrow::Status::OK();
}

void RecordBatchSendQueueImpl::add_send_budget(const std::uint32_t budget_bytes) noexcept
{
    const auto budget_bytes_wide = static_cast<std::uint64_t>(budget_bytes);
    const auto max_budget        = std::numeric_limits<std::uint64_t>::max();
    if (budget_bytes_wide > max_budget - available_budget_)
    {
        available_budget_ = max_budget;
        return;
    }
    available_budget_ += budget_bytes_wide;
}

std::optional<RecordBatchSlice> RecordBatchSendQueueImpl::dequeue_slice(
    const std::uint64_t max_frame_buffer_bytes)
{
    if (pending_batches_.empty() || max_frame_buffer_bytes == 0)
    {
        return std::nullopt;
    }

    auto& pending_batch = pending_batches_.front();
    const auto num_rows = pending_batch.record_batch->num_rows();
    if (num_rows == 0)
    {
        const auto record_batch_slice = pending_batch.record_batch->Slice(0, 0);
        const auto is_end_of_group    = pending_batch.is_end_of_group;
        pending_batches_.pop_front();
        return RecordBatchSlice{.record_batch    = record_batch_slice,
                                .buffer_bytes    = 0,
                                .is_end_of_group = is_end_of_group};
    }

    if (available_budget_ == 0 && !pending_batch.is_first_batch &&
        pending_batch.remaining_buffer_bytes > 0)
    {
        return std::nullopt;
    }

    const bool charge_budget = !pending_batch.is_first_batch && available_budget_ > 0;
    const auto slice_limit   = charge_budget ? std::min(max_frame_buffer_bytes, available_budget_)
                                             : max_frame_buffer_bytes;
    auto slice_buffer_bytes  = std::uint64_t{0};
    auto slice_rows          = std::int64_t{0};
    const auto first_row     = pending_batch.next_row;

    while (pending_batch.next_row + slice_rows < num_rows)
    {
        const auto row_index = static_cast<std::size_t>(pending_batch.next_row + slice_rows);
        const auto row_bytes = pending_batch.row_buffer_bytes.at(row_index);
        if (row_bytes > slice_limit - slice_buffer_bytes)
        {
            // Include the row that crosses the budget or frame target. It is
            // indivisible, so this is the only permitted oversubscription.
            slice_buffer_bytes += row_bytes;
            ++slice_rows;
            break;
        }

        slice_buffer_bytes += row_bytes;
        ++slice_rows;
    }

    pending_batch.next_row += slice_rows;
    pending_batch.remaining_buffer_bytes -= slice_buffer_bytes;
    const bool is_final_slice  = pending_batch.next_row == num_rows;
    const bool is_end_of_group = is_final_slice && pending_batch.is_end_of_group;
    auto record_batch_slice    = pending_batch.record_batch->Slice(first_row, slice_rows);

    if (charge_budget)
    {
        available_budget_ -= std::min(slice_buffer_bytes, available_budget_);
    }

    if (is_final_slice)
    {
        pending_batches_.pop_front();
    }

    return RecordBatchSlice{.record_batch    = std::move(record_batch_slice),
                            .buffer_bytes    = slice_buffer_bytes,
                            .is_end_of_group = is_end_of_group};
}

bool RecordBatchSendQueueImpl::empty() const noexcept
{
    return pending_batches_.empty();
}

std::uint64_t RecordBatchSendQueueImpl::available_budget() const noexcept
{
    return available_budget_;
}

} // namespace exasol::udf::v2::internal
