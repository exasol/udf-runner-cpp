#pragma once

#include <exasol/udf/v2/interface/call.hpp>
#include <exasol/udf/v2/interface/control_stream.hpp>
#include <exasol/udf/v2/interface/result.hpp>
#include <exasol/udf/v2/interface/status.hpp>
#include <exasol/udf/v2/interface/timeout.hpp>

#include <memory>

namespace exasol::udf::v2::interface
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
    [[nodiscard]] virtual MessageStatus wait_for_message() = 0;
    /// Waits for activity on any stream, optionally waiting up to `timeout`.
    [[nodiscard]] virtual MessageStatus wait_for_message(Timeout timeout) = 0;
    /// Accepts the next inbound call opening.
    [[nodiscard]] virtual Result<std::unique_ptr<Call>> accept_call() = 0;
    /// Creates a builder for a new outbound call.
    ///
    /// The context allocates and reserves the stream ID. The returned builder
    /// is bound to that stream and must contain an OpenCall field before it
    /// is passed to open_call().
    [[nodiscard]] virtual std::unique_ptr<CallMessageBuilder> create_call_message_builder() = 0;
    /// Opens an outbound call using `opening_message` as its first message.
    [[nodiscard]] virtual Result<std::unique_ptr<Call>> open_call(
        std::unique_ptr<CallMessageBuilder> opening_message) = 0;
    /// Returns the connection-level stream-zero interface.
    [[nodiscard]] virtual ControlStream& control_stream() = 0;

protected:
    Context() = default;
};

} // namespace exasol::udf::v2::interface
