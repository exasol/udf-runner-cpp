#pragma once

#include <exasol/udf/v2/types.hpp>

namespace exasol::udf::v2
{

/// Non-owning view of a received Arrow record batch.
class RecordBatchView
{
public:
    explicit RecordBatchView(const RecordBatch* value = nullptr) : value_(value)
    {
    }
    /// Returns the received Arrow array pointer, or null when absent.
    [[nodiscard]] ArrowArray* array() const noexcept
    {
        return value_ == nullptr ? nullptr : value_->array;
    }
    /// Returns whether the final group in this batch ends here.
    [[nodiscard]] bool is_end_of_group() const noexcept
    {
        return value_ != nullptr && value_->is_end_of_group;
    }

private:
    const RecordBatch* value_;
};

} // namespace exasol::udf::v2
