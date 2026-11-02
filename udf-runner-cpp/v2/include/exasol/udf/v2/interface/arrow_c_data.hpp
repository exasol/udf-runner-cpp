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
        std::unique_ptr<T> owner(value);
        if (owner != nullptr && owner->release != nullptr)
        {
            owner->release(owner.get());
        }
    }
};

using ArrowSchemaPtr = std::unique_ptr<ArrowSchema, ArrowCDataDeleter<ArrowSchema>>;
using ArrowArrayPtr  = std::unique_ptr<ArrowArray, ArrowCDataDeleter<ArrowArray>>;

} // namespace exasol::udf::v2::interface
