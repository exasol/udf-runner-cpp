#pragma once

#include <exasol/udf/v2/interface/interface.hpp>

namespace exasol::udf::v2::test
{

class CallDouble final : public Call
{
public:
    using Call::receive;
    using Call::send;

    [[nodiscard]] MessageStatus receive_status() const override
    {
        return MessageStatus::MessageAvailable;
    }

    [[nodiscard]] Result<CallMessageView> receive(Timeout timeout) override
    {
        (void)timeout;
        return Result<CallMessageView>::success(CallMessageView{});
    }

    [[nodiscard]] Result<void> send(CallMessageBuilder message, Timeout timeout) override
    {
        (void)message;
        (void)timeout;
        return Result<void>::success();
    }
};

class ControlStreamDouble final : public ControlStream
{
public:
    using ControlStream::receive;

    [[nodiscard]] MessageStatus receive_status() const override
    {
        return MessageStatus::NoMessage;
    }

    [[nodiscard]] Result<ControlMessageView> receive(Timeout timeout) override
    {
        (void)timeout;
        return Result<ControlMessageView>::success(ControlMessageView{});
    }

    [[nodiscard]] Result<void> send(ControlMessageBuilder message) override // NOSONAR
    {
        (void)message;
        return Result<void>::success();
    }
};

class ContextDouble final : public Context
{
public:
    using Context::wait_for_message;

    [[nodiscard]] MessageStatus receive_status() const override
    {
        return MessageStatus::NoMessage;
    }

    [[nodiscard]] bool is_new_inbound_call_available() const override
    {
        return false;
    }

    [[nodiscard]] MessageStatus wait_for_message(Timeout timeout) override
    {
        (void)timeout;
        return MessageStatus::TimedOut;
    }

    [[nodiscard]] Result<std::unique_ptr<Call>> accept_call() override
    {
        return Result<std::unique_ptr<Call>>::failure(OperationStatus::Cancelled);
    }

    [[nodiscard]] Result<std::unique_ptr<Call>> open_call(
        CallMessageBuilder opening_message) override
    {
        (void)opening_message;
        return Result<std::unique_ptr<Call>>::failure(OperationStatus::Cancelled);
    }

    [[nodiscard]] ControlStream& control_stream() override
    {
        return control_stream_;
    }

private:
    ControlStreamDouble control_stream_;
};

} // namespace exasol::udf::v2::test
