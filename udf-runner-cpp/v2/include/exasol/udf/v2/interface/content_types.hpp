#pragma once

#include <arrow/c/abi.h>

#include <bit>
#include <cstdint>
#include <memory>
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

/// Protocol capabilities advertised by the server on the control stream.
struct ServerCapabilities
{
    std::uint32_t major;    // NOLINT(misc-non-private-member-variables-in-classes)
    std::uint32_t minor;    // NOLINT(misc-non-private-member-variables-in-classes)
    std::endian endianness; // NOLINT(misc-non-private-member-variables-in-classes)
    std::uint32_t
        number_of_supported_workers; // NOLINT(misc-non-private-member-variables-in-classes)
};

/// One named text or binary payload attached to a control message.
struct Payload
{
    std::string name;   // NOLINT(misc-non-private-member-variables-in-classes)
    std::string value;  // NOLINT(misc-non-private-member-variables-in-classes)
    bool binary{false}; // NOLINT(misc-non-private-member-variables-in-classes)
};

} // namespace exasol::udf::v2::interface
