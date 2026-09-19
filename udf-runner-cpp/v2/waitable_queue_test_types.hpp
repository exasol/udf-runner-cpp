#pragma once

#include <exasol/udf/v2/waitable_queue.hpp>

namespace exasol::udf::v2
{

extern template class WaitableQueue<SpscQueue<int>>;
extern template class WaitableQueue<MpmcQueue<int>>;

} // namespace exasol::udf::v2
