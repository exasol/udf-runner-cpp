#pragma once

#include <exasol/udf/v2/interface/message_builder/control_message_builder.hpp>
#include <exasol/udf/v2/interface/message_view/control_message_view.hpp>
#include <exasol/udf/v2/interface/result.hpp>
#include <exasol/udf/v2/interface/status.hpp>
#include <exasol/udf/v2/interface/timeout.hpp>

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
    ControlStream(ControlStream&&)                 = delete;
    ControlStream& operator=(ControlStream&&)      = delete;
    /// Returns the non-blocking readiness state for stream zero.
    [[nodiscard]] virtual MessageStatus receive_status() const = 0;
    /// Receives the next control message without waiting.
    [[nodiscard]] Result<ControlMessageView> receive()
    {
        return receive(std::nullopt);
    }
    /// Receives the next control message, optionally waiting up to `timeout`.
    [[nodiscard]] virtual Result<ControlMessageView> receive(Timeout timeout) = 0;
    /// Sends one connection-level message and transfers ownership on success.
    // The by-value parameter is intentional: successful implementations consume the builder.
    [[nodiscard]] virtual Result<void> send(ControlMessageBuilder message) = 0; // NOSONAR

protected:
    ControlStream() = default;
};
} // namespace exasol::udf::v2
