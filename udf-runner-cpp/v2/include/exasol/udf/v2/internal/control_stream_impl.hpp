#pragma once

#include <exasol/udf/v2/interface/control_stream.hpp>

#include <memory>

namespace exasol::udf::v2::internal
{

class ContextState;

class ControlStreamImpl final : public interface::ControlStream
{
public:
    explicit ControlStreamImpl(std::weak_ptr<ContextState> state);

    [[nodiscard]] interface::MessageStatus receive_status() const override;
    [[nodiscard]] std::unique_ptr<interface::ControlMessageBuilder> create_message_builder()
        const override;
    [[nodiscard]] interface::Result<std::unique_ptr<interface::ControlMessageView>> receive()
        override;
    [[nodiscard]] interface::Result<std::unique_ptr<interface::ControlMessageView>> receive(
        interface::Timeout timeout) override;
    [[nodiscard]] interface::Result<void> send(
        std::unique_ptr<interface::ControlMessageBuilder>&& message) override;

private:
    std::weak_ptr<ContextState> state_;
};

} // namespace exasol::udf::v2::internal
