#pragma once

#include <exasol/udf/v2/interface/context.hpp>
#include <exasol/udf/v2/internal/context_state_interface.hpp>
#include <exasol/udf/v2/internal/control_stream_impl.hpp>

#include <memory>

namespace exasol::udf::v2::internal
{

class ContextImpl final : public interface::Context
{
public:
    explicit ContextImpl(std::shared_ptr<ContextStateInterface> state);
    ~ContextImpl() override;

    [[nodiscard]] interface::MessageStatus receive_status() const override;
    [[nodiscard]] bool is_new_inbound_call_available() const override;
    [[nodiscard]] interface::MessageStatus wait_for_message() override;
    [[nodiscard]] interface::MessageStatus wait_for_message(interface::Timeout timeout) override;
    [[nodiscard]] interface::Result<std::unique_ptr<interface::Call>> accept_call() override;
    [[nodiscard]] std::unique_ptr<interface::OpeningCallMessageBuilder>
    create_opening_call_message_builder() override;
    [[nodiscard]] interface::Result<std::unique_ptr<interface::Call>> open_call(
        std::unique_ptr<interface::OpeningCallMessageBuilder>&& opening_message) override;
    [[nodiscard]] interface::ControlStream& control_stream() override;

private:
    std::shared_ptr<ContextStateInterface> state_;
    ControlStreamImpl control_stream_;
};

} // namespace exasol::udf::v2::internal
