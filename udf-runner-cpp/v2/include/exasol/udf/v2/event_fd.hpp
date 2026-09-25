#pragma once

#include <cstdint>

namespace exasol::udf::v2
{

// Adds an epoll-compatible readiness descriptor to a queue. The descriptor
// signals that one or more queue elements may be available; it is not a
// one-to-one mapping between eventfd counter values and queue elements.
class EventFd
{
public:
    EventFd() = default;
    virtual ~EventFd() = default;

    EventFd(const EventFd&)            = delete;
    EventFd& operator=(const EventFd&) = delete;
    EventFd(EventFd&&)                 = default;
    EventFd& operator=(EventFd&&)      = default;

    [[nodiscard]] virtual int native_handle() const noexcept = 0;
    virtual std::uint64_t read_notification()                = 0;
    virtual void write_notification()                        = 0;
};

} // namespace exasol::udf::v2
