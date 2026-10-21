#pragma once

#include <exasol/udf/v2/interface/content_types.hpp>
#include <exasol/udf/v2/interface/result.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <variant>
#include <vector>

namespace exasol::udf::v2::protocol
{

struct EncodedFrame
{
    std::vector<std::byte> bytes;
};

struct DecodedFrame
{
    std::uint64_t stream_id{};
    std::variant<interface::ControlMessage, interface::CallMessage> message;
};

[[nodiscard]] interface::Result<EncodedFrame> encode_control(interface::ControlMessage message);
[[nodiscard]] interface::Result<EncodedFrame> encode_call(std::uint64_t stream_id,
                                                          interface::CallMessage message);
[[nodiscard]] interface::Result<DecodedFrame> decode(std::span<const std::byte> frame);

} // namespace exasol::udf::v2::protocol
