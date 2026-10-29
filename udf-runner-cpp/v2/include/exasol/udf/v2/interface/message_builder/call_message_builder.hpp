#pragma once

#include <exasol/udf/v2/interface/content_types.hpp>

#include <string_view>

namespace exasol::udf::v2::interface
{

/// Builds one composite message for a call stream.
class CallMessageBuilder
{
public:
    CallMessageBuilder(const CallMessageBuilder&)                = delete;
    CallMessageBuilder(CallMessageBuilder&&) noexcept            = delete;
    CallMessageBuilder& operator=(const CallMessageBuilder&)     = delete;
    CallMessageBuilder& operator=(CallMessageBuilder&&) noexcept = delete;
    virtual ~CallMessageBuilder()                                = default;

    /// Adds or replaces the call-opening metadata.
    virtual CallMessageBuilder& open_call(std::string_view name) = 0;
    /// Adds one named payload to the message.
    virtual CallMessageBuilder& add_payload(std::string_view name,
                                            std::string_view value,
                                            bool binary = false) = 0;
    /// Adds or replaces the data schema and its correlation flags.
    virtual CallMessageBuilder& data_schema(ArrowSchemaPtr schema,
                                            bool has_group_id = false,
                                            bool has_row_id   = false) = 0;
    /// Adds or replaces flow-control information and grants byte credit to the peer.
    virtual CallMessageBuilder& next(std::uint32_t byte_budget,
                                     bool reset           = false,
                                     std::uint64_t row_id = 0) = 0;
    /// Adds or replaces the record batch and its group-boundary flag.
    virtual CallMessageBuilder& record_batch(ArrowArrayPtr array, bool is_end_of_group = false) = 0;
    /// Adds or replaces a non-terminal or terminal error description.
    virtual CallMessageBuilder& error(std::string_view code, std::string_view text) = 0;
    /// Marks the call as closed after this message.
    virtual CallMessageBuilder& close_call() = 0;

protected:
    CallMessageBuilder() = default;
};

} // namespace exasol::udf::v2::interface
