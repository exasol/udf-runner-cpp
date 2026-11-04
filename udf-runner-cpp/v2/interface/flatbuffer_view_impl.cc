#include <exasol/udf/v2/internal/flatbuffer_view_impl.hpp>

#include <utility>

namespace exasol::udf::v2::internal
{
namespace
{

    std::optional<std::string_view> string_view(
        const third_party::flatbuffers::String* value) noexcept
    {
        return value == nullptr ? std::nullopt
                                : std::optional<std::string_view>(
                                      std::string_view(value->data(), value->size()));
    }

    class OpenCallViewImpl final : public interface::OpenCallView
    {
    public:
        OpenCallViewImpl(std::shared_ptr<const protocol::DecodedFrame> frame,
                         const protocol::OpenCall* value)
            : frame_(std::move(frame)), value_(value)
        {
        }
        std::optional<std::string_view> call_name() const noexcept override
        {
            return string_view(value_ == nullptr ? nullptr : value_->call_name());
        }

    private:
        std::shared_ptr<const protocol::DecodedFrame> frame_;
        const protocol::OpenCall* value_;
    };

    class PayloadValueViewImpl final : public interface::PayloadValueView
    {
    public:
        PayloadValueViewImpl(std::shared_ptr<const protocol::DecodedFrame> frame,
                             const protocol::Payload* value)
            : frame_(std::move(frame)), value_(value)
        {
        }
        interface::PayloadType type() const noexcept override
        {
            return value_ != nullptr &&
                           value_->payload_type() == protocol::PayloadValue_StringPayload
                       ? interface::PayloadType::String
                       : interface::PayloadType::Binary;
        }
        std::optional<std::string_view> string_value() const noexcept override
        {
            const auto* value = value_ == nullptr ? nullptr : value_->payload_as_StringPayload();
            return string_view(value == nullptr ? nullptr : value->value());
        }
        std::optional<std::span<const std::uint8_t>> binary_value() const noexcept override
        {
            const auto* value = value_ == nullptr ? nullptr : value_->payload_as_BinaryPayload();
            if (value == nullptr || value->value() == nullptr)
            {
                return std::nullopt;
            }
            return std::span<const std::uint8_t>(value->value()->Data(), value->value()->size());
        }

    private:
        std::shared_ptr<const protocol::DecodedFrame> frame_;
        const protocol::Payload* value_;
    };

    class PayloadViewImpl final : public interface::PayloadView
    {
    public:
        PayloadViewImpl(std::shared_ptr<const protocol::DecodedFrame> frame,
                        const protocol::Payload* value)
            : frame_(std::move(frame)), value_(value)
        {
        }
        std::optional<std::string_view> name() const noexcept override
        {
            return string_view(value_ == nullptr ? nullptr : value_->name());
        }
        std::unique_ptr<interface::PayloadValueView> value() const override
        {
            if (value_ == nullptr || value_->payload() == nullptr)
            {
                return nullptr;
            }
            return std::make_unique<PayloadValueViewImpl>(frame_, value_);
        }

    private:
        std::shared_ptr<const protocol::DecodedFrame> frame_;
        const protocol::Payload* value_;
    };

    class PayloadsViewImpl final : public interface::PayloadsView
    {
    public:
        PayloadsViewImpl(std::shared_ptr<const protocol::DecodedFrame> frame,
                         const protocol::Payloads* value)
            : frame_(std::move(frame)), value_(value)
        {
        }
        std::size_t size() const noexcept override
        {
            return value_ == nullptr || value_->payloads() == nullptr ? 0
                                                                      : value_->payloads()->size();
        }
        std::unique_ptr<interface::PayloadView> at(std::size_t index) const override
        {
            if (value_ == nullptr || value_->payloads() == nullptr || index >= size())
            {
                return nullptr;
            }
            return std::make_unique<PayloadViewImpl>(frame_, value_->payloads()->Get(index));
        }

    private:
        std::shared_ptr<const protocol::DecodedFrame> frame_;
        const protocol::Payloads* value_;
    };

    class ResetViewImpl final : public interface::ResetView
    {
    public:
        ResetViewImpl(std::shared_ptr<const protocol::DecodedFrame> frame,
                      const protocol::Reset* value)
            : frame_(std::move(frame)), value_(value)
        {
        }
        std::uint64_t row_id() const noexcept override
        {
            return value_ == nullptr ? 0 : value_->row_id();
        }

    private:
        std::shared_ptr<const protocol::DecodedFrame> frame_;
        const protocol::Reset* value_;
    };

