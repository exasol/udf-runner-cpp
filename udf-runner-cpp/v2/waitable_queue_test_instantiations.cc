#include "waitable_queue_test_types.hpp"

namespace exasol::udf::v2
{

template class WaitableQueue<SpscQueue<int>>;
template class WaitableQueue<MpmcQueue<int>>;

} // namespace exasol::udf::v2
