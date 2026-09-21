#include <cstddef>
#include <cstdint>

#include "udf_protocol.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data,
                                      std::size_t size) {
  exasol::udf::protocol::VerifyFrameBuffer(data, size);
  return 0;
}
