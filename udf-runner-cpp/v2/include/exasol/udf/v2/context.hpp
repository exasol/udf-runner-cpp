#pragma once

#include <exasol/udf/v2/call.hpp>
#include <exasol/udf/v2/control_stream.hpp>

namespace exasol::udf::v2
{

/// Provides connection-wide readiness and lifecycle operations for worker calls.
class Context
{
public:
    /// Destroys the connection context and releases its transport resources.
    virtual ~Context();
    Context(const Context&)            = delete;
    Context& operator=(const Context&) = delete;
    Context(Context&&)                 = delete;
    Context& operator=(Context&&)      = delete;
    /// Returns whether any stream currently has an unread message.
    [[nodiscard]] virtual MessageStatus receive_status() const = 0;
    /// Returns whether a not-yet-accepted inbound call is available.
    [[nodiscard]] virtual bool is_new_inbound_call_available() const = 0;
    /// Checks for activity on any stream without waiting.
    [[nodiscard]] MessageStatus wait_for_message()
    {
        return wait_for_message(std::nullopt);
    }
    /// Waits for activity on any stream, optionally waiting up to `timeout`.
    [[nodiscard]] virtual MessageStatus wait_for_message(Timeout timeout) = 0;
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
