#pragma once

#include <exasol/udf/v2/interface/content_types.hpp>

#include <optional>
#include <utility>

namespace exasol::udf::v2::interface
{

/// Owning view of a received Arrow data schema and its correlation flags.
class DataSchemaView
{
public:
    DataSchemaView() = default;
    explicit DataSchemaView(DataSchema value) : data_schema_(std::move(value))
    {
    }
    DataSchemaView(const DataSchemaView&)                = delete;
    DataSchemaView& operator=(const DataSchemaView&)     = delete;
    DataSchemaView(DataSchemaView&&) noexcept            = default;
    DataSchemaView& operator=(DataSchemaView&&) noexcept = default;
    ~DataSchemaView()                                    = default;

    /// Transfers ownership of the received Arrow schema to the caller.
    [[nodiscard]] ArrowSchemaPtr take_schema() &&
    {
        if (!data_schema_.has_value())
        {
            return {};
        }
        return std::move(data_schema_.value().schema);
    }
    /// Returns whether the schema reserves a group-id field.
    [[nodiscard]] bool has_group_id() const
    {
        if (!data_schema_.has_value())
        {
            return false;
        }
        return data_schema_.value().has_group_id;
    }
    /// Returns whether the schema reserves a row-id field.
    [[nodiscard]] bool has_row_id() const
    {
        if (!data_schema_.has_value())
        {
            return false;
        }
        return data_schema_.value().has_row_id;
    }

private:
    std::optional<DataSchema> data_schema_;
};

} // namespace exasol::udf::v2::interface
