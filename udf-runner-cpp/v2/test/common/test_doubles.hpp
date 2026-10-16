#pragma once

#include <exasol/udf/v2/interface/interface.hpp>

#include <memory>

namespace exasol::udf::v2::test
{

class CallDouble final : public interface::Call
{
public:
    using interface::Call::receive;
    using interface::Call::send;

    [[nodiscard]] interface::MessageStatus receive_status() const override
    {
        return interface::MessageStatus::MessageAvailable;
    }

    [[nodiscard]] interface::Result<interface::CallMessageView> receive(
        interface::Timeout timeout) override
    {
        (void)timeout;
        return interface::Result<interface::CallMessageView>::success(interface::CallMessageView{});
    }

    [[nodiscard]] interface::Result<void> send(interface::CallMessageBuilder message,
                                               interface::Timeout timeout) override
    {
        (void)message;
        (void)timeout;
        return interface::Result<void>::success();
    }
};

class ControlStreamDouble final : public interface::ControlStream
{
public:
    using interface::ControlStream::receive;

    [[nodiscard]] interface::MessageStatus receive_status() const override
    {
        return interface::MessageStatus::NoMessage;
    }

    [[nodiscard]] interface::Result<interface::ControlMessageView> receive(
        interface::Timeout timeout) override
    {
        (void)timeout;
        return interface::Result<interface::ControlMessageView>::success(
            interface::ControlMessageView{});
    }

    [[nodiscard]] interface::Result<void> send(
        interface::ControlMessageBuilder message) override // NOSONAR
    {
        (void)message;
        return interface::Result<void>::success();
    }
};

class ContextDouble final : public interface::Context
{
public:
    using interface::Context::wait_for_message;

    [[nodiscard]] interface::MessageStatus receive_status() const override
    {
        return interface::MessageStatus::NoMessage;
    }

    [[nodiscard]] bool is_new_inbound_call_available() const override
    {
        return false;
    }

    [[nodiscard]] interface::MessageStatus wait_for_message(interface::Timeout timeout) override
    {
        (void)timeout;
        return interface::MessageStatus::TimedOut;
    }

    [[nodiscard]] interface::Result<std::unique_ptr<interface::Call>> accept_call() override
    {
        return interface::Result<std::unique_ptr<interface::Call>>::failure(
            interface::OperationStatus::Cancelled);
    }

    [[nodiscard]] interface::Result<std::unique_ptr<interface::Call>> open_call(
        interface::CallMessageBuilder opening_message) override
    {
        (void)opening_message;
        return interface::Result<std::unique_ptr<interface::Call>>::failure(
            interface::OperationStatus::Cancelled);
    }

    [[nodiscard]] interface::ControlStream& control_stream() override
    {
        return control_stream_;
    }

private:
    ControlStreamDouble control_stream_;
};

} // namespace exasol::udf::v2::test
