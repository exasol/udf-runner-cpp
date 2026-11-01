#pragma once

#include <exasol/udf/v2/interface/message_view/data_schema_view.hpp>

namespace exasol::udf::v2::internal
{
class DataSchemaViewImpl final : public interface::DataSchemaView
{
public:
    DataSchemaViewImpl(interface::ArrowSchemaPtr schema, bool has_group_id, bool has_row_id);
    interface::ArrowSchemaPtr take_schema() && override;
    [[nodiscard]] bool has_group_id() const noexcept override;
    [[nodiscard]] bool has_row_id() const noexcept override;

private:
    interface::ArrowSchemaPtr schema_;
    bool has_group_id_;
    bool has_row_id_;
};
} // namespace exasol::udf::v2::internal
