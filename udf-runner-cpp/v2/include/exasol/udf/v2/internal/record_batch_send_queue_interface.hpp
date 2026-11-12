#pragma once

#include <arrow/record_batch.h>
#include <arrow/status.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <span>

namespace exasol::udf::v2::internal
{

struct RecordBatchSlice
{
    std::shared_ptr<const arrow::RecordBatch> record_batch;
    std::uint64_t buffer_bytes{};
    bool is_end_of_group{};
};

/// Per-call queue for logical Arrow record batches waiting on peer send credit.
class RecordBatchSendQueueInterface
{
public:
    virtual ~RecordBatchSendQueueInterface() = default;

    RecordBatchSendQueueInterface(const RecordBatchSendQueueInterface&)            = delete;
    RecordBatchSendQueueInterface& operator=(const RecordBatchSendQueueInterface&) = delete;
    RecordBatchSendQueueInterface(RecordBatchSendQueueInterface&&)                 = delete;
    RecordBatchSendQueueInterface& operator=(RecordBatchSendQueueInterface&&)      = delete;

    /// Adds a logical batch with the serialized Arrow buffer bytes for each row.
    ///
    /// The caller supplies row sizes from its serialization or sizing pass. The
    /// queue uses them to choose row-aligned slices without copying the Arrow
    /// buffers. On failure, the queue retains no batch ownership.
    [[nodiscard]] virtual arrow::Status enqueue_batch(
        const std::shared_ptr<const arrow::RecordBatch>& record_batch,
        std::span<const std::uint64_t> row_buffer_bytes,
        bool is_end_of_group) = 0;

    /// Adds a peer-granted byte budget to this call direction.
    virtual void add_send_budget(std::uint32_t budget_bytes) noexcept = 0;

    /// Removes the next row-aligned slice that is allowed by credit and frame target.
    ///
    /// The first logical batch may be emitted without credit. If a row crosses
    /// either limit, that row is included as the single-row oversubscription.
    /// A null result means the queue is empty or a later batch needs more credit.
    [[nodiscard]] virtual std::optional<RecordBatchSlice> dequeue_slice(
        std::uint64_t max_frame_buffer_bytes) = 0;

    [[nodiscard]] virtual bool empty() const noexcept                     = 0;
    [[nodiscard]] virtual std::uint64_t available_budget() const noexcept = 0;

protected:
    RecordBatchSendQueueInterface() = default;
};

} // namespace exasol::udf::v2::internal
