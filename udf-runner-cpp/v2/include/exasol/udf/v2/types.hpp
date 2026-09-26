#pragma once

#include <arrow/c/abi.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace exasol::udf::v2
{

/// Result of a non-consuming readiness operation.
enum class MessageStatus
{
    message_available,
    no_message,
    timed_out,
    cancelled,
    peer_closed,
    connection_error
};
/// Result status of a consuming or sending operation.
enum class OperationStatus
{
    ok,
    timed_out,
    cancelled,
    peer_closed,
    protocol_error,
    transport_error
};
/// Native byte order used by data buffers outside the FlatBuffer frame.
enum class Endianness
{
    little,
    big
};

/// Machine-readable and human-readable operation error details.
struct ErrorInfo
{
    std::string code;
    std::string message;
};

/// Status, optional value, and optional error returned by an operation.
template <typename T>
struct Result
{
    OperationStatus status{OperationStatus::transport_error};
    std::optional<T> value;
    std::optional<ErrorInfo> error;

    [[nodiscard]] static Result success(T result)
    {
        return {.status = OperationStatus::ok, .value = std::move(result)};
    }
    [[nodiscard]] static Result failure(OperationStatus result_status,
                                        std::optional<ErrorInfo> detail = {})
    {
        return {.status = result_status, .error = std::move(detail)};
    }
    [[nodiscard]] explicit operator bool() const noexcept
    {
        return status == OperationStatus::ok;
    }
};

template <>
struct Result<void>
{
    OperationStatus status{OperationStatus::transport_error};
    std::optional<ErrorInfo> error;

    [[nodiscard]] static Result success()
    {
        return {.status = OperationStatus::ok};
    }
    [[nodiscard]] static Result failure(OperationStatus result_status,
                                        std::optional<ErrorInfo> detail = {})
    {
        return {.status = result_status, .error = std::move(detail)};
    }
    [[nodiscard]] explicit operator bool() const noexcept
    {
        return status == OperationStatus::ok;
    }
};

/// Optional deadline duration used by receive and wait operations.
using Timeout = std::optional<std::chrono::steady_clock::duration>;

/// Named string or binary payload.
struct Payload
{
    std::string name;
    std::string value;
    bool binary{false};
};
using Payloads = std::vector<Payload>;
/// Metadata identifying a newly opened call.
struct OpenCall
{
    std::string call_name;
};
/// Flow-control credit and resume information.
struct Next
{
    std::uint32_t byte_budget{};
    bool reset{false};
    std::uint64_t row_id{};
};
/// Arrow schema and correlation-field metadata for one data direction.
struct DataSchema
{
    ArrowSchema* schema{};
    bool has_group_id{false};
    bool has_row_id{false};
};
/// Arrow record batch and its group-boundary marker.
struct RecordBatch
{
    ArrowArray* array{};
    bool is_end_of_group{false};
};

/// Composite message exchanged on a call stream.
struct CallMessage
{
    std::optional<OpenCall> open_call;
    std::optional<Payloads> payloads;
    std::optional<DataSchema> data_schema;
    std::optional<Next> next;
    std::optional<RecordBatch> record_batch;
    std::optional<ErrorInfo> error;
    bool close_call{false};
};

/// Protocol capabilities advertised by a peer.
struct ServerCapabilities
{
    std::uint32_t major{};
    std::uint32_t minor{};
    Endianness endianness{Endianness::little};
    std::uint32_t number_of_supported_workers{};
};
/// Composite message exchanged on the connection control stream.
struct ControlMessage
{
    std::optional<ServerCapabilities> server_capabilities;
    bool keep_alive{false};
    std::optional<Payloads> payloads;
    std::optional<ErrorInfo> error;
    bool close_connection{false};
};

} // namespace exasol::udf::v2
