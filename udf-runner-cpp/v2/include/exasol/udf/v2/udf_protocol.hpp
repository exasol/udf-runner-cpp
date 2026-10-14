#ifndef EXASOL_UDF_V2_UDF_PROTOCOL_HPP_
#define EXASOL_UDF_V2_UDF_PROTOCOL_HPP_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

// The generated API uses FlatBuffers runtime types. Rewrite the runtime
// namespace only while parsing the generated header, so the global
// ::flatbuffers namespace is never part of this library's API or ABI.
#define flatbuffers exasol::udf::v2::third_party::flatbuffers
#include "udf_protocol_generated.h"
#undef flatbuffers

namespace exasol::udf::protocol
{

bool verify_frame_buffer(const void* data, std::size_t size);

/// Owns a verified FlatBuffer frame and exposes its generated read-only view.
///
/// The generated objects returned by this class are only valid while the
/// DecodedFrame object is alive. The input bytes are copied so callers may
/// reuse or release their receive buffer immediately after decoding.
class DecodedFrame
{
public:
    [[nodiscard]] static std::optional<DecodedFrame> decode(std::span<const std::uint8_t> buffer);

    [[nodiscard]] const Frame& frame() const noexcept
    {
        return *frame_;
    }

private:
    explicit DecodedFrame(std::vector<std::uint8_t> buffer);

    std::shared_ptr<const std::vector<std::uint8_t>> buffer_;
    const Frame* frame_;
};

} // namespace exasol::udf::protocol

#endif // EXASOL_UDF_V2_UDF_PROTOCOL_HPP_
