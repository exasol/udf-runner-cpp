#include <exasol/udf/v2/internal/record_batch_send_queue_impl.hpp>

#include <algorithm>
#include <limits>

namespace exasol::udf::v2::internal
{

arrow::Status RecordBatchSendQueueImpl::enqueue_batch(
    const std::shared_ptr<const arrow::RecordBatch>& record_batch,
    const std::uint64_t serialized_buffer_bytes,
    const std::uint64_t consumed_budget_bytes,
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

    if (serialized_buffer_bytes != total_buffer_bytes)
    {
        return arrow::Status::Invalid("Serialized batch size must equal the sum of "
                                      "its row buffer byte counts");
    }

    if (consumed_budget_bytes > serialized_buffer_bytes)
    {
        return arrow::Status::Invalid("Consumed budget cannot exceed the serialized batch size");
    }

    if (consumed_budget_bytes > available_budget_)
    {
        return arrow::Status::Invalid("Consumed budget exceeds the available context budget");
    }

    const bool is_first_batch = !first_batch_enqueued_;
    const auto batch_debt =
        is_first_batch ? std::uint64_t{0} : serialized_buffer_bytes - consumed_budget_bytes;

    pending_batches_.push_back(
        {.record_batch           = record_batch,
         .row_buffer_bytes       = {row_buffer_bytes.begin(), row_buffer_bytes.end()},
         .remaining_buffer_bytes = total_buffer_bytes,
         .remaining_batch_budget = consumed_budget_bytes,
         .remaining_debt         = batch_debt,
         .next_row               = 0,
         .is_end_of_group        = is_end_of_group,
         .is_first_batch         = is_first_batch});
    available_budget_ -= consumed_budget_bytes;
    first_batch_enqueued_ = true;
    return arrow::Status::OK();
}

std::uint32_t RecordBatchSendQueueImpl::add_send_budget(const std::uint32_t budget_bytes) noexcept
{
    auto remaining_credit = static_cast<std::uint64_t>(budget_bytes);
    for (auto& pending_batch : pending_batches_)
    {
        const auto debt_paid = std::min(remaining_credit, pending_batch.remaining_debt);
        pending_batch.remaining_debt -= debt_paid;
        pending_batch.remaining_batch_budget += debt_paid;
        remaining_credit -= debt_paid;
        if (remaining_credit == 0)
        {
            break;
        }
    }

    const auto max_budget   = std::numeric_limits<std::uint64_t>::max();
    const auto added_budget = std::min(remaining_credit, max_budget - available_budget_);
    available_budget_ += added_budget;
    return static_cast<std::uint32_t>(added_budget);
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

    const auto slice_limit =
        pending_batch.is_first_batch
            ? max_frame_buffer_bytes
            : std::min(max_frame_buffer_bytes, pending_batch.remaining_batch_budget);
    if (slice_limit == 0 && pending_batch.remaining_buffer_bytes > 0)
    {
        return std::nullopt;
    }
    auto slice_buffer_bytes = std::uint64_t{0};
    auto slice_rows         = std::int64_t{0};
    const auto first_row    = pending_batch.next_row;

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

    const auto consumed_budget = std::min(slice_buffer_bytes, pending_batch.remaining_batch_budget);
    pending_batch.remaining_batch_budget -= consumed_budget;
    if (!pending_batch.is_first_batch)
    {
        const auto forgiven_debt =
            std::min(slice_buffer_bytes - consumed_budget, pending_batch.remaining_debt);
        pending_batch.remaining_debt -= forgiven_debt;
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
