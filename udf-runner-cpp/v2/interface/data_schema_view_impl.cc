#include <exasol/udf/v2/internal/data_schema_view_impl.hpp>
#include <utility>
namespace exasol::udf::v2::internal
{
DataSchemaViewImpl::DataSchemaViewImpl(interface::ArrowSchemaPtr schema,
                                       bool has_group_id,
                                       bool has_row_id)
    : schema_(std::move(schema)), has_group_id_(has_group_id), has_row_id_(has_row_id)
{
} // namespace exasol::udf::v2::internal
interface::ArrowSchemaPtr DataSchemaViewImpl::take_schema() &&
{
    return std::move(schema_);
}
bool DataSchemaViewImpl::has_group_id() const noexcept
{
    return has_group_id_;
}
bool DataSchemaViewImpl::has_row_id() const noexcept
{
    return has_row_id_;
}
} // namespace exasol::udf::v2::internal
