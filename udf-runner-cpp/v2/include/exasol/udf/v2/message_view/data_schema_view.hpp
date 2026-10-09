#pragma once

#include <exasol/udf/v2/types.hpp>

#include <functional>
#include <optional>

namespace exasol::udf::v2
{

/// Non-owning view of a received Arrow data schema and its correlation flags.
class DataSchemaView
{
public:
    DataSchemaView() = default;
    explicit DataSchemaView(const DataSchema& value) : value_(value)
    {
    }
    /// Returns the received Arrow schema pointer, or null when absent.
    [[nodiscard]] ArrowSchema* schema() const noexcept
    {
        return value_ ? value_->get().schema : nullptr;
    }
    /// Returns whether the schema reserves a group-id field.
    [[nodiscard]] bool has_group_id() const noexcept
    {
        return value_ && value_->get().has_group_id;
    }
    /// Returns whether the schema reserves a row-id field.
    [[nodiscard]] bool has_row_id() const noexcept
    {
        return value_ && value_->get().has_row_id;
    }

private:
    std::optional<std::reference_wrapper<const DataSchema>> value_;
};

} // namespace exasol::udf::v2
