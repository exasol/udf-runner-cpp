#pragma once

#include <exasol/udf/v2/interface/message_builder/control_message_builder.hpp>
#include <exasol/udf/v2/interface/message_view/control_message_view.hpp>
#include <exasol/udf/v2/interface/result.hpp>
#include <exasol/udf/v2/interface/status.hpp>
#include <exasol/udf/v2/interface/timeout.hpp>

#include <memory>

namespace exasol::udf::v2::interface
{
/// Provides access to connection-level traffic on protocol stream zero.
class ControlStream
{
public:
    /// Destroys the control-stream handle.
    virtual ~ControlStream();
    ControlStream(const ControlStream&)            = delete;
    ControlStream& operator=(const ControlStream&) = delete;
    ControlStream(ControlStream&&)                 = delete;
    ControlStream& operator=(ControlStream&&)      = delete;
    /// Returns the non-blocking readiness state for stream zero.
    [[nodiscard]] virtual MessageStatus receive_status() const = 0;
    /// Creates a builder bound to control stream zero.
    [[nodiscard]] virtual std::unique_ptr<ControlMessageBuilder> create_message_builder() const = 0;
    /// Receives the next control message without waiting.
    [[nodiscard]] virtual Result<std::unique_ptr<ControlMessageView>> receive() = 0;
    /// Receives the next control message, optionally waiting up to `timeout`.
    [[nodiscard]] virtual Result<std::unique_ptr<ControlMessageView>> receive(Timeout timeout) = 0;
    /// Sends one connection-level message and transfers its builder to the stream.
    [[nodiscard]] virtual Result<void> send(std::unique_ptr<ControlMessageBuilder> message) = 0;

protected:
    ControlStream() = default;
};
} // namespace exasol::udf::v2::interface
