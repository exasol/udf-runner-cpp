#include <exasol/udf/v2/interface/message_builder/control_message_builder.hpp>

#include <cstdlib>
#include <string>
#include <utility>

namespace exasol::udf::v2
{

ControlMessageBuilder& ControlMessageBuilder::server_capabilities(
    std::uint32_t major,
    std::uint32_t minor,
    std::endian endianness,
    std::uint32_t number_of_supported_workers)
{
    message_.server_capabilities =
        ServerCapabilities{major, minor, endianness, number_of_supported_workers};
    return *this;
}

ControlMessageBuilder& ControlMessageBuilder::keep_alive()
{
    message_.keep_alive = true;
    return *this;
}

// LCOV_EXCL_START: payload encoding will be implemented with the protocol codec.
[[noreturn]] ControlMessageBuilder& ControlMessageBuilder::add_payload(std::string_view name,
                                                                       std::string_view value,
                                                                       bool binary)
{
    static_cast<void>(name);
    static_cast<void>(value);
    static_cast<void>(binary);
    std::abort();
}
// LCOV_EXCL_STOP

ControlMessageBuilder& ControlMessageBuilder::error(std::string_view code, std::string_view text)
{
    message_.error.emplace(std::string(code), std::string(text));
    return *this;
}

ControlMessageBuilder& ControlMessageBuilder::close_connection()
{
    message_.close_connection = true;
    return *this;
}

const ControlMessage& ControlMessageBuilder::message() const& noexcept
{
    return message_;
}

ControlMessage ControlMessageBuilder::take() &&
{
    return std::move(message_);
}

} // namespace exasol::udf::v2