    class ErrorViewImpl final : public interface::ErrorView
    {
    public:
        ErrorViewImpl(std::shared_ptr<const protocol::DecodedFrame> frame,
                      const protocol::Error* value)
            : frame_(std::move(frame)), value_(value)
        {
        }
        std::optional<std::string_view> code() const noexcept override
        {
            return string_view(value_ == nullptr ? nullptr : value_->code());
        }
        std::optional<std::string_view> message() const noexcept override
        {
            return string_view(value_ == nullptr ? nullptr : value_->message());
        }

    private:
        std::shared_ptr<const protocol::DecodedFrame> frame_;
        const protocol::Error* value_;
    };

    template <typename Interface, typename Value>
    class MarkerViewImpl : public Interface
    {
    public:
        MarkerViewImpl(std::shared_ptr<const protocol::DecodedFrame> frame, const Value*)
            : frame_(std::move(frame))
        {
        }

    private:
        std::shared_ptr<const protocol::DecodedFrame> frame_;
    };

    class VersionViewImpl final : public interface::VersionView
    {
    public:
        VersionViewImpl(std::shared_ptr<const protocol::DecodedFrame> frame,
                        const protocol::Version* value)
            : frame_(std::move(frame)), value_(value)
        {
        }
        std::uint32_t major() const noexcept override
        {
            return value_ == nullptr ? 0 : value_->major();
        }
        std::uint32_t minor() const noexcept override
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
        std::unique_ptr<interface::VersionView> supported_version() const override
        {
            if (value_ == nullptr || value_->supported_version() == nullptr)
            {
                return nullptr;
            }
            return std::make_unique<VersionViewImpl>(frame_, value_->supported_version());
        }
        interface::Endianness endianness() const noexcept override
        {
            return value_ != nullptr && value_->endianness() == protocol::Endianness_Big
                       ? interface::Endianness::Big
                       : interface::Endianness::Little;
        }
        std::uint32_t number_of_supported_workers() const noexcept override
        {
            return value_ == nullptr ? 0 : value_->number_of_supported_workers();
        }

    private:
        std::shared_ptr<const protocol::DecodedFrame> frame_;
        const protocol::ServerCapabilities* value_;
    };

} // namespace

std::unique_ptr<interface::OpenCallView> make_open_call_view(
    std::shared_ptr<const protocol::DecodedFrame> frame, const protocol::OpenCall* value)
{
    return value == nullptr ? nullptr : std::make_unique<OpenCallViewImpl>(std::move(frame), value);
}

std::unique_ptr<interface::PayloadsView> make_payloads_view(
    std::shared_ptr<const protocol::DecodedFrame> frame, const protocol::Payloads* value)
{
    return value == nullptr ? nullptr : std::make_unique<PayloadsViewImpl>(std::move(frame), value);
}

std::unique_ptr<interface::ResetView> make_reset_view(
    std::shared_ptr<const protocol::DecodedFrame> frame, const protocol::Reset* value)
{
    return value == nullptr ? nullptr : std::make_unique<ResetViewImpl>(std::move(frame), value);
}

std::unique_ptr<interface::ErrorView> make_error_view(
    std::shared_ptr<const protocol::DecodedFrame> frame, const protocol::Error* value)
{
    return value == nullptr ? nullptr : std::make_unique<ErrorViewImpl>(std::move(frame), value);
}

std::unique_ptr<interface::CloseCallView> make_close_call_view(
    std::shared_ptr<const protocol::DecodedFrame> frame, const protocol::CloseCall* value)
{
    return value == nullptr
               ? nullptr
               : std::make_unique<MarkerViewImpl<interface::CloseCallView, protocol::CloseCall>>(
                     std::move(frame), value);
}

std::unique_ptr<interface::ServerCapabilitiesView> make_server_capabilities_view(
    std::shared_ptr<const protocol::DecodedFrame> frame, const protocol::ServerCapabilities* value)
{
    return value == nullptr ? nullptr
                            : std::make_unique<ServerCapabilitiesViewImpl>(std::move(frame), value);
}

std::unique_ptr<interface::KeepAliveView> make_keep_alive_view(
    std::shared_ptr<const protocol::DecodedFrame> frame, const protocol::KeepAlive* value)
{
    return value == nullptr
               ? nullptr
               : std::make_unique<MarkerViewImpl<interface::KeepAliveView, protocol::KeepAlive>>(
                     std::move(frame), value);
}

std::unique_ptr<interface::CloseConnectionView> make_close_connection_view(
    std::shared_ptr<const protocol::DecodedFrame> frame, const protocol::CloseConnection* value)
{
    return value == nullptr
               ? nullptr
               : std::make_unique<
                     MarkerViewImpl<interface::CloseConnectionView, protocol::CloseConnection>>(
                     std::move(frame), value);
}

} // namespace exasol::udf::v2::internal
