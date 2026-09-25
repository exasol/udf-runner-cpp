#pragma once

#include <utility>

#include <exasol/udf/v2/event_fd_factory.hpp>
#include <exasol/udf/v2/mpmc_queue.hpp>
#include <exasol/udf/v2/spsc_queue.hpp>
#include <exasol/udf/v2/waitable_queue.hpp>

namespace exasol::udf::v2
{

template <typename T>
using WaitableSpscQueue = WaitableQueue<SpscQueue<T>>;

template <typename T>
using WaitableMpmcQueue = WaitableQueue<MpmcQueue<T>>;

template <typename T>
[[nodiscard]] WaitableSpscQueue<T> make_waitable_spsc_queue()
{
    return WaitableSpscQueue<T>(SpscQueue<T>{}, make_linux_event_fd());
}

template <typename T>
[[nodiscard]] WaitableSpscQueue<T> make_waitable_spsc_queue(SpscQueue<T> queue)
{
    return WaitableSpscQueue<T>(std::move(queue), make_linux_event_fd());
}

template <typename T>
[[nodiscard]] WaitableMpmcQueue<T> make_waitable_mpmc_queue()
{
    return WaitableMpmcQueue<T>(MpmcQueue<T>{}, make_linux_event_fd());
}

template <typename T>
[[nodiscard]] WaitableMpmcQueue<T> make_waitable_mpmc_queue(MpmcQueue<T> queue)
{
    return WaitableMpmcQueue<T>(std::move(queue), make_linux_event_fd());
}

} // namespace exasol::udf::v2
