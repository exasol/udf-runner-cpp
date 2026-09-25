#include <exasol/udf/v2/event_fd.hpp>

#include <sys/eventfd.h>
#include <unistd.h>

#include <cerrno>
#include <system_error>
#include <utility>

namespace exasol::udf::v2
{

EventFd::~EventFd() = default;

LinuxEventFd::LinuxEventFd() : file_descriptor(::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC))
{
    if (file_descriptor == -1)
    {
        throw std::system_error(errno, std::generic_category(), "eventfd");
    }
}

LinuxEventFd::~LinuxEventFd()
{
    if (file_descriptor != -1)
    {
        ::close(file_descriptor);
    }
}

LinuxEventFd::LinuxEventFd(LinuxEventFd&& other) noexcept
    : file_descriptor(std::exchange(other.file_descriptor, -1))
{
}

LinuxEventFd& LinuxEventFd::operator=(LinuxEventFd&& other) noexcept
{
    if (this != &other)
    {
        if (file_descriptor != -1)
        {
            ::close(file_descriptor);
        }
        file_descriptor = std::exchange(other.file_descriptor, -1);
    }
    return *this;
}

int LinuxEventFd::native_handle() const noexcept
{
    return file_descriptor;
}

std::uint64_t LinuxEventFd::read_notification()
{
    std::uint64_t value  = 0;
    const ssize_t result = ::read(file_descriptor, &value, sizeof(value));
    if (result == sizeof(value))
    {
        return value;
    }
    if (result == -1)
    {
        throw std::system_error(errno, std::generic_category(), "read eventfd");
    }
    throw std::system_error(EIO, std::generic_category(), "short read from eventfd");
}

void LinuxEventFd::write_notification()
{
    constexpr std::uint64_t signal = 1;
    const ssize_t result           = ::write(file_descriptor, &signal, sizeof(signal));
    if (result == sizeof(signal))
    {
        return;
    }
    if (result == -1)
    {
        throw std::system_error(errno, std::generic_category(), "write eventfd");
    }
    throw std::system_error(EIO, std::generic_category(), "short write to eventfd");
}

} // namespace exasol::udf::v2
