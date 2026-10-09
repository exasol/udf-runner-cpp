#pragma once

#include <arrow/c/abi.h>

#include <bit>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace exasol::udf::v2
{

// These are intentionally public value types: workers construct and inspect them directly.
// NOLINTBEGIN(misc-non-private-member-variables-in-classes)

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
    OperationStatus status{
        OperationStatus::TransportError}; // NOLINT(misc-non-private-member-variables-in-classes)
    std::optional<T> value;               // NOLINT(misc-non-private-member-variables-in-classes)
    std::optional<ErrorInfo> error;       // NOLINT(misc-non-private-member-variables-in-classes)

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
    OperationStatus status{
        OperationStatus::TransportError}; // NOLINT(misc-non-private-member-variables-in-classes)
    std::optional<ErrorInfo> error;       // NOLINT(misc-non-private-member-variables-in-classes)

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

/// Named string or binary payload.
struct Payload
{
    std::string name;   // NOLINT(misc-non-private-member-variables-in-classes)
    std::string value;  // NOLINT(misc-non-private-member-variables-in-classes)
    bool binary{false}; // NOLINT(misc-non-private-member-variables-in-classes)
};
using Payloads = std::vector<Payload>;
/// Metadata identifying a newly opened call.
struct OpenCall
{
    std::string call_name; // NOLINT(misc-non-private-member-variables-in-classes)
};
/// Flow-control resume information; transport credit remains internal.
struct Next
{
    bool reset{false};      // NOLINT(misc-non-private-member-variables-in-classes)
    std::uint64_t row_id{}; // NOLINT(misc-non-private-member-variables-in-classes)
};
/// Arrow schema and correlation-field metadata for one data direction.
struct DataSchema
{
    ArrowSchema* schema{};    // NOLINT(misc-non-private-member-variables-in-classes)
    bool has_group_id{false}; // NOLINT(misc-non-private-member-variables-in-classes)
    bool has_row_id{false};   // NOLINT(misc-non-private-member-variables-in-classes)
};
/// Arrow record batch and its group-boundary marker.
struct RecordBatch
{
    ArrowArray* array{};         // NOLINT(misc-non-private-member-variables-in-classes)
    bool is_end_of_group{false}; // NOLINT(misc-non-private-member-variables-in-classes)
};

/// Composite message exchanged on a call stream.
struct CallMessage
{
    std::optional<OpenCall> open_call;       // NOLINT(misc-non-private-member-variables-in-classes)
    std::optional<Payloads> payloads;        // NOLINT(misc-non-private-member-variables-in-classes)
    std::optional<DataSchema> data_schema;   // NOLINT(misc-non-private-member-variables-in-classes)
    std::optional<Next> next;                // NOLINT(misc-non-private-member-variables-in-classes)
    std::optional<RecordBatch> record_batch; // NOLINT(misc-non-private-member-variables-in-classes)
    std::optional<ErrorInfo> error;          // NOLINT(misc-non-private-member-variables-in-classes)
    bool close_call{false};                  // NOLINT(misc-non-private-member-variables-in-classes)
};

/// Protocol capabilities advertised by a peer.
struct ServerCapabilities
{
    std::uint32_t major{}; // NOLINT(misc-non-private-member-variables-in-classes)
    std::uint32_t minor{}; // NOLINT(misc-non-private-member-variables-in-classes)
    std::endian endianness{
        std::endian::native}; // NOLINT(misc-non-private-member-variables-in-classes)
    std::uint32_t
        number_of_supported_workers{}; // NOLINT(misc-non-private-member-variables-in-classes)
};
/// Composite message exchanged on the connection control stream.
struct ControlMessage
{
    std::optional<ServerCapabilities>
        server_capabilities;          // NOLINT(misc-non-private-member-variables-in-classes)
    bool keep_alive{false};           // NOLINT(misc-non-private-member-variables-in-classes)
    std::optional<Payloads> payloads; // NOLINT(misc-non-private-member-variables-in-classes)
    std::optional<ErrorInfo> error;   // NOLINT(misc-non-private-member-variables-in-classes)
    bool close_connection{false};     // NOLINT(misc-non-private-member-variables-in-classes)
};

// NOLINTEND(misc-non-private-member-variables-in-classes)

} // namespace exasol::udf::v2
