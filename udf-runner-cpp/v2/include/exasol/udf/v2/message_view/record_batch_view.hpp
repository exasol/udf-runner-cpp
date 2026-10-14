#pragma once

#include <exasol/udf/v2/content_types.hpp>

#include <functional>
#include <optional>

namespace exasol::udf::v2
{

/// Non-owning view of a received Arrow record batch.
class RecordBatchView
{
public:
    RecordBatchView() = default;
    explicit RecordBatchView(const RecordBatch& value) : record_batch_(value)
    {
    }
    /// Returns the received Arrow array pointer, or null when absent.
    [[nodiscard]] ArrowArray* array() const noexcept
    {
        return record_batch_.has_value() ? record_batch_->get().array : nullptr;
    }
    /// Returns whether the final group in this batch ends here.
    [[nodiscard]] bool is_end_of_group() const noexcept
    {
        return record_batch_.has_value() && record_batch_->get().is_end_of_group;
    }

private:
    std::optional<std::reference_wrapper<const RecordBatch>> record_batch_;
};

} // namespace exasol::udf::v2
