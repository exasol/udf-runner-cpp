#pragma once

#if !defined(__linux__)
#error "exasol::udf::v2::LinuxEventFd requires Linux eventfd"
#endif

#include <cstdint>

#include <exasol/udf/v2/event_fd.hpp>

namespace exasol::udf::v2
{

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
