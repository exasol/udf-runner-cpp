#include <exasol/udf/v2/linux_event_fd.hpp>

#include <sys/eventfd.h>

#include <cerrno>
#include <system_error>
#include <utility>

namespace exasol::udf::v2
{

LinuxEventFd::LinuxEventFd() : LinuxEventFd(posix::default_posix())
{
}

LinuxEventFd::LinuxEventFd(posix::Posix& posix_facade)
    : m_file_descriptor(posix_facade.eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)), m_posix(&posix_facade)
{
    if (m_file_descriptor == -1)
    {
        throw std::system_error(errno, std::generic_category(), "eventfd");
    }
}

LinuxEventFd::~LinuxEventFd()
{
    if (m_file_descriptor != -1)
    {
        m_posix->close(m_file_descriptor);
    }
}

LinuxEventFd::LinuxEventFd(LinuxEventFd&& other) noexcept
    : m_file_descriptor(std::exchange(other.m_file_descriptor, -1)),
      m_posix(std::exchange(other.m_posix, nullptr))
{
}

LinuxEventFd& LinuxEventFd::operator=(LinuxEventFd&& other) noexcept
{
    if (this != &other)
    {
        if (m_file_descriptor != -1)
        {
            m_posix->close(m_file_descriptor);
        }
        m_file_descriptor = std::exchange(other.m_file_descriptor, -1);
        m_posix           = std::exchange(other.m_posix, nullptr);
    }
    return *this;
}

int LinuxEventFd::native_handle() const noexcept
{
    return m_file_descriptor;
}

std::uint64_t LinuxEventFd::read_notification()
{
    std::uint64_t value = 0;
    ssize_t result      = -1;
    do
    {
        result = m_posix->read(m_file_descriptor, &value, sizeof(value));
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
        result = m_posix->write(m_file_descriptor, &signal, sizeof(signal));
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
