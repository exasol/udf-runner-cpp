#pragma once

#include <exasol/udf/v2/interface/call.hpp>

#include <cstdint>
#include <memory>

namespace exasol::udf::v2::internal
{

class ContextState;

class CallImpl final : public interface::Call
{
public:
    CallImpl(std::weak_ptr<ContextState> state, std::uint64_t stream_id);

    [[nodiscard]] interface::MessageStatus receive_status() const override;
    [[nodiscard]] std::unique_ptr<interface::CallMessageBuilder> create_message_builder()
        const override;
    [[nodiscard]] interface::Result<std::unique_ptr<interface::CallMessageView>> receive() override;
    [[nodiscard]] interface::Result<std::unique_ptr<interface::CallMessageView>> receive(
        interface::Timeout timeout) override;
    [[nodiscard]] interface::Result<void> send(
        std::unique_ptr<interface::CallMessageBuilder>&& message) override;
    [[nodiscard]] interface::Result<void> send(
        std::unique_ptr<interface::CallMessageBuilder>&& message,
        interface::Timeout timeout) override;

private:
    std::weak_ptr<ContextState> state_;
    std::uint64_t stream_id_;
};

} // namespace exasol::udf::v2::internal
