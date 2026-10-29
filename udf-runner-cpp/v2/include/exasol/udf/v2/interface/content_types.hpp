#pragma once

#include <exasol/udf/v2/interface/error_info.hpp>

#include <arrow/c/abi.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace exasol::udf::v2::interface
{

/// Deleter for Arrow C Data Interface objects.
template <typename T>
struct ArrowCDataDeleter
{
    void operator()(T* value) const noexcept
    {
        if (value != nullptr && value->release != nullptr)
        {
            value->release(value);
        }
    }
};

using ArrowSchemaPtr = std::unique_ptr<ArrowSchema, ArrowCDataDeleter<ArrowSchema>>;
using ArrowArrayPtr  = std::unique_ptr<ArrowArray, ArrowCDataDeleter<ArrowArray>>;

/// Metadata identifying a newly opened call.
struct OpenCall
{
    std::string call_name; // NOLINT(misc-non-private-member-variables-in-classes)
};

/// Flow-control information exposed by received messages.
struct Next
{
    bool reset{false};      // NOLINT(misc-non-private-member-variables-in-classes)
    std::uint64_t row_id{}; // NOLINT(misc-non-private-member-variables-in-classes)
};

/// Flow-control information supplied when sending a message.
struct OutboundNext
{
    std::uint32_t byte_budget{}; // NOLINT(misc-non-private-member-variables-in-classes)
    bool reset{false};           // NOLINT(misc-non-private-member-variables-in-classes)
    std::uint64_t row_id{};      // NOLINT(misc-non-private-member-variables-in-classes)
};

/// Arrow schema and correlation-field metadata for one data direction.
struct DataSchema
{
    ArrowSchemaPtr schema;    // NOLINT(misc-non-private-member-variables-in-classes)
    bool has_group_id{false}; // NOLINT(misc-non-private-member-variables-in-classes)
    bool has_row_id{false};   // NOLINT(misc-non-private-member-variables-in-classes)
};

/// Arrow record batch and its group-boundary marker.
struct RecordBatch
{
    ArrowArrayPtr array;         // NOLINT(misc-non-private-member-variables-in-classes)
    bool is_end_of_group{false}; // NOLINT(misc-non-private-member-variables-in-classes)
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

    bool keep_alive{false};         // NOLINT(misc-non-private-member-variables-in-classes)
    std::optional<ErrorInfo> error; // NOLINT(misc-non-private-member-variables-in-classes)
    bool close_connection{false};   // NOLINT(misc-non-private-member-variables-in-classes)
};

} // namespace exasol::udf::v2::interface
