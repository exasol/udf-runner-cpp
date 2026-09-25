#pragma once

#include <memory>
#include <utility>

#include <exasol/udf/v2/event_fd_factory.hpp>
#include <exasol/udf/v2/mpmc_queue.hpp>
#include <exasol/udf/v2/spsc_queue.hpp>
#include <exasol/udf/v2/waitable_queue.hpp>

namespace exasol::udf::v2
{

template <typename Queue>
class LinuxWaitableQueue : public WaitableQueue<Queue>
{
    using Base = WaitableQueue<Queue>;

public:
    LinuxWaitableQueue() : Base(Queue{}, make_linux_event_fd())
    {
    }

    explicit LinuxWaitableQueue(Queue queue)
        : Base(std::move(queue), make_linux_event_fd())
    {
    }

    LinuxWaitableQueue(Queue queue, std::unique_ptr<EventFd> event_fd)
        : Base(std::move(queue), std::move(event_fd))
    {
    }

    LinuxWaitableQueue(LinuxWaitableQueue&&) noexcept = default;
    LinuxWaitableQueue& operator=(LinuxWaitableQueue&&) noexcept = default;
};

template <typename T>
using WaitableSpscQueue = LinuxWaitableQueue<SpscQueue<T>>;

template <typename T>
using WaitableMpmcQueue = LinuxWaitableQueue<MpmcQueue<T>>;

} // namespace exasol::udf::v2
