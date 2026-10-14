#pragma once

#include <exasol/udf/v2/types.hpp>

#include <arrow/c/abi.h>

#include <bit>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace exasol::udf::v2
{

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
    CallMessage()                                  = default;
    CallMessage(const CallMessage&)                = default;
    CallMessage(CallMessage&&) noexcept            = default;
    CallMessage& operator=(const CallMessage&)     = delete;
    CallMessage& operator=(CallMessage&&) noexcept = delete;
    ~CallMessage()                                 = default;

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
    ControlMessage()                                     = default;
    ControlMessage(const ControlMessage&)                = default;
    ControlMessage(ControlMessage&&) noexcept            = default;
    ControlMessage& operator=(const ControlMessage&)     = delete;
    ControlMessage& operator=(ControlMessage&&) noexcept = delete;
    ~ControlMessage()                                    = default;

    std::optional<ServerCapabilities>
        server_capabilities;          // NOLINT(misc-non-private-member-variables-in-classes)
    bool keep_alive{false};           // NOLINT(misc-non-private-member-variables-in-classes)
    std::optional<Payloads> payloads; // NOLINT(misc-non-private-member-variables-in-classes)
    std::optional<ErrorInfo> error;   // NOLINT(misc-non-private-member-variables-in-classes)
    bool close_connection{false};     // NOLINT(misc-non-private-member-variables-in-classes)
};

} // namespace exasol::udf::v2
