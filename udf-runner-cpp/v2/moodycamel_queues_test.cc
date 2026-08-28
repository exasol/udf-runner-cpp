#include <cassert>

#include <exasol/udf/v2/mpmc_queue.hpp>
#include <exasol/udf/v2/spsc_queue.hpp>

int main() {
    exasol::udf::v2::SpscQueue<int> spsc;
    assert(spsc.enqueue(7));
    int value = 0;
    assert(spsc.try_dequeue(value));
    assert(value == 7);

    exasol::udf::v2::SpscCircularBuffer<int> circular(2);
    assert(circular.try_enqueue(8));
    assert(circular.try_dequeue(value));
    assert(value == 8);

    exasol::udf::v2::MpmcQueue<int> mpmc;
    assert(mpmc.enqueue(9));
    assert(mpmc.try_dequeue(value));
    assert(value == 9);

    exasol::udf::v2::BlockingMpmcQueue<int> blocking;
    assert(blocking.enqueue(10));
    assert(blocking.try_dequeue(value));
    assert(value == 10);
}
