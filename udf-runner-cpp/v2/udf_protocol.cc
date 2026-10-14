#include <exasol/udf/v2/udf_protocol.hpp>

#include <utility>

namespace exasol::udf::protocol
{

bool verify_frame_buffer(const void* data, std::size_t size)
{
    using IsolatedVerifier = exasol::udf::v2::third_party::flatbuffers::Verifier;
    IsolatedVerifier verifier(static_cast<const uint8_t*>(data), size);
    return verifier.VerifyBuffer<Frame>();
}

DecodedFrame::DecodedFrame(std::vector<std::uint8_t> buffer)
    : buffer_(std::make_shared<const std::vector<std::uint8_t>>(std::move(buffer))),
      frame_(GetFrame(buffer_->data()))
{
}

std::optional<DecodedFrame> DecodedFrame::decode(std::span<const std::uint8_t> buffer)
{
    if (buffer.empty() || !verify_frame_buffer(buffer.data(), buffer.size()))
    {
        return std::nullopt;
    }
    return DecodedFrame(std::vector<std::uint8_t>(buffer.begin(), buffer.end()));
}

} // namespace exasol::udf::protocol
