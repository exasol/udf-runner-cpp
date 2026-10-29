#include <exasol/udf/v2/event_fd_factory.hpp>

#include <utility>

#include <exasol/udf/v2/linux_event_fd.hpp>

namespace exasol::udf::v2
{

std::unique_ptr<EventFd> make_linux_event_fd()
{
    return std::make_unique<LinuxEventFd>();
}

std::unique_ptr<EventFd> make_linux_event_fd(std::shared_ptr<posix::Posix> posix)
{
    return std::make_unique<LinuxEventFd>(std::move(posix));
}

} // namespace exasol::udf::v2
