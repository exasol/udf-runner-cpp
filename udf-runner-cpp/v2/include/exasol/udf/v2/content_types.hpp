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

/// Move-only owner for one Arrow C Data Interface schema.
class ArrowSchemaOwner
{
public:
    ArrowSchemaOwner() = default;
    explicit ArrowSchemaOwner(ArrowSchema* schema) noexcept : schema_(schema)
    {
    }
    ArrowSchemaOwner(const ArrowSchemaOwner&)            = delete;
    ArrowSchemaOwner& operator=(const ArrowSchemaOwner&) = delete;
    ArrowSchemaOwner(ArrowSchemaOwner&& other) noexcept : schema_(other.release())
    {
    }
    ArrowSchemaOwner& operator=(ArrowSchemaOwner&& other) noexcept
    {
        if (this != &other)
        {
            reset();
            schema_ = other.release();
        }
        return *this;
    }
    ~ArrowSchemaOwner()
    {
        reset();
    }

    [[nodiscard]] ArrowSchema* get() const noexcept
    {
        return schema_;
    }
    [[nodiscard]] ArrowSchema* release() noexcept
    {
        return std::exchange(schema_, nullptr);
    }

private:
    void reset() noexcept
    {
        if (schema_ != nullptr && schema_->release != nullptr)
        {
            schema_->release(schema_);
        }
        schema_ = nullptr;
    }

    ArrowSchema* schema_{};
};

/// Move-only owner for one Arrow C Data Interface array.
class ArrowArrayOwner
{
public:
    ArrowArrayOwner() = default;
    explicit ArrowArrayOwner(ArrowArray* array) noexcept : array_(array)
    {
    }
    ArrowArrayOwner(const ArrowArrayOwner&)            = delete;
    ArrowArrayOwner& operator=(const ArrowArrayOwner&) = delete;
    ArrowArrayOwner(ArrowArrayOwner&& other) noexcept : array_(other.release())
    {
    }
    ArrowArrayOwner& operator=(ArrowArrayOwner&& other) noexcept
    {
        if (this != &other)
        {
            reset();
            array_ = other.release();
        }
        return *this;
    }
    ~ArrowArrayOwner()
    {
        reset();
    }

    [[nodiscard]] ArrowArray* get() const noexcept
    {
        return array_;
    }
    [[nodiscard]] ArrowArray* release() noexcept
    {
        return std::exchange(array_, nullptr);
    }

private:
    void reset() noexcept
    {
        if (array_ != nullptr && array_->release != nullptr)
        {
            array_->release(array_);
        }
        array_ = nullptr;
    }

    ArrowArray* array_{};
};

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
    ArrowSchemaOwner schema;  // NOLINT(misc-non-private-member-variables-in-classes)
    bool has_group_id{false}; // NOLINT(misc-non-private-member-variables-in-classes)
    bool has_row_id{false};   // NOLINT(misc-non-private-member-variables-in-classes)
};

/// Arrow record batch and its group-boundary marker.
struct RecordBatch
{
    ArrowArrayOwner array;       // NOLINT(misc-non-private-member-variables-in-classes)
    bool is_end_of_group{false}; // NOLINT(misc-non-private-member-variables-in-classes)
};

/// Composite message exchanged on a call stream.
struct CallMessage
{
    CallMessage()                                  = default;
    CallMessage(const CallMessage&)                = delete;
    CallMessage(CallMessage&&) noexcept            = default;
    CallMessage& operator=(const CallMessage&)     = delete;
    CallMessage& operator=(CallMessage&&) noexcept = delete;
    ~CallMessage()                                 = default;

    std::optional<OpenCall> open_call;     // NOLINT(misc-non-private-member-variables-in-classes)
    std::optional<Payloads> payloads;      // NOLINT(misc-non-private-member-variables-in-classes)
    std::optional<DataSchema> data_schema; // NOLINT(misc-non-private-member-variables-in-classes)
    std::optional<Next> next;              // NOLINT(misc-non-private-member-variables-in-classes)
    std::optional<OutboundNext>
        outbound_next;                       // NOLINT(misc-non-private-member-variables-in-classes)
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
