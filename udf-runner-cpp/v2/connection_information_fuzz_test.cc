#include "test_utils/json_schema_fuzzing.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    exasol::udf::v2::fuzzing::fuzz_json_schema(data, size,
                                               "json_schema/connection_information.schema.json");
    return 0;
}
