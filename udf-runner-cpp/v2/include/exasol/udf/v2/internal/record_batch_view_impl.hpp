#pragma once

#include <exasol/udf/v2/interface/message_view/record_batch_view.hpp>

namespace exasol::udf::v2::internal
{
class RecordBatchViewImpl final : public interface::RecordBatchView
{
public:
    RecordBatchViewImpl(interface::ArrowArrayPtr array, bool is_end_of_group);
    interface::ArrowArrayPtr take_array() && override;
    bool is_end_of_group() const noexcept override;
private:
    interface::ArrowArrayPtr array_;
    bool is_end_of_group_;
};
}
