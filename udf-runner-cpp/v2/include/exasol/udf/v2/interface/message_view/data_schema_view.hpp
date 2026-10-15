#pragma once

#include <exasol/udf/v2/interface/content_types.hpp>

#include <functional>
#include <optional>

namespace exasol::udf::v2
{

/// Non-owning view of a usable received Arrow data schema and its correlation flags.
class DataSchemaView
{
public:
    DataSchemaView() = default;
    explicit DataSchemaView(const DataSchema& value) : data_schema_(value)
    {
    }
    /// Returns the received Arrow schema pointer, or null when no usable schema is available.
    [[nodiscard]] ArrowSchema* schema() const
    {
        if (!data_schema_.has_value())
        {
            return nullptr;
        }
        return data_schema_.value().get().schema.get();
    }
    /// Returns whether the schema reserves a group-id field.
    [[nodiscard]] bool has_group_id() const
    {
        if (!data_schema_.has_value())
        {
            return false;
        }
        return data_schema_.value().get().has_group_id;
    }
    /// Returns whether the schema reserves a row-id field.
    [[nodiscard]] bool has_row_id() const
    {
        if (!data_schema_.has_value())
        {
            return false;
        }
        return data_schema_.value().get().has_row_id;
    }

private:
    std::optional<std::reference_wrapper<const DataSchema>> data_schema_;
};

} // namespace exasol::udf::v2
