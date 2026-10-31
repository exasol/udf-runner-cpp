#include <exasol/udf/v2/internal/control_message_builder_impl.hpp>

#include <string>
#include <utility>

namespace exasol::udf::v2::internal
{

ControlMessageBuilderImpl::ControlMessageBuilderImpl(const std::uint64_t stream_id)
    : message_(stream_id)
{
}

interface::ControlMessageBuilder& ControlMessageBuilderImpl::server_capabilities(
    std::uint32_t major,
    std::uint32_t minor,
    std::endian endianness,
    std::uint32_t number_of_supported_workers)
{
    message_.server_capabilities =
        interface::ServerCapabilities{major, minor, endianness, number_of_supported_workers};
    return *this;
}

interface::ControlMessageBuilder& ControlMessageBuilderImpl::keep_alive()
{
    message_.keep_alive = true;
    return *this;
}

interface::ControlMessageBuilder& ControlMessageBuilderImpl::add_payload(std::string_view name,
                                                                         std::string_view value,
                                                                         bool binary)
{
    message_.payloads.push_back({std::string(name), std::string(value), binary});
    return *this;
}

interface::ControlMessageBuilder& ControlMessageBuilderImpl::error(std::string_view code,
                                                                   std::string_view text)
{
    message_.error.emplace(std::string(code), std::string(text));
    return *this;
}

interface::ControlMessageBuilder& ControlMessageBuilderImpl::close_connection()
{
    message_.close_connection = true;
    return *this;
}

const ControlMessage& ControlMessageBuilderImpl::message() const noexcept
{
    return message_;
}

} // namespace exasol::udf::v2::internal
