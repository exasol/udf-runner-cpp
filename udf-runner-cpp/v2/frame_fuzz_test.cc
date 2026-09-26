// [utest~udf-v2-frame-fuzzing~1 -> dsn~udf-v2-frame-schema-implementation~1]
#include <cstddef>
#include <cstdint>

#include "udf_protocol.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    exasol::udf::protocol::verify_frame_buffer(data, size);
    return 0;
}
