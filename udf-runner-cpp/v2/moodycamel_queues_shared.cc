#include <exasol/udf/v2/mpmc_queue.hpp>
#include <exasol/udf/v2/spsc_queue.hpp>

extern "C" void exasol_udf_v2_moodycamel_queue_anchor() {
    exasol::udf::v2::SpscQueue<int> spsc;
    spsc.enqueue(1);

    exasol::udf::v2::MpmcQueue<int> mpmc;
    mpmc.enqueue(2);
}
