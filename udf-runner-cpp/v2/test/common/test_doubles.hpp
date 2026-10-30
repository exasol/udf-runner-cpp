#pragma once

#include <exasol/udf/v2/interface/interface.hpp>
#include <exasol/udf/v2/internal/call_message_view_impl.hpp>
#include <exasol/udf/v2/internal/control_message_view_impl.hpp>
#include <exasol/udf/v2/internal/message_builder_factory.hpp>

#include <memory>

namespace exasol::udf::v2::test
{

class CallDouble final : public interface::Call
{
public:
    [[nodiscard]] interface::MessageStatus receive_status() const override
    {
        return interface::MessageStatus::MessageAvailable;
    }

    [[nodiscard]] std::unique_ptr<interface::CallMessageBuilder> create_message_builder()
        const override
    {
        return internal::MessageBuilderFactory::create_call_builder(2);
    }

    [[nodiscard]] interface::Result<std::unique_ptr<interface::CallMessageView>> receive(
        interface::Timeout timeout) override
    {
        (void)timeout;
        return interface::Result<std::unique_ptr<interface::CallMessageView>>::success(
            std::make_unique<internal::CallMessageViewImpl>(nullptr));
    }

    [[nodiscard]] interface::Result<std::unique_ptr<interface::CallMessageView>> receive() override
    {
        return receive(std::nullopt);
    }

    [[nodiscard]] interface::Result<void> send(
        std::unique_ptr<interface::CallMessageBuilder> message, interface::Timeout timeout) override
    {
        (void)message;
        (void)timeout;
        return interface::Result<void>::success();
    }

    [[nodiscard]] interface::Result<void> send(
        std::unique_ptr<interface::CallMessageBuilder> message) override
    {
        return send(std::move(message), std::nullopt);
    }
};

class ControlStreamDouble final : public interface::ControlStream
{
public:
    [[nodiscard]] interface::MessageStatus receive_status() const override
    {
        return interface::MessageStatus::NoMessage;
    }

    [[nodiscard]] std::unique_ptr<interface::ControlMessageBuilder> create_message_builder()
        const override
    {
        return internal::MessageBuilderFactory::create_control_builder();
    }

    [[nodiscard]] interface::Result<std::unique_ptr<interface::ControlMessageView>> receive(
        interface::Timeout timeout) override
    {
        (void)timeout;
        return interface::Result<std::unique_ptr<interface::ControlMessageView>>::success(
            std::make_unique<internal::ControlMessageViewImpl>(nullptr));
    }

    [[nodiscard]] interface::Result<std::unique_ptr<interface::ControlMessageView>> receive()
        override
    {
        return receive(std::nullopt);
    }

    [[nodiscard]] interface::Result<void> send(
        std::unique_ptr<interface::ControlMessageBuilder> message) override
    {
        (void)message;
        return interface::Result<void>::success();
    }
};

class ContextDouble final : public interface::Context
{
public:
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

    [[nodiscard]] interface::MessageStatus wait_for_message() override
    {
        return wait_for_message(std::nullopt);
    }

    [[nodiscard]] interface::Result<std::unique_ptr<interface::Call>> accept_call() override
    {
        return interface::Result<std::unique_ptr<interface::Call>>::failure(
            interface::OperationStatus::Cancelled);
    }

    [[nodiscard]] std::unique_ptr<interface::OpeningCallMessageBuilder>
    create_call_message_builder() override
    {
        return internal::MessageBuilderFactory::create_opening_call_builder(2);
    }

    [[nodiscard]] interface::Result<std::unique_ptr<interface::Call>> open_call(
        std::unique_ptr<interface::OpeningCallMessageBuilder> opening_message) override
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
