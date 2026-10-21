#pragma once

#include <exasol/udf/v2/interface/context.hpp>
#include <exasol/udf/v2/context/background_io_thread.hpp>
#include <exasol/udf/v2/socket/owned_file_descriptor.hpp>

#include <memory>

namespace exasol::udf::v2::context
{

/** Constructs one low-level context for each owned connected connection. */
class ContextManager final
{
public:
    ContextManager();
    explicit ContextManager(std::unique_ptr<BackgroundIoThreadFactory> factory);

    ContextManager(const ContextManager&)            = delete;
    ContextManager& operator=(const ContextManager&) = delete;
    ContextManager(ContextManager&&)                 = delete;
    ContextManager& operator=(ContextManager&&)      = delete;

    /** Transfers @p connection ownership into the new context. */
    [[nodiscard]] std::unique_ptr<interface::Context> create_context(
        socket::OwnedFileDescriptor connection);

private:
    std::unique_ptr<BackgroundIoThreadFactory> factory_;
};

} // namespace exasol::udf::v2::context
