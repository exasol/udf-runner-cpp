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
    virtual ~RecordBatchView() = default;
    RecordBatchView(const RecordBatchView&) = delete;
    RecordBatchView& operator=(const RecordBatchView&) = delete;
    RecordBatchView(RecordBatchView&&) = delete;
    RecordBatchView& operator=(RecordBatchView&&) = delete;

    /// Transfers ownership of the received Arrow array to the caller.
    [[nodiscard]] virtual ArrowArrayPtr take_array() && = 0;
    /// Returns whether the final group in this batch ends here.
    [[nodiscard]] virtual bool is_end_of_group() const noexcept = 0;

protected:
    RecordBatchView() = default;
};

} // namespace exasol::udf::v2::interface
