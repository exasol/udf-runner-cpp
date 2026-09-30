#pragma once

#include <exasol/udf/v2/message_builder.hpp>
#include <exasol/udf/v2/message_view.hpp>

#include <utility>

namespace exasol::udf::v2
{
/// Owns one logical non-zero protocol stream.
class Call
{
public:
    /// Destroys the call and releases its protocol resources.
    virtual ~Call();
    Call(const Call&)            = delete;
    Call& operator=(const Call&) = delete;
    Call(Call&&)                 = delete;
    Call& operator=(Call&&)      = delete;
    /// Returns the non-blocking readiness state for this call's stream.
    [[nodiscard]] virtual MessageStatus receive_status() const = 0;
    /// Receives the next message for this call without waiting.
    [[nodiscard]] Result<CallMessageView> receive()
    {
        return receive(std::nullopt);
    }
    /// Receives the next message for this call, optionally waiting up to `timeout`.
    [[nodiscard]] virtual Result<CallMessageView> receive(Timeout timeout) = 0;
    /// Sends one composite call message without waiting for flow-control permission.
    [[nodiscard]] Result<void> send(CallMessageBuilder message)
    {
        return send(std::move(message), std::nullopt);
    }
    /// Sends one composite call message, waiting up to `timeout` for flow-control permission.
    [[nodiscard]] virtual Result<void> send(CallMessageBuilder message, Timeout timeout) = 0;

protected:
    Call() = default;
};
} // namespace exasol::udf::v2
