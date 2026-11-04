#pragma once

#include <exasol/udf/v2/interface/message_view/control_message_view.hpp>
#include <exasol/udf/v2/udf_protocol.hpp>

#include <memory>
#include <optional>

namespace exasol::udf::v2::internal
{

/// Concrete control-message view backed by a verified decoded frame.
class ControlMessageViewImpl final : public interface::ControlMessageView
{
public:
    explicit ControlMessageViewImpl(std::shared_ptr<const protocol::DecodedFrame> frame);

    [[nodiscard]] std::uint64_t stream_id() const noexcept override;
    [[nodiscard]] std::unique_ptr<interface::ServerCapabilitiesView> server_capabilities()
        const override;
    [[nodiscard]] std::unique_ptr<interface::KeepAliveView> keep_alive() const override;
    [[nodiscard]] std::unique_ptr<interface::PayloadsView> payloads() const override;
    [[nodiscard]] std::unique_ptr<interface::ErrorView> error() const override;
    [[nodiscard]] std::unique_ptr<interface::CloseConnectionView> close_connection() const override;

private:
    std::shared_ptr<const protocol::DecodedFrame> decoded_frame_;
};

} // namespace exasol::udf::v2::internal
