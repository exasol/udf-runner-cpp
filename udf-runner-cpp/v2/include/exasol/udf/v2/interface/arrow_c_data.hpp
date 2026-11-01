#pragma once

#include <arrow/c/abi.h>

#include <memory>

namespace exasol::udf::v2::interface
{

/// Deleter for Arrow C Data Interface objects.
template <typename T>
struct ArrowCDataDeleter
{
    void operator()(T* value) const noexcept
    {
        if (value != nullptr && value->release != nullptr)
        {
            value->release(value);
        }
        delete value;
    }
};

using ArrowSchemaPtr = std::unique_ptr<ArrowSchema, ArrowCDataDeleter<ArrowSchema>>;
using ArrowArrayPtr  = std::unique_ptr<ArrowArray, ArrowCDataDeleter<ArrowArray>>;

} // namespace exasol::udf::v2::interface
