#pragma once

#if !defined(__linux__)
#error "exasol::udf::v2::LinuxEventFd requires Linux eventfd"
#endif

#include <cstdint>
#include <memory>

#include <exasol/udf/v2/event_fd.hpp>
#include <exasol/udf/v2/posix/posix.hpp>

namespace exasol::udf::v2
{

class LinuxEventFd final : public EventFd
{
public:
    LinuxEventFd();
    explicit LinuxEventFd(posix::Posix& posix_facade);
    ~LinuxEventFd() override;

    LinuxEventFd(const LinuxEventFd&)            = delete;
    LinuxEventFd& operator=(const LinuxEventFd&) = delete;
    LinuxEventFd(LinuxEventFd&& other) noexcept;
    LinuxEventFd& operator=(LinuxEventFd&& other) noexcept;

    [[nodiscard]] int native_handle() const noexcept override;
    std::uint64_t read_notification() override;
    void write_notification() override;

private:
    std::unique_ptr<posix::Native> m_owned_posix;
    int m_file_descriptor = -1;
    posix::Posix* m_posix = nullptr;
};

} // namespace exasol::udf::v2
