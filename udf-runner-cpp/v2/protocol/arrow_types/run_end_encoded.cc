#include <exasol/udf/v2/protocol/arrow_types/run_end_encoded.hpp>

#include <arrow/array/array_primitive.h>
#include <arrow/array/builder_primitive.h>
#include <arrow/status.h>

#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

namespace exasol::udf::v2::protocol::arrow_types::correlation
{

arrow::Result<std::shared_ptr<arrow::RunEndEncodedArray>> create_run_end_encoded_correlation_ids(
    std::span<const std::uint64_t> ids)
{
    if (ids.size() > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max()))
    {
        return arrow::Status::Invalid("correlation array exceeds int64 length");
    }

    arrow::Int64Builder run_ends_builder;
    arrow::UInt64Builder values_builder;
    for (std::size_t index = 0; index < ids.size(); ++index)
    {
        if (index == 0 || ids[index] != ids[index - 1])
        {
            if (index != 0)
            {
                ARROW_RETURN_NOT_OK(run_ends_builder.Append(static_cast<std::int64_t>(index)));
            }
            ARROW_RETURN_NOT_OK(values_builder.Append(ids[index]));
        }
    }
    if (!ids.empty())
    {
        ARROW_RETURN_NOT_OK(run_ends_builder.Append(static_cast<std::int64_t>(ids.size())));
    }

    std::shared_ptr<arrow::Array> run_ends;
    std::shared_ptr<arrow::Array> values;
    ARROW_RETURN_NOT_OK(run_ends_builder.Finish(&run_ends));
    ARROW_RETURN_NOT_OK(values_builder.Finish(&values));
    return arrow::RunEndEncodedArray::Make(static_cast<std::int64_t>(ids.size()), run_ends, values);
}

arrow::Result<std::vector<std::uint64_t>> decode_run_end_encoded_correlation_ids(
    const arrow::Array& array)
{
    if (array.type_id() == arrow::Type::UINT64)
    {
        ARROW_RETURN_NOT_OK(array.ValidateFull());
        if (array.null_count() != 0)
        {
            return arrow::Status::Invalid("correlation IDs must not be null");
        }
        const auto& plain = dynamic_cast<const arrow::UInt64Array&>(array);
        std::vector<std::uint64_t> ids;
        ids.reserve(static_cast<std::size_t>(plain.length()));
        for (std::int64_t index = 0; index < plain.length(); ++index)
        {
            ids.push_back(plain.Value(index));
        }
        return ids;
    }
    if (array.type_id() != arrow::Type::RUN_END_ENCODED)
    {
        return arrow::Status::TypeError("expected RunEndEncoded or uint64 correlation IDs");
    }
    ARROW_RETURN_NOT_OK(array.ValidateFull());
    const auto& encoded = dynamic_cast<const arrow::RunEndEncodedArray&>(array);
    if (encoded.type()->id() != arrow::Type::RUN_END_ENCODED ||
        encoded.run_ends()->type_id() != arrow::Type::INT64 ||
        encoded.values()->type_id() != arrow::Type::UINT64)
    {
        return arrow::Status::TypeError("expected RunEndEncoded<int64, uint64>");
    }
    ARROW_ASSIGN_OR_RAISE(auto logical_run_ends,
                          encoded.LogicalRunEnds(arrow::default_memory_pool()));
    const auto run_ends = std::static_pointer_cast<arrow::Int64Array>(logical_run_ends);
    const auto values   = std::static_pointer_cast<arrow::UInt64Array>(encoded.LogicalValues());
    if (run_ends->null_count() != 0 || values->null_count() != 0 ||
        run_ends->length() != values->length())
    {
        return arrow::Status::Invalid("invalid correlation runs");
    }

    std::vector<std::uint64_t> ids;
    ids.reserve(static_cast<std::size_t>(encoded.length()));
    std::int64_t previous_end = 0;
    for (std::int64_t run = 0; run < run_ends->length(); ++run)
    {
        const auto end = run_ends->Value(run);
        if (end <= previous_end || end > encoded.length())
        {
            return arrow::Status::Invalid("invalid correlation run end");
        }
        ids.insert(ids.end(), static_cast<std::size_t>(end - previous_end), values->Value(run));
        previous_end = end;
    }
    if (previous_end != encoded.length())
    {
        return arrow::Status::Invalid("final run end does not equal logical length");
    }
    return ids;
}

} // namespace exasol::udf::v2::protocol::arrow_types::correlation
