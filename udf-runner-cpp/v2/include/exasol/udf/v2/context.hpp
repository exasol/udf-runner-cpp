#pragma once

#include <exasol/udf/v2/call.hpp>
#include <exasol/udf/v2/control_stream.hpp>

namespace exasol::udf::v2
{

class Context
{
public:
    /// Destroys the connection context and releases its transport resources.
    virtual ~Context();
    Context(const Context&)            = delete;
    Context& operator=(const Context&) = delete;
    /// Returns whether any stream currently has an unread message.
    [[nodiscard]] virtual MessageStatus receive_status() const = 0;
    /// Waits for activity on any stream without consuming a message.
    [[nodiscard]] virtual MessageStatus wait_for_message(Timeout timeout = std::nullopt) = 0;
    /// Accepts the next inbound call opening.
    [[nodiscard]] virtual Result<std::unique_ptr<Call>> accept_call() = 0;
    /// Opens an outbound call using `opening_message` as its first message.
    [[nodiscard]] virtual Result<std::unique_ptr<Call>> open_call(
        CallMessageBuilder opening_message) = 0;
    /// Returns the connection-level stream-zero interface.
    [[nodiscard]] virtual ControlStream& control_stream() = 0;

protected:
    Context() = default;
};

} // namespace exasol::udf::v2
