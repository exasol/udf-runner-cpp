#pragma once

#if !defined(__linux__)
#error "exasol::udf::v2::LinuxEventFd requires Linux eventfd"
#endif

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
    virtual ~EventFd();

    EventFd(const EventFd&)            = delete;
    EventFd& operator=(const EventFd&) = delete;
    EventFd(EventFd&&)                 = default;
    EventFd& operator=(EventFd&&)      = default;

    [[nodiscard]] virtual int native_handle() const noexcept = 0;
    virtual std::uint64_t read_notification()                = 0;
    virtual void write_notification()                        = 0;
};

class LinuxEventFd final : public EventFd
{
public:
    LinuxEventFd();
    ~LinuxEventFd() override;

    LinuxEventFd(const LinuxEventFd&)            = delete;
    LinuxEventFd& operator=(const LinuxEventFd&) = delete;
    LinuxEventFd(LinuxEventFd&& other) noexcept;
    LinuxEventFd& operator=(LinuxEventFd&& other) noexcept;

    [[nodiscard]] int native_handle() const noexcept override;
    std::uint64_t read_notification() override;
    void write_notification() override;

private:
    int file_descriptor = -1;
};

} // namespace exasol::udf::v2
