#include <exasol/udf/v2/linux_event_fd.hpp>

#include <sys/eventfd.h>

#include <cerrno>
#include <system_error>
#include <utility>

namespace exasol::udf::v2
{

LinuxEventFd::LinuxEventFd() : LinuxEventFd(std::make_shared<posix::Native>())
{
}

LinuxEventFd::LinuxEventFd(std::shared_ptr<posix::Posix> posix_facade)
    : file_descriptor(posix_facade->eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)),
      posix(std::move(posix_facade))
{
    if (this->file_descriptor == -1)
    {
        throw std::system_error(errno, std::generic_category(), "eventfd");
    }
}

LinuxEventFd::~LinuxEventFd()
{
    if (this->file_descriptor != -1)
    {
        this->posix->close(this->file_descriptor);
    }
}

LinuxEventFd::LinuxEventFd(LinuxEventFd&& other) noexcept
    : file_descriptor(std::exchange(other.file_descriptor, -1)), posix(std::move(other.posix))
{
}

LinuxEventFd& LinuxEventFd::operator=(LinuxEventFd&& other) noexcept
{
    if (this != &other)
    {
        if (this->file_descriptor != -1)
        {
            this->posix->close(this->file_descriptor);
        }
        this->file_descriptor = std::exchange(other.file_descriptor, -1);
        this->posix           = std::move(other.posix);
    }
    return *this;
}

int LinuxEventFd::native_handle() const noexcept
{
    return this->file_descriptor;
}

std::uint64_t LinuxEventFd::read_notification()
{
    std::uint64_t value = 0;
    ssize_t result      = -1;
    do
    {
        result = this->posix->read(this->file_descriptor, &value, sizeof(value));
    } while (result == -1 && errno == EINTR);

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
    ssize_t result                 = -1;
    do
    {
        result = this->posix->write(this->file_descriptor, &signal, sizeof(signal));
    } while (result == -1 && errno == EINTR);

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
