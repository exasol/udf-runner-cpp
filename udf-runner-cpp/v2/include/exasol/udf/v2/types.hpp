#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <utility>

namespace exasol::udf::v2
{

/// Result of a non-consuming readiness operation.
enum class MessageStatus
{
    MessageAvailable,
    NoMessage,
    TimedOut,
    Cancelled,
    PeerClosed,
    ConnectionError
};

/// Result status of a consuming or sending operation.
enum class OperationStatus
{
    Ok,
    TimedOut,
    Cancelled,
    PeerClosed,
    ProtocolError,
    TransportError
};

/// Machine-readable and human-readable operation error details.
struct ErrorInfo
{
    std::string code;    // NOLINT(misc-non-private-member-variables-in-classes)
    std::string message; // NOLINT(misc-non-private-member-variables-in-classes)
};

/// Status, optional value, and optional error returned by an operation.
template <typename T>
struct Result
{
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    OperationStatus status{OperationStatus::TransportError};
    std::optional<T> value;         // NOLINT(misc-non-private-member-variables-in-classes)
    std::optional<ErrorInfo> error; // NOLINT(misc-non-private-member-variables-in-classes)

    [[nodiscard]] static Result success(T result)
    {
        return {.status = OperationStatus::Ok, .value = std::move(result)};
    }
    [[nodiscard]] static Result failure(OperationStatus result_status,
                                        const std::optional<ErrorInfo>& detail = {})
    {
        return {.status = result_status, .error = detail};
    }
    [[nodiscard]] explicit operator bool() const noexcept
    {
        return status == OperationStatus::Ok;
    }
};

template <>
struct Result<void>
{
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    OperationStatus status{OperationStatus::TransportError};
    std::optional<ErrorInfo> error; // NOLINT(misc-non-private-member-variables-in-classes)

    [[nodiscard]] static Result success()
    {
        return {.status = OperationStatus::Ok};
    }
    [[nodiscard]] static Result failure(OperationStatus result_status,
                                        const std::optional<ErrorInfo>& detail = {})
    {
        return {.status = result_status, .error = detail};
    }
    [[nodiscard]] explicit operator bool() const noexcept
    {
        return status == OperationStatus::Ok;
    }
};

/// Optional deadline duration used by receive and wait operations.
using Timeout = std::optional<std::chrono::steady_clock::duration>;

} // namespace exasol::udf::v2
