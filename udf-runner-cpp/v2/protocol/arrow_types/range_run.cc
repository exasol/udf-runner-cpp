#include <exasol/udf/v2/protocol/arrow_types/range_run.hpp>
#include <exasol/udf/v2/protocol/arrow_types/run_end_encoded.hpp>

#include <arrow/array/array_primitive.h>
#include <arrow/array/array_run_end.h>
#include <arrow/type.h>

#include <cstdint>
#include <limits>
#include <memory>
#include <utility>

namespace exasol::udf::v2::protocol::arrow_types::correlation
{

namespace
{
    constexpr const char* range_run_name = "exasol.udf.range_run";

    bool has_range_run_storage(const arrow::DataType& type)
    {
        if (type.id() != arrow::Type::RUN_END_ENCODED)
        {
            return false;
        }
        const auto& encoded = dynamic_cast<const arrow::RunEndEncodedType&>(type);
        return encoded.run_end_type()->id() == arrow::Type::INT64 &&
               encoded.value_type()->id() == arrow::Type::UINT64;
    }

    arrow::Result<std::shared_ptr<RangeRunArray>> wrap_range_run(
        const std::shared_ptr<arrow::RunEndEncodedArray>& storage)
    {
        if (!has_range_run_storage(*storage->type()))
        {
            return arrow::Status::Invalid("expected RunEndEncoded<int64, uint64> storage");
        }
        ARROW_RETURN_NOT_OK(storage->ValidateFull());
        if (storage->values()->null_count() != 0)
        {
            return arrow::Status::Invalid("range_run children must not contain nulls");
        }
        const auto& ends          = dynamic_cast<const arrow::Int64Array&>(*storage->run_ends());
        const auto& starts        = dynamic_cast<const arrow::UInt64Array&>(*storage->values());
        std::int64_t previous_end = 0;
        for (std::int64_t run = 0; run < ends.length(); ++run)
        {
            const auto end   = ends.Value(run);
            const auto count = static_cast<std::uint64_t>(end - previous_end);
            if (starts.Value(run) > std::numeric_limits<std::uint64_t>::max() - (count - 1))
            {
                return arrow::Status::Invalid("range_run exceeds uint64 values");
            }
            previous_end = end;
        }
        return std::static_pointer_cast<RangeRunArray>(
            arrow::ExtensionType::WrapArray(std::make_shared<RangeRunType>(), storage));
    }
} // namespace

RangeRunType::RangeRunType()
    : arrow::ExtensionType(arrow::run_end_encoded(arrow::int64(), arrow::uint64()))
{
}

std::string RangeRunType::extension_name() const
{
    return range_run_name;
}

bool RangeRunType::ExtensionEquals(const arrow::ExtensionType& other) const
{
    return other.extension_name() == extension_name() &&
           other.storage_type()->Equals(storage_type());
}

std::shared_ptr<arrow::Array> RangeRunType::MakeArray(std::shared_ptr<arrow::ArrayData> data) const
{
    return std::make_shared<RangeRunArray>(std::move(data));
}

arrow::Result<std::shared_ptr<arrow::DataType>> RangeRunType::Deserialize(
    std::shared_ptr<arrow::DataType> storage_type, const std::string& serialized_data) const
{
    if (!has_range_run_storage(*storage_type) || !serialized_data.empty())
    {
        return arrow::Status::Invalid("invalid exasol.udf.range_run storage or metadata");
    }
    return std::make_shared<RangeRunType>();
}

std::string RangeRunType::Serialize() const
{
    return {};
}

arrow::Result<std::shared_ptr<RangeRunArray>> RangeRunType::Make(
    const std::shared_ptr<arrow::DataType>& type,
    std::int64_t logical_length,
    const std::shared_ptr<arrow::Array>& run_ends,
    const std::shared_ptr<arrow::Array>& values,
    std::int64_t logical_offset)
{
    if (type == nullptr || run_ends == nullptr || values == nullptr)
    {
        return arrow::Status::Invalid("range_run type and children must not be null");
    }
    if (!has_range_run_storage(*type))
    {
        return arrow::Status::Invalid("expected RunEndEncoded<int64, uint64> storage");
    }
    ARROW_ASSIGN_OR_RAISE(
        auto storage,
        arrow::RunEndEncodedArray::Make(type, logical_length, run_ends, values, logical_offset));
    return wrap_range_run(storage);
}

arrow::Result<std::shared_ptr<RangeRunArray>> RangeRunType::Make(
    std::int64_t logical_length,
    const std::shared_ptr<arrow::Array>& run_ends,
    const std::shared_ptr<arrow::Array>& values,
    std::int64_t logical_offset)
{
    if (run_ends == nullptr || values == nullptr)
    {
        return arrow::Status::Invalid("range_run children must not be null");
    }
    ARROW_ASSIGN_OR_RAISE(auto storage, arrow::RunEndEncodedArray::Make(logical_length, run_ends,
                                                                        values, logical_offset));
    return wrap_range_run(storage);
}

arrow::Status register_range_run_type()
{
    const auto existing = arrow::GetExtensionType(range_run_name);
    if (existing != nullptr)
    {
        if (existing->ExtensionEquals(RangeRunType()))
        {
            return arrow::Status::OK();
        }
        return arrow::Status::Invalid("a different exasol.udf.range_run type is registered");
    }
    return arrow::RegisterExtensionType(std::make_shared<RangeRunType>());
}

arrow::Result<std::shared_ptr<RangeRunArray>> make_range_run(std::span<const std::uint64_t> values,
                                                             std::span<const std::uint64_t> counts)
{
    if (values.size() != counts.size())
    {
        return arrow::Status::Invalid("range_run values and counts must have equal length");
    }

    ARROW_ASSIGN_OR_RAISE(auto storage, make_run_end(values, counts));
    return wrap_range_run(storage);
}

} // namespace exasol::udf::v2::protocol::arrow_types::correlation
