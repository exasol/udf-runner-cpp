#pragma once

#include <exasol/udf/v2/interface/content_types.hpp>

namespace exasol::udf::v2::test
{

inline void count_schema_release(ArrowSchema* schema)
{
    auto* count = static_cast<int*>(schema->private_data);
    ++*count;
    schema->release = nullptr;
}

inline void count_array_release(ArrowArray* array)
{
    auto* count = static_cast<int*>(array->private_data);
    ++*count;
    array->release = nullptr;
}

} // namespace exasol::udf::v2::test
