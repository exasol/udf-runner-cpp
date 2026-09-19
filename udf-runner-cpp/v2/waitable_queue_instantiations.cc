#include <exasol/udf/v2/waitable_queue.hpp>

namespace exasol::udf::v2
{

template class WaitableQueue<SpscQueue<int>>;
template class WaitableQueue<MpmcQueue<int>>;

} // namespace exasol::udf::v2
