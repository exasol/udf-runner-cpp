#pragma once

#include <exasol/udf/v2/interface/message_builder/call_message_builder.hpp>
#include <exasol/udf/v2/interface/message_view/call_message_view.hpp>
#include <exasol/udf/v2/interface/result.hpp>
#include <exasol/udf/v2/interface/status.hpp>
#include <exasol/udf/v2/interface/timeout.hpp>

#include <memory>
#include <utility>

namespace exasol::udf::v2::interface
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
    /// Creates a builder bound to this call's stream.
    [[nodiscard]] virtual std::unique_ptr<CallMessageBuilder> create_message_builder() const = 0;
    /// Receives the next message for this call without waiting.
    [[nodiscard]] virtual Result<std::unique_ptr<CallMessageView>> receive() = 0;
    /// Receives the next message for this call, optionally waiting up to `timeout`.
    [[nodiscard]] virtual Result<std::unique_ptr<CallMessageView>> receive(Timeout timeout) = 0;
    /// Sends one composite call message without waiting for flow-control permission.
    [[nodiscard]] virtual Result<void> send(std::unique_ptr<CallMessageBuilder> message) = 0;
    /// Sends one composite call message, waiting up to `timeout` for flow-control permission.
    [[nodiscard]] virtual Result<void> send(std::unique_ptr<CallMessageBuilder> message,
                                            Timeout timeout) = 0;

protected:
    Call() = default;
};
} // namespace exasol::udf::v2::interface
