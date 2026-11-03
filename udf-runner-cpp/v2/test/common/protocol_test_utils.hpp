#pragma once

#include <exasol/udf/v2/exception.hpp>
#include <exasol/udf/v2/udf_protocol.hpp>

#include <cstdint>
#include <memory>
#include <span>
#include <utility>

namespace exasol::udf::v2::test
{

[[nodiscard]] inline std::shared_ptr<const protocol::DecodedFrame> make_empty_decoded_frame(
    const std::uint64_t stream_id)
{
    third_party::flatbuffers::FlatBufferBuilder builder;
    const auto stream_id_table = protocol::CreateStreamId(builder, stream_id);
    const auto frame           = protocol::CreateFrame(builder, stream_id_table, 0, 0);
    builder.Finish(frame);
    const auto encoded =
        std::span<const std::uint8_t>(builder.GetBufferPointer(), builder.GetSize());
    auto decoded = protocol::DecodedFrame::decode(encoded);
    if (!decoded)
    {
        throw Exception("Could not decode a test frame.");
    }
    return std::make_shared<const protocol::DecodedFrame>(std::move(*decoded));
}

} // namespace exasol::udf::v2::test
