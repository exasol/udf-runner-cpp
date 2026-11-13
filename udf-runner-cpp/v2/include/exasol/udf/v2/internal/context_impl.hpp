#pragma once

#include <exasol/udf/v2/interface/context.hpp>
#include <exasol/udf/v2/internal/background_thread_interface.hpp>
#include <exasol/udf/v2/internal/context_state.hpp>
#include <exasol/udf/v2/internal/control_stream_impl.hpp>
#include <exasol/udf/v2/internal/stream_registry.hpp>

#include <memory>

namespace exasol::udf::v2::internal
{

class ContextImpl final : public interface::Context
{
public:
    ContextImpl(EndpointRole endpoint_role,
                std::unique_ptr<BackgroundThreadInterface> background_thread);
    ~ContextImpl() override;

    [[nodiscard]] interface::MessageStatus receive_status() const override;
    [[nodiscard]] bool is_new_inbound_call_available() const override;
    [[nodiscard]] interface::MessageStatus wait_for_message() override;
    [[nodiscard]] interface::MessageStatus wait_for_message(interface::Timeout timeout) override;
    [[nodiscard]] interface::Result<std::unique_ptr<interface::Call>> accept_call() override;
    [[nodiscard]] std::unique_ptr<interface::OpeningCallMessageBuilder>
    create_opening_call_message_builder() override;
    [[nodiscard]] interface::Result<std::unique_ptr<interface::Call>> open_call(
        std::unique_ptr<interface::OpeningCallMessageBuilder> opening_message) override;
    [[nodiscard]] interface::ControlStream& control_stream() override;

private:
    std::shared_ptr<ContextState> state_;
    ControlStreamImpl control_stream_;
};

} // namespace exasol::udf::v2::internal
