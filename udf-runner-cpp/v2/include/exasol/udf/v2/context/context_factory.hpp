#pragma once

#include <exasol/udf/v2/interface/context.hpp>
#include <exasol/udf/v2/context/background_io_thread.hpp>
#include <exasol/udf/v2/context/context_manager.hpp>
#include <exasol/udf/v2/socket/owned_file_descriptor.hpp>

#include <memory>

namespace exasol::udf::v2
{

/** Creates a context backed by the production background I/O thread. */
[[nodiscard]] std::unique_ptr<interface::Context> create_context(
    socket::OwnedFileDescriptor connection);

/** Creates a context with an injected background-thread factory. */
[[nodiscard]] std::unique_ptr<interface::Context> create_context(
    socket::OwnedFileDescriptor connection,
    std::unique_ptr<context::BackgroundIoThreadFactory> background_thread_factory);

} // namespace exasol::udf::v2
