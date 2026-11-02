#pragma once

#include <exasol/udf/v2/interface/message_view/control_message_view.hpp>

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
    [[nodiscard]] std::optional<std::reference_wrapper<const protocol::ServerCapabilities>>
    server_capabilities() const noexcept override;
    [[nodiscard]] std::optional<std::reference_wrapper<const protocol::KeepAlive>> keep_alive()
        const noexcept override;
    [[nodiscard]] std::optional<std::reference_wrapper<const protocol::Payloads>> payloads()
        const noexcept override;
    [[nodiscard]] std::optional<std::reference_wrapper<const protocol::Error>> error()
        const noexcept override;
    [[nodiscard]] std::optional<std::reference_wrapper<const protocol::CloseConnection>>
    close_connection() const noexcept override;

private:
    std::shared_ptr<const protocol::DecodedFrame> decoded_frame_;
};

} // namespace exasol::udf::v2::internal
