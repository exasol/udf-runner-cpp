#pragma once

#include <exasol/udf/v2/interface/content_types.hpp>

#include <string_view>
#include <utility>

namespace exasol::udf::v2::interface
{

/// Common operations for opening and subsequent call-message builders.
class CallMessageBuilderBase
{
public:
    CallMessageBuilderBase(const CallMessageBuilderBase&)            = delete;
    CallMessageBuilderBase(CallMessageBuilderBase&&)                 = delete;
    CallMessageBuilderBase& operator=(const CallMessageBuilderBase&) = delete;
    CallMessageBuilderBase& operator=(CallMessageBuilderBase&&)      = delete;
    virtual ~CallMessageBuilderBase()                                = default;

    /// Adds one named payload to the message.
    virtual CallMessageBuilderBase& add_payload(std::string_view name,
                                                std::string_view value,
                                                bool binary) = 0;
    // LCOV_EXCL_START: the current add_payload implementation aborts by design.
    CallMessageBuilderBase& add_payload(std::string_view name, std::string_view value)
    {
        return add_payload(name, value, false);
    }
    // LCOV_EXCL_STOP
    /// Adds or replaces the data schema and its correlation flags.
    virtual CallMessageBuilderBase& data_schema(ArrowSchemaPtr schema,
                                                bool has_group_id,
                                                bool has_row_id) = 0;
    CallMessageBuilderBase& data_schema(ArrowSchemaPtr schema)
    {
        return data_schema(std::move(schema), false, false);
    }
    CallMessageBuilderBase& data_schema(ArrowSchemaPtr schema, bool has_group_id)
    {
        return data_schema(std::move(schema), has_group_id, false);
    }
    /// Adds or replaces flow-control information and grants byte credit to the peer.
    virtual CallMessageBuilderBase& next(std::uint32_t byte_budget,
                                         bool reset,
                                         std::uint64_t row_id) = 0;
    CallMessageBuilderBase& next(std::uint32_t byte_budget)
    {
        return next(byte_budget, false, 0);
    }
    CallMessageBuilderBase& next(std::uint32_t byte_budget, bool reset)
    {
        return next(byte_budget, reset, 0);
    }
    /// Adds or replaces the record batch and its group-boundary flag.
    virtual CallMessageBuilderBase& record_batch(ArrowArrayPtr array, bool is_end_of_group) = 0;
    CallMessageBuilderBase& record_batch(ArrowArrayPtr array)
    {
        return record_batch(std::move(array), false);
    }
    /// Adds or replaces an error description.
    virtual CallMessageBuilderBase& error(std::string_view code, std::string_view text) = 0;
    /// Marks the call as closed after this message.
    virtual CallMessageBuilderBase& close_call() = 0;

protected:
    CallMessageBuilderBase() = default;
};

} // namespace exasol::udf::v2::interface
