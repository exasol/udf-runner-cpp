#pragma once

#include <exasol/udf/v2/interface/arrow_c_data.hpp>

#include <utility>

namespace exasol::udf::v2::interface
{

/// Arrow record batch and its group-boundary marker.
/// Owning view of a received Arrow record batch.
class RecordBatchView
{
public:
    RecordBatchView() = default;
    RecordBatchView(ArrowArrayPtr array, bool is_end_of_group)
        : array_(std::move(array)), is_end_of_group_(is_end_of_group)
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
        return std::move(array_);
    }
    /// Returns whether the final group in this batch ends here.
    [[nodiscard]] bool is_end_of_group() const
    {
        return is_end_of_group_;
    }

private:
    ArrowArrayPtr array_;
    bool is_end_of_group_{false};
};

} // namespace exasol::udf::v2::interface
