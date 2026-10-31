#include <exasol/udf/v2/internal/control_message_builder_impl.hpp>

#include <string>
#include <utility>

namespace exasol::udf::v2::internal
{

ControlMessage::ControlMessage(const std::uint64_t stream_id) : stream_id_(stream_id)
{
}

void ControlMessage::server_capabilities(const std::uint32_t major,
                                         const std::uint32_t minor,
                                         const std::endian endianness,
                                         const std::uint32_t number_of_supported_workers)
{
    server_capabilities_ =
        interface::ServerCapabilities{major, minor, endianness, number_of_supported_workers};
}

void ControlMessage::keep_alive()
{
    keep_alive_ = true;
}

void ControlMessage::add_payload(std::string_view name, std::string_view value, const bool binary)
{
    payloads_.push_back({std::string(name), std::string(value), binary});
}

void ControlMessage::error(std::string_view code, std::string_view text)
{
    error_.emplace(std::string(code), std::string(text));
}

void ControlMessage::close_connection()
{
    close_connection_ = true;
}

std::uint64_t ControlMessage::stream_id() const noexcept
{
    return stream_id_;
}

const std::optional<interface::ServerCapabilities>& ControlMessage::server_capabilities()
    const noexcept
{
    return server_capabilities_;
}

bool ControlMessage::is_keep_alive() const noexcept
{
    return keep_alive_;
}

const std::vector<interface::Payload>& ControlMessage::payloads() const noexcept
{
    return payloads_;
}

const std::optional<interface::ErrorInfo>& ControlMessage::error() const noexcept
{
    return error_;
}

bool ControlMessage::is_close_connection() const noexcept
{
    return close_connection_;
}

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
    message_.server_capabilities(major, minor, endianness, number_of_supported_workers);
    return *this;
}

interface::ControlMessageBuilder& ControlMessageBuilderImpl::keep_alive()
{
    message_.keep_alive();
    return *this;
}

interface::ControlMessageBuilder& ControlMessageBuilderImpl::add_payload(std::string_view name,
                                                                         std::string_view value,
                                                                         bool binary)
{
    message_.add_payload(name, value, binary);
    return *this;
}

interface::ControlMessageBuilder& ControlMessageBuilderImpl::error(std::string_view code,
                                                                   std::string_view text)
{
    message_.error(code, text);
    return *this;
}

interface::ControlMessageBuilder& ControlMessageBuilderImpl::close_connection()
{
    message_.close_connection();
    return *this;
}

const ControlMessage& ControlMessageBuilderImpl::message() const noexcept
{
    return message_;
}

} // namespace exasol::udf::v2::internal
