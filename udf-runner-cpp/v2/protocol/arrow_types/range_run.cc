#include <exasol/udf/v2/protocol/arrow_types/range_run.hpp>

#include <exasol/udf/v2/protocol/arrow_types/run_end_encoded.hpp>

#include <arrow/array/array_primitive.h>
#include <arrow/array/array_run_end.h>
#include <arrow/array/builder_primitive.h>
#include <arrow/type.h>

#include <cstdint>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

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

arrow::Result<std::shared_ptr<RangeRunArray>> create_row_id_range_run(
    std::span<const std::uint64_t> ids, std::span<const std::uint64_t> group_ids)
{
    if (ids.size() > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max()))
    {
        return arrow::Status::Invalid("row ID array exceeds int64 length");
    }
    if (!group_ids.empty() && group_ids.size() != ids.size())
    {
        return arrow::Status::Invalid("group IDs must match row ID array length");
    }

    arrow::Int64Builder run_ends_builder;
    arrow::UInt64Builder values_builder;
    for (std::size_t start = 0; start < ids.size();)
    {
        std::size_t end = start + 1;
        while (end < ids.size())
        {
            if (!group_ids.empty() && group_ids[end] != group_ids[end - 1])
            {
                break;
            }
            if (ids[end - 1] == std::numeric_limits<std::uint64_t>::max() ||
                ids[end] != ids[end - 1] + 1)
            {
                break;
            }
            ++end;
        }
        ARROW_RETURN_NOT_OK(run_ends_builder.Append(static_cast<std::int64_t>(end)));
        ARROW_RETURN_NOT_OK(values_builder.Append(ids[start]));
        start = end;
    }

    std::shared_ptr<arrow::Array> run_ends;
    std::shared_ptr<arrow::Array> values;
    ARROW_RETURN_NOT_OK(run_ends_builder.Finish(&run_ends));
    ARROW_RETURN_NOT_OK(values_builder.Finish(&values));
    ARROW_ASSIGN_OR_RAISE(
        auto storage,
        arrow::RunEndEncodedArray::Make(static_cast<std::int64_t>(ids.size()), run_ends, values));
    return std::static_pointer_cast<RangeRunArray>(
        arrow::ExtensionType::WrapArray(std::make_shared<RangeRunType>(), storage));
}

arrow::Result<std::vector<std::uint64_t>> decode_row_id_range_run(const arrow::Array& array)
{
    if (array.type_id() == arrow::Type::UINT64)
    {
        return decode_run_end_encoded_correlation_ids(array);
    }
    if (array.type_id() != arrow::Type::EXTENSION)
    {
        return arrow::Status::TypeError("expected range_run or uint64 row IDs");
    }
    const auto& extension = dynamic_cast<const arrow::ExtensionArray&>(array);
    if (extension.extension_type()->extension_name() != range_run_name ||
        !has_range_run_storage(*extension.extension_type()->storage_type()))
    {
        return arrow::Status::TypeError("expected exasol.udf.range_run");
    }
    if (array.offset() != 0)
    {
        return arrow::Status::Invalid("sliced range_run arrays are unsupported");
    }
    ARROW_RETURN_NOT_OK(array.ValidateFull());
    const auto& storage = dynamic_cast<const arrow::RunEndEncodedArray&>(*extension.storage());
    const auto run_ends = std::static_pointer_cast<arrow::Int64Array>(storage.run_ends());
    const auto values   = std::static_pointer_cast<arrow::UInt64Array>(storage.values());
    if (run_ends->length() != values->length() || run_ends->null_count() != 0 ||
        values->null_count() != 0)
    {
        return arrow::Status::Invalid("invalid range_run children");
    }

    std::vector<std::uint64_t> ids;
    ids.reserve(static_cast<std::size_t>(array.length()));
    std::int64_t previous_end = 0;
    for (std::int64_t run = 0; run < run_ends->length(); ++run)
    {
        const auto end = run_ends->Value(run);
        if (end <= previous_end || end > array.length())
        {
            return arrow::Status::Invalid("invalid range_run end");
        }
        const auto run_length = static_cast<std::uint64_t>(end - previous_end);
        const auto first      = values->Value(run);
        if (first > std::numeric_limits<std::uint64_t>::max() - (run_length - 1))
        {
            return arrow::Status::Invalid("range_run exceeds uint64 row IDs");
        }
        for (std::uint64_t offset = 0; offset < run_length; ++offset)
        {
            ids.push_back(first + offset);
        }
        previous_end = end;
    }
    if (previous_end != array.length())
    {
        return arrow::Status::Invalid("final range_run end does not equal logical length");
    }
    return ids;
}

} // namespace exasol::udf::v2::protocol::arrow_types::correlation
