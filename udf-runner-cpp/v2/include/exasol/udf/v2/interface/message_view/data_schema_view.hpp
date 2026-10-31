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
    virtual ~DataSchemaView()                        = default;
    DataSchemaView(const DataSchemaView&)            = delete;
    DataSchemaView& operator=(const DataSchemaView&) = delete;
    DataSchemaView(DataSchemaView&&)                 = delete;
    DataSchemaView& operator=(DataSchemaView&&)      = delete;

    /// Transfers ownership of the received Arrow schema to the caller.
    [[nodiscard]] virtual ArrowSchemaPtr take_schema() && = 0;
    /// Returns whether the schema reserves a group-id field.
    [[nodiscard]] virtual bool has_group_id() const noexcept = 0;
    /// Returns whether the schema reserves a row-id field.
    [[nodiscard]] virtual bool has_row_id() const noexcept = 0;

protected:
    DataSchemaView() = default;
};

} // namespace exasol::udf::v2::interface
