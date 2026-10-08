#pragma once

#include <exasol/udf/v2/types.hpp>

namespace exasol::udf::v2
{

/// Non-owning view of a received Arrow data schema and its correlation flags.
class DataSchemaView
{
public:
    explicit DataSchemaView(const DataSchema* value = nullptr) : value_(value)
    {
    }
    /// Returns the received Arrow schema pointer, or null when absent.
    [[nodiscard]] ArrowSchema* schema() const noexcept
    {
        return value_ == nullptr ? nullptr : value_->schema;
    }
    /// Returns whether the schema reserves a group-id field.
    [[nodiscard]] bool has_group_id() const noexcept
    {
        return value_ != nullptr && value_->has_group_id;
    }
    /// Returns whether the schema reserves a row-id field.
    [[nodiscard]] bool has_row_id() const noexcept
    {
        return value_ != nullptr && value_->has_row_id;
    }

private:
    const DataSchema* value_;
};

} // namespace exasol::udf::v2
