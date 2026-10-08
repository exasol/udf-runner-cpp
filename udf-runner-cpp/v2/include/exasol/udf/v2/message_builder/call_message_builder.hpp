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
    /// Adds one named payload to the message.
    CallMessageBuilder& add_payload(std::string_view name,
                                    std::string_view value,
                                    bool binary = false);
    /// Adds or replaces the data schema and its correlation flags.
    CallMessageBuilder& data_schema(ArrowSchema* schema,
                                    bool has_group_id = false,
                                    bool has_row_id   = false);
    /// Adds or replaces flow-control credit information.
    CallMessageBuilder& next(bool reset = false, std::uint64_t row_id = 0);
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

} // namespace exasol::udf::v2
