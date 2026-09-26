#pragma once

#include <memory>

#include <exasol/udf/v2/event_fd.hpp>

namespace exasol::udf::v2
{

std::unique_ptr<EventFd> make_linux_event_fd();

} // namespace exasol::udf::v2
