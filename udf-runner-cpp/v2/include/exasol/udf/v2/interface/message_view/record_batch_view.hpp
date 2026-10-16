#pragma once

#include <exasol/udf/v2/interface/content_types.hpp>

#include <optional>
#include <utility>

namespace exasol::udf::v2::interface
{

/// Owning view of a received Arrow record batch.
class RecordBatchView
{
public:
    RecordBatchView() = default;
    explicit RecordBatchView(RecordBatch value) : record_batch_(std::move(value))
    {
    }
    RecordBatchView(const RecordBatchView&)                = delete;
    RecordBatchView& operator=(const RecordBatchView&)     = delete;
    RecordBatchView(RecordBatchView&&) noexcept            = default;
    RecordBatchView& operator=(RecordBatchView&&) noexcept = default;
    ~RecordBatchView()                                     = default;

    /// Transfers ownership of the received Arrow array to the caller.
    [[nodiscard]] ArrowArrayPtr take_array() &&
    {
        if (!record_batch_.has_value())
        {
            return {};
        }
        return std::move(record_batch_.value().array);
    }
    /// Returns whether the final group in this batch ends here.
    [[nodiscard]] bool is_end_of_group() const
    {
        if (!record_batch_.has_value())
        {
            return false;
        }
        return record_batch_.value().is_end_of_group;
    }

private:
    std::optional<RecordBatch> record_batch_;
};

} // namespace exasol::udf::v2::interface
