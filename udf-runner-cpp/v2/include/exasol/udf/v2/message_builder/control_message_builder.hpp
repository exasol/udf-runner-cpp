#pragma once

#include <exasol/udf/v2/types.hpp>

#include <string_view>

namespace exasol::udf::v2
{

/// Builds one composite message for the connection control stream.
class ControlMessageBuilder
{
public:
    /// Adds or replaces the advertised protocol capabilities.
    ControlMessageBuilder& server_capabilities(std::uint32_t major,
                                               std::uint32_t minor,
                                               std::endian endianness,
                                               std::uint32_t number_of_supported_workers);
    /// Adds a keep-alive message.
    ControlMessageBuilder& keep_alive();
    /// Adds one named payload to the message.
    ControlMessageBuilder& add_payload(std::string_view name,
                                       std::string_view value,
                                       bool binary = false);
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
