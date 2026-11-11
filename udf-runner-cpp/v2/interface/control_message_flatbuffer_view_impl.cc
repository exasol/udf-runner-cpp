#include <exasol/udf/v2/internal/control_message_flatbuffer_view_impl.hpp>

#include <exasol/udf/v2/internal/common_flatbuffer_view_impl.hpp>

#include <utility>

namespace exasol::udf::v2::internal
{
namespace
{

    class VersionViewImpl final : public interface::VersionView
    {
    public:
        VersionViewImpl(std::shared_ptr<const protocol::DecodedFrame> frame,
                        const protocol::Version* value)
            : frame_(std::move(frame)), value_(value)
        {
        }
        [[nodiscard]] std::uint32_t major() const noexcept override
        {
            return value_ == nullptr ? 0 : value_->major();
        }
        [[nodiscard]] std::uint32_t minor() const noexcept override
        {
            return value_ == nullptr ? 0 : value_->minor();
        }

    private:
        std::shared_ptr<const protocol::DecodedFrame> frame_;
        const protocol::Version* value_;
    };

    class ServerCapabilitiesViewImpl final : public interface::ServerCapabilitiesView
    {
    public:
        ServerCapabilitiesViewImpl(std::shared_ptr<const protocol::DecodedFrame> frame,
                                   const protocol::ServerCapabilities* value)
            : frame_(std::move(frame)), value_(value)
        {
        }
        [[nodiscard]] std::unique_ptr<interface::VersionView> supported_version() const override
        {
            if (value_ == nullptr || value_->supported_version() == nullptr)
            {
                return nullptr;
            }
            return std::make_unique<VersionViewImpl>(frame_, value_->supported_version());
        }
        [[nodiscard]] interface::Endianness endianness() const noexcept override
        {
            return value_ != nullptr && value_->endianness() == protocol::Endianness_Big
                       ? interface::Endianness::Big
                       : interface::Endianness::Little;
        }
        [[nodiscard]] std::uint32_t number_of_supported_workers() const noexcept override
        {
            return value_ == nullptr ? 0 : value_->number_of_supported_workers();
        }

    private:
        std::shared_ptr<const protocol::DecodedFrame> frame_;
        const protocol::ServerCapabilities* value_;
    };

} // namespace

std::unique_ptr<interface::ServerCapabilitiesView> make_server_capabilities_view(
    std::shared_ptr<const protocol::DecodedFrame> frame, const protocol::ServerCapabilities* value)
{
    return value == nullptr ? nullptr
                            : std::make_unique<ServerCapabilitiesViewImpl>(std::move(frame), value);
}

std::unique_ptr<interface::KeepAliveView> make_keep_alive_view(
    std::shared_ptr<const protocol::DecodedFrame> frame, const protocol::KeepAlive* value)
{
    return make_marker_view<interface::KeepAliveView>(std::move(frame), value);
}

std::unique_ptr<interface::CloseConnectionView> make_close_connection_view(
    std::shared_ptr<const protocol::DecodedFrame> frame, const protocol::CloseConnection* value)
{
    return make_marker_view<interface::CloseConnectionView>(std::move(frame), value);
}

} // namespace exasol::udf::v2::internal
