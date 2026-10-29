#pragma once

#include <bit>
#include <cstdint>
#include <string_view>

namespace exasol::udf::v2::interface
{

/// Builds one composite message for the connection control stream.
class ControlMessageBuilder
{
public:
    ControlMessageBuilder(const ControlMessageBuilder&)            = delete;
    ControlMessageBuilder& operator=(const ControlMessageBuilder&) = delete;
    ControlMessageBuilder(ControlMessageBuilder&&)                 = delete;
    ControlMessageBuilder& operator=(ControlMessageBuilder&&)      = delete;
    virtual ~ControlMessageBuilder()                               = default;
    /// Adds or replaces the advertised protocol capabilities.
    virtual ControlMessageBuilder& server_capabilities(
        std::uint32_t major,
        std::uint32_t minor,
        std::endian endianness,
        std::uint32_t number_of_supported_workers) = 0;
    /// Adds a keep-alive message.
    virtual ControlMessageBuilder& keep_alive() = 0;
    /// Adds one named payload to the message.
    virtual ControlMessageBuilder& add_payload(std::string_view name,
                                               std::string_view value,
                                               bool binary = false) = 0;
    /// Adds or replaces a control-stream error description.
    virtual ControlMessageBuilder& error(std::string_view code, std::string_view text) = 0;
    /// Starts or acknowledges connection shutdown.
    virtual ControlMessageBuilder& close_connection() = 0;

protected:
    ControlMessageBuilder() = default;
};

} // namespace exasol::udf::v2::interface
