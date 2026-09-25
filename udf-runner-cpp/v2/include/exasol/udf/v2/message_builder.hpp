#pragma once

#include <exasol/udf/v2/types.hpp>

#include <string_view>

namespace exasol::udf::v2
{

/// Builds one composite message for a call stream.
class CallMessageBuilder
{
public:
    /// Adds or replaces the call-opening metadata.
    CallMessageBuilder& open_call(std::string_view name);
    /// Adds or replaces the named payload collection.
    CallMessageBuilder& payloads(Payloads values);
    /// Adds or replaces the data schema and its correlation flags.
    CallMessageBuilder& data_schema(ArrowSchema* schema,
                                    bool has_group_id = false,
                                    bool has_row_id   = false);
    /// Adds or replaces flow-control credit information.
    CallMessageBuilder& next(std::uint32_t byte_budget,
                             bool reset           = false,
                             std::uint64_t row_id = 0);
    /// Adds or replaces the record batch and its group-boundary flag.
    CallMessageBuilder& record_batch(ArrowArray* array, bool is_end_of_group = false);
    /// Adds or replaces a non-terminal or terminal error description.
    CallMessageBuilder& error(std::string_view code, std::string_view text);
    /// Marks the call as closed after this message.
    CallMessageBuilder& close_call();
    /// Returns the current message without consuming the builder.
    [[nodiscard]] const CallMessage& message() const& noexcept;
    /// Moves the built message out of the builder.
    [[nodiscard]] CallMessage take() &&;

private:
    CallMessage message_;
};

/// Builds one composite message for the connection control stream.
class ControlMessageBuilder
{
public:
    /// Adds or replaces the advertised protocol capabilities.
    ControlMessageBuilder& server_capabilities(std::uint32_t major,
                                               std::uint32_t minor,
                                               Endianness endianness,
                                               std::uint32_t number_of_supported_workers);
    /// Adds a keep-alive message.
    ControlMessageBuilder& keep_alive();
    /// Adds or replaces the named payload collection.
    ControlMessageBuilder& payloads(Payloads values);
    /// Adds or replaces a control-stream error description.
    ControlMessageBuilder& error(std::string_view code, std::string_view text);
    /// Starts or acknowledges connection shutdown.
    ControlMessageBuilder& close_connection();
    /// Returns the current message without consuming the builder.
    [[nodiscard]] const ControlMessage& message() const& noexcept;
    /// Moves the built message out of the builder.
    [[nodiscard]] ControlMessage take() &&;

private:
    ControlMessage message_;
};

} // namespace exasol::udf::v2
