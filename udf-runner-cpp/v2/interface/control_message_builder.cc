#include <exasol/udf/v2/internal/control_message_builder_impl.hpp>

#include <cstdlib>
#include <string>
#include <utility>

namespace exasol::udf::v2::internal
{

ControlMessageBuilderImpl::ControlMessageBuilderImpl(const std::uint64_t stream_id)
    : stream_id_(stream_id)
{
}

// LCOV_EXCL_START: capability encoding will be implemented with the protocol codec.
interface::ControlMessageBuilder& ControlMessageBuilderImpl::server_capabilities( // NOSONAR
    std::uint32_t major,
    std::uint32_t minor,
    std::endian endianness,
    std::uint32_t number_of_supported_workers)
{
    static_cast<void>(major);
    static_cast<void>(minor);
    static_cast<void>(endianness);
    static_cast<void>(number_of_supported_workers);
    std::abort();
}
// LCOV_EXCL_STOP

interface::ControlMessageBuilder& ControlMessageBuilderImpl::keep_alive()
{
    message_.keep_alive = true;
    return *this;
}

// LCOV_EXCL_START: payload encoding will be implemented with the protocol codec.
interface::ControlMessageBuilder& ControlMessageBuilderImpl::add_payload( // NOSONAR
    std::string_view name,
    std::string_view value,
    bool binary)
{
    static_cast<void>(name);
    static_cast<void>(value);
    static_cast<void>(binary);
    std::abort();
}
// LCOV_EXCL_STOP

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

ControlMessageEncodingView ControlMessageBuilderImpl::encoding_view() const noexcept
{
    return {stream_id_, message_};
}

} // namespace exasol::udf::v2::internal
