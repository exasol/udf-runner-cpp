#pragma once

#include <exasol/udf/v2/content_types.hpp>
#include <exasol/udf/v2/udf_protocol.hpp>

#include <functional>
#include <memory>
#include <optional>

namespace exasol::udf::v2
{

/// Non-owning view of one received connection-level message.
class ControlMessageView
{
public:
    ControlMessageView() = default;
    /// Returns whether server capabilities are present.
    [[nodiscard]] bool has_server_capabilities() const noexcept;
    /// Returns server capabilities, or an empty optional when absent.
    [[nodiscard]]
    std::optional<std::reference_wrapper<const ServerCapabilities>> server_capabilities()
        const noexcept;
    [[nodiscard]] std::optional<std::reference_wrapper<const protocol::ServerCapabilities>>
    flatbuffer_server_capabilities() const noexcept;
    /// Returns whether a wire-level FlatBuffer keep-alive object is present.
    [[nodiscard]] bool has_keep_alive() const noexcept;
    /// Returns whether wire-level payloads are present.
    [[nodiscard]] bool has_payloads() const noexcept;
    /// Returns the wire-level FlatBuffer payload table, or an empty optional when absent.
    [[nodiscard]] std::optional<std::reference_wrapper<const protocol::Payloads>> payloads()
        const noexcept;
    /// Returns whether an error is present.
    [[nodiscard]] bool has_error() const noexcept;
    /// Returns the error, or an empty optional when absent.
    [[nodiscard]] std::optional<std::reference_wrapper<const ErrorInfo>> error() const noexcept;
    [[nodiscard]] std::optional<std::reference_wrapper<const protocol::Error>> flatbuffer_error()
        const noexcept;
    /// Returns whether this message starts or acknowledges connection shutdown.
    [[nodiscard]] bool has_close_connection() const noexcept;

    explicit ControlMessageView(std::shared_ptr<const protocol::DecodedFrame> frame);

private:
    std::optional<ControlMessage> control_message_;
    std::shared_ptr<const protocol::DecodedFrame> decoded_frame_;
};

} // namespace exasol::udf::v2
