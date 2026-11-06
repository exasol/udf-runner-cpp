#include <exasol/udf/v2/internal/call_message_flatbuffer_view_impl.hpp>

#include <exasol/udf/v2/internal/common_flatbuffer_view_impl.hpp>

#include <utility>

namespace exasol::udf::v2::internal
{
namespace
{

    class OpenCallViewImpl final : public interface::OpenCallView
    {
    public:
        OpenCallViewImpl(std::shared_ptr<const protocol::DecodedFrame> frame,
                         const protocol::OpenCall* value)
            : frame_(std::move(frame)), value_(value)
        {
        }
        [[nodiscard]] std::optional<std::string_view> call_name() const noexcept override
        {
            return flatbuffer_string_view(value_ == nullptr ? nullptr : value_->call_name());
        }

    private:
        std::shared_ptr<const protocol::DecodedFrame> frame_;
        const protocol::OpenCall* value_;
    };

    class ResetViewImpl final : public interface::ResetView
    {
    public:
        ResetViewImpl(std::shared_ptr<const protocol::DecodedFrame> frame,
                      const protocol::Reset* value)
            : frame_(std::move(frame)), value_(value)
        {
        }
        [[nodiscard]] std::uint64_t row_id() const noexcept override
        {
            return value_ == nullptr ? 0 : value_->row_id();
        }

    private:
        std::shared_ptr<const protocol::DecodedFrame> frame_;
        const protocol::Reset* value_;
    };

} // namespace

std::unique_ptr<interface::OpenCallView> make_open_call_view(
    std::shared_ptr<const protocol::DecodedFrame> frame, const protocol::OpenCall* value)
{
    return value == nullptr ? nullptr : std::make_unique<OpenCallViewImpl>(std::move(frame), value);
}

std::unique_ptr<interface::ResetView> make_reset_view(
    std::shared_ptr<const protocol::DecodedFrame> frame, const protocol::Reset* value)
{
    return value == nullptr ? nullptr : std::make_unique<ResetViewImpl>(std::move(frame), value);
}

std::unique_ptr<interface::CloseCallView> make_close_call_view(
    std::shared_ptr<const protocol::DecodedFrame> frame, const protocol::CloseCall* value)
{
    return make_marker_view<interface::CloseCallView>(std::move(frame), value);
}

} // namespace exasol::udf::v2::internal
