#pragma once

#include <exasol/udf/v2/interface/arrow_c_data.hpp>

#include <utility>

namespace exasol::udf::v2::interface
{

/// Arrow schema and correlation-field metadata for one data direction.
/// Owning view of a received Arrow data schema and its correlation flags.
class DataSchemaView
{
public:
    DataSchemaView() = default;
    DataSchemaView(ArrowSchemaPtr schema, bool has_group_id, bool has_row_id)
        : schema_(std::move(schema)), has_group_id_(has_group_id), has_row_id_(has_row_id)
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
        return std::move(schema_);
    }
    /// Returns whether the schema reserves a group-id field.
    [[nodiscard]] bool has_group_id() const
    {
        return has_group_id_;
    }
    /// Returns whether the schema reserves a row-id field.
    [[nodiscard]] bool has_row_id() const
    {
        return has_row_id_;
    }

private:
    ArrowSchemaPtr schema_;
    bool has_group_id_{false};
    bool has_row_id_{false};
};

} // namespace exasol::udf::v2::interface
