#pragma once

#include <exasol/udf/v2/message_builder.hpp>
#include <exasol/udf/v2/message_view.hpp>

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
    /// Returns the non-blocking readiness state for this call's stream.
    [[nodiscard]] virtual MessageStatus receive_status() const = 0;
    /// Receives the next message for this call, optionally waiting up to `timeout`.
    [[nodiscard]] virtual Result<CallMessageView> receive(Timeout timeout = std::nullopt) = 0;
    /// Sends one composite call message and transfers ownership on success.
    [[nodiscard]] virtual Result<void> send(CallMessageBuilder message) = 0;

protected:
    Call() = default;
};
} // namespace exasol::udf::v2
