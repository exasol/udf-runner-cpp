#pragma once

#include <memory>

#include <exasol/udf/v2/event_fd.hpp>
#include <exasol/udf/v2/posix/posix.hpp>

namespace exasol::udf::v2
{

std::unique_ptr<EventFd> make_linux_event_fd();
std::unique_ptr<EventFd> make_linux_event_fd(std::shared_ptr<posix::Posix> posix);

} // namespace exasol::udf::v2
