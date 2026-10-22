#include <exasol/udf/v2/event_fd_factory.hpp>

#include <exasol/udf/v2/linux_event_fd.hpp>

namespace exasol::udf::v2
{

std::unique_ptr<EventFd> make_linux_event_fd()
{
    return std::make_unique<LinuxEventFd>();
}

std::unique_ptr<EventFd> make_linux_event_fd(posix::Posix& posix)
{
    return std::make_unique<LinuxEventFd>(posix);
}

} // namespace exasol::udf::v2
