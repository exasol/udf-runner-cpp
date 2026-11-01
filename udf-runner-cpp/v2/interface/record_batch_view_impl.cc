#include <exasol/udf/v2/internal/record_batch_view_impl.hpp>
#include <utility>
namespace exasol::udf::v2::internal
{
RecordBatchViewImpl::RecordBatchViewImpl(interface::ArrowArrayPtr array, bool is_end_of_group)
    : array_(std::move(array)), is_end_of_group_(is_end_of_group)
{
} // namespace exasol::udf::v2::internal
interface::ArrowArrayPtr RecordBatchViewImpl::take_array() &&
{
    return std::move(array_);
}
bool RecordBatchViewImpl::is_end_of_group() const noexcept
{
    return is_end_of_group_;
}
} // namespace exasol::udf::v2::internal
