#pragma once

#include <exasol/udf/v2/message_builder.hpp>
#include <exasol/udf/v2/message_view.hpp>

namespace exasol::udf::v2
{
/// Provides access to connection-level traffic on protocol stream zero.
class ControlStream
{
public:
    /// Destroys the control-stream handle.
    virtual ~ControlStream();
    ControlStream(const ControlStream&)            = delete;
    ControlStream& operator=(const ControlStream&) = delete;
    /// Returns the non-blocking readiness state for stream zero.
    [[nodiscard]] virtual MessageStatus receive_status() const = 0;
    /// Receives the next control message, optionally waiting up to `timeout`.
    [[nodiscard]] virtual Result<ControlMessageView> receive(Timeout timeout = std::nullopt) = 0;
    /// Sends one connection-level message and transfers ownership on success.
    [[nodiscard]] virtual Result<void> send(ControlMessageBuilder message) = 0;

protected:
    ControlStream() = default;
};
} // namespace exasol::udf::v2
