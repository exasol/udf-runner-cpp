#pragma once

#include <exasol/udf/v2/types.hpp>

#include <functional>
#include <optional>

namespace exasol::udf::v2
{

/// Non-owning view of one received connection-level message.
class ControlMessageView
{
public:
    ControlMessageView() = default;
    explicit ControlMessageView(const ControlMessage& value) : value_(value)
    {
    }
    /// Returns whether server capabilities are present.
    [[nodiscard]] bool has_server_capabilities() const noexcept;
    /// Returns server capabilities, or an empty optional when absent.
    [[nodiscard]]
    std::optional<std::reference_wrapper<const ServerCapabilities>> server_capabilities()
        const noexcept;
    /// Returns whether a keep-alive is present.
    [[nodiscard]] bool has_keep_alive() const noexcept;
    /// Returns whether this message is a keep-alive message.
    [[nodiscard]] bool keep_alive() const noexcept;
    /// Returns whether payloads are present.
    [[nodiscard]] bool has_payloads() const noexcept;
    /// Returns payloads, or an empty optional when absent.
    [[nodiscard]] std::optional<std::reference_wrapper<const Payloads>> payloads() const noexcept;
    /// Returns whether an error is present.
    [[nodiscard]] bool has_error() const noexcept;
    /// Returns the error, or an empty optional when absent.
    [[nodiscard]] std::optional<std::reference_wrapper<const ErrorInfo>> error() const noexcept;
    /// Returns whether connection shutdown is being started or acknowledged.
    [[nodiscard]] bool has_close_connection() const noexcept;
    /// Returns whether this message starts or acknowledges connection shutdown.
    [[nodiscard]] bool close_connection() const noexcept;

private:
    std::optional<std::reference_wrapper<const ControlMessage>> value_;
};

} // namespace exasol::udf::v2
