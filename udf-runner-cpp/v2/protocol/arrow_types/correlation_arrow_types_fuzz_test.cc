#include <exasol/udf/v2/protocol/arrow_types/range_run.hpp>
#include <exasol/udf/v2/protocol/arrow_types/run_end_encoded.hpp>

#include <arrow/array/array_primitive.h>
#include <arrow/array/array_run_end.h>
#include <arrow/array/builder_primitive.h>
#include <arrow/c/bridge.h>
#include <arrow/type.h>

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

namespace correlation = exasol::udf::v2::protocol::arrow_types::correlation;

constexpr std::size_t max_rows = 32;

[[noreturn]] void fuzz_failure()
{
    std::abort();
}

class Input
{
public:
    Input(const std::uint8_t* data, std::size_t size) : data(data), size(size)
    {
    }

    std::uint8_t read_byte()
    {
        if (position >= size)
        {
            return 0;
        }
        return data[position++];
    }

    std::uint64_t read_uint64()
    {
        std::uint64_t value = 0;
        for (unsigned int shift = 0; shift < 64; shift += 8)
        {
            value |= static_cast<std::uint64_t>(read_byte()) << shift;
        }
        return value;
    }

    std::int64_t read_int64()
    {
        return std::bit_cast<std::int64_t>(read_uint64());
    }

private:
    const std::uint8_t* data;
    std::size_t size;
    std::size_t position = 0;
};

std::shared_ptr<arrow::Array> make_run_ends(const std::vector<std::int64_t>& ends)
{
    arrow::Int64Builder builder;
    if (!builder.AppendValues(ends).ok())
    {
        fuzz_failure();
    }
    std::shared_ptr<arrow::Array> array;
    if (!builder.Finish(&array).ok())
    {
        fuzz_failure();
    }
    return array;
}

std::shared_ptr<arrow::Array> make_values(const std::vector<std::uint64_t>& values,
                                          const std::vector<bool>& nulls)
{
    arrow::UInt64Builder builder;
    for (std::size_t index = 0; index < values.size(); ++index)
    {
        const auto status = nulls[index] ? builder.AppendNull() : builder.Append(values[index]);
        if (!status.ok())
        {
            fuzz_failure();
        }
    }
    std::shared_ptr<arrow::Array> array;
    if (!builder.Finish(&array).ok())
    {
        fuzz_failure();
    }
    return array;
}

template <typename ArrayType>
std::optional<std::vector<std::uint64_t>> read_values(const std::shared_ptr<ArrayType>& array)
{
    try
    {
        correlation::RunArrayView<ArrayType> view(array);
        return std::vector<std::uint64_t>(view.begin(), view.end());
    }
    catch (const std::invalid_argument&)
    {
        // Rejecting malformed input is an expected fuzz outcome.
        return std::nullopt;
    }
}

void fuzz_valid_round_trip(Input& input)
{
    const auto run_count = static_cast<std::size_t>(input.read_byte()) % (max_rows + 1);
    std::vector<std::uint64_t> range_values;
    std::vector<std::uint64_t> range_counts;
    std::vector<std::uint64_t> repeated_values;
    std::vector<std::uint64_t> repeated_counts;
    std::vector<std::uint64_t> expected_ranges;
    std::vector<std::uint64_t> expected_repeated;
    for (std::size_t run = 0; run < run_count; ++run)
    {
        const auto range_value    = input.read_uint64();
        const auto repeated_value = input.read_uint64();
        auto range_count          = static_cast<std::uint64_t>(input.read_byte() % 4) + 1;
        const auto repeated_count = static_cast<std::uint64_t>(input.read_byte() % 4) + 1;
        if (range_value > std::numeric_limits<std::uint64_t>::max() - (range_count - 1))
        {
            range_count = 1;
        }
        range_values.push_back(range_value);
        range_counts.push_back(range_count);
        repeated_values.push_back(repeated_value);
        repeated_counts.push_back(repeated_count);
        for (std::uint64_t offset = 0; offset < range_count; ++offset)
        {
            expected_ranges.push_back(range_value + offset);
        }
        expected_repeated.insert(expected_repeated.end(), repeated_count, repeated_value);
    }

    auto repeated = correlation::make_run_end(repeated_values, repeated_counts);
    auto ranges   = correlation::make_range_run(range_values, range_counts);
    if (!repeated.ok() || !ranges.ok())
    {
        fuzz_failure();
    }
    correlation::RunArrayView<arrow::RunEndEncodedArray> repeated_view(repeated.ValueOrDie());
    correlation::RunArrayView<correlation::RangeRunArray> range_view(ranges.ValueOrDie());
    if (std::vector<std::uint64_t>(repeated_view.begin(), repeated_view.end()) !=
            expected_repeated ||
        std::vector<std::uint64_t>(range_view.begin(), range_view.end()) != expected_ranges)
    {
        fuzz_failure();
    }

    const auto slice_offset =
        static_cast<std::size_t>(input.read_byte()) % (expected_ranges.size() + 1);
    const auto slice_length =
        static_cast<std::size_t>(input.read_byte()) % (expected_ranges.size() - slice_offset + 1);
    auto sliced = std::static_pointer_cast<correlation::RangeRunArray>(ranges.ValueOrDie()->Slice(
        static_cast<std::int64_t>(slice_offset), static_cast<std::int64_t>(slice_length)));
    correlation::RunArrayView<correlation::RangeRunArray> sliced_view(sliced);
    for (std::size_t position = 0; position < slice_length; ++position)
    {
        if (*sliced_view.find(static_cast<std::int64_t>(position)) !=
            expected_ranges[slice_offset + position])
        {
            fuzz_failure();
        }
    }
    if (sliced_view.find(static_cast<std::int64_t>(slice_length)) != sliced_view.end())
    {
        fuzz_failure();
    }

    const auto storage =
        std::static_pointer_cast<arrow::RunEndEncodedArray>(ranges.ValueOrDie()->storage());
    const auto range_ends   = std::static_pointer_cast<arrow::Int64Array>(storage->run_ends());
    const auto range_starts = std::static_pointer_cast<arrow::UInt64Array>(storage->values());
    const auto repeated_ends =
        std::static_pointer_cast<arrow::Int64Array>(repeated.ValueOrDie()->run_ends());
    const auto repeated_starts =
        std::static_pointer_cast<arrow::UInt64Array>(repeated.ValueOrDie()->values());
    if (range_ends->length() != static_cast<std::int64_t>(run_count) ||
        repeated_ends->length() != static_cast<std::int64_t>(run_count))
    {
        fuzz_failure();
    }
    std::int64_t range_total    = 0;
    std::int64_t repeated_total = 0;
    for (std::size_t run = 0; run < run_count; ++run)
    {
        range_total += static_cast<std::int64_t>(range_counts[run]);
        repeated_total += static_cast<std::int64_t>(repeated_counts[run]);
        if (range_ends->Value(static_cast<std::int64_t>(run)) != range_total ||
            range_starts->Value(static_cast<std::int64_t>(run)) != range_values[run] ||
            repeated_ends->Value(static_cast<std::int64_t>(run)) != repeated_total ||
            repeated_starts->Value(static_cast<std::int64_t>(run)) != repeated_values[run])
        {
            fuzz_failure();
        }
    }
}

std::optional<std::vector<std::uint64_t>> expected_repeated_values(
    std::int64_t logical_length,
    const std::vector<std::int64_t>& ends,
    const std::vector<std::uint64_t>& values,
    const std::vector<bool>& nulls)
{
    std::vector<std::uint64_t> expected;
    std::size_t physical_run = 0;
    for (std::int64_t position = 0; position < logical_length; ++position)
    {
        while (physical_run < ends.size() && ends[physical_run] <= position)
        {
            ++physical_run;
        }
        if (physical_run == ends.size() || nulls[physical_run])
        {
            return std::nullopt;
        }
        expected.push_back(values[physical_run]);
    }
    return expected;
}

void fuzz_malformed_runs(Input& input)
{
    const auto logical_length = static_cast<std::int64_t>(input.read_byte() % (max_rows + 1));
    const auto count          = input.read_byte() % (max_rows + 1);
    std::vector<std::int64_t> ends;
    std::vector<std::uint64_t> values;
    std::vector<bool> nulls;
    ends.reserve(count);
    values.reserve(count);
    nulls.reserve(count);
    for (std::size_t index = 0; index < count; ++index)
    {
        ends.push_back(input.read_int64());
        values.push_back(input.read_uint64());
        nulls.push_back(input.read_byte() % 2U != 0);
    }

    auto storage = std::make_shared<arrow::RunEndEncodedArray>(
        arrow::run_end_encoded(arrow::int64(), arrow::uint64()), logical_length,
        make_run_ends(ends), make_values(values, nulls));
    auto extension =
        arrow::ExtensionType::WrapArray(std::make_shared<correlation::RangeRunType>(), storage);

    const auto expected_repeated = expected_repeated_values(logical_length, ends, values, nulls);
    std::vector<std::uint64_t> expected_ranges;
    bool valid_ranges         = true;
    std::int64_t previous_end = 0;
    for (std::size_t run = 0; run < count; ++run)
    {
        const auto end = ends[run];
        if (nulls[run] || end <= previous_end)
        {
            valid_ranges = false;
            break;
        }
#ifndef NDEBUG
        const auto length = static_cast<std::uint64_t>(end - previous_end);
        if (values[run] > std::numeric_limits<std::uint64_t>::max() - (length - 1))
        {
            valid_ranges = false;
        }
#endif
        if (valid_ranges)
        {
            const auto visible_start = std::min(previous_end, logical_length);
            const auto visible_end   = std::min(end, logical_length);
            for (auto position = visible_start; position < visible_end; ++position)
            {
                expected_ranges.push_back(values[run] +
                                          static_cast<std::uint64_t>(position - previous_end));
            }
        }
        previous_end = end;
    }
    if (previous_end < logical_length)
    {
        valid_ranges = false;
    }
    const auto repeated = read_values(storage);
    if (repeated.has_value() &&
        (!expected_repeated.has_value() || repeated.value() != expected_repeated.value()))
    {
        fuzz_failure();
    }
    const auto ranges =
        read_values(std::static_pointer_cast<correlation::RangeRunArray>(extension));
    if (ranges.has_value() && (!valid_ranges || ranges.value() != expected_ranges))
    {
        fuzz_failure();
    }
}

std::shared_ptr<arrow::Array> import_array(const arrow::Array& array)
{
    ArrowArray c_array{};
    ArrowSchema c_schema{};
    if (!arrow::ExportArray(array, &c_array, &c_schema).ok())
    {
        fuzz_failure();
    }
    // Arrow 25 exports one unused REE parent buffer, while its importer expects none.
    if (c_array.n_buffers == 1 && c_array.buffers[0] == nullptr)
    {
        c_array.n_buffers = 0;
    }
    auto imported = arrow::ImportArray(&c_array, &c_schema);
    if (!imported.ok())
    {
        fuzz_failure();
    }
    return imported.ValueOrDie();
}

void fuzz_deserialization(Input& input)
{
    if (!correlation::register_range_run_type().ok())
    {
        fuzz_failure();
    }

    const auto run_count = static_cast<std::size_t>(input.read_byte()) % (max_rows + 1);
    std::vector<std::uint64_t> starts;
    std::vector<std::uint64_t> counts;
    std::vector<std::uint64_t> expected_ranges;
    std::vector<std::uint64_t> expected_repeated;
    for (std::size_t run = 0; run < run_count; ++run)
    {
        const auto start = input.read_uint64();
        auto count       = static_cast<std::uint64_t>(input.read_byte() % 4) + 1;
        if (start > std::numeric_limits<std::uint64_t>::max() - (count - 1))
        {
            count = 1;
        }
        starts.push_back(start);
        counts.push_back(count);
        for (std::uint64_t offset = 0; offset < count; ++offset)
        {
            expected_ranges.push_back(start + offset);
        }
        expected_repeated.insert(expected_repeated.end(), count, start);
    }

    auto ranges   = correlation::make_range_run(starts, counts);
    auto repeated = correlation::make_run_end(starts, counts);
    if (!ranges.ok() || !repeated.ok())
    {
        fuzz_failure();
    }
    auto imported_ranges   = import_array(*ranges.ValueOrDie());
    auto imported_repeated = import_array(*repeated.ValueOrDie());
    if (imported_ranges->type_id() != arrow::Type::EXTENSION ||
        imported_repeated->type_id() != arrow::Type::RUN_END_ENCODED ||
        read_values(std::static_pointer_cast<correlation::RangeRunArray>(imported_ranges)) !=
            expected_ranges ||
        read_values(std::static_pointer_cast<arrow::RunEndEncodedArray>(imported_repeated)) !=
            expected_repeated)
    {
        fuzz_failure();
    }

    std::string metadata;
    const auto metadata_size = input.read_byte() % 33;
    for (std::size_t index = 0; index < metadata_size; ++index)
    {
        metadata.push_back(static_cast<char>(input.read_byte()));
    }
    correlation::RangeRunType type;
    const auto storage = input.read_byte() % 4;
    const std::shared_ptr<arrow::DataType> storage_type =
        storage == 0   ? type.storage_type()
        : storage == 1 ? arrow::uint64()
        : storage == 2 ? arrow::run_end_encoded(arrow::int32(), arrow::uint64())
                       : arrow::run_end_encoded(arrow::int64(), arrow::int64());
    const auto deserialized = type.Deserialize(storage_type, metadata);
    if (deserialized.ok() != (storage == 0 && metadata.empty()))
    {
        fuzz_failure();
    }
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    Input input(data, size);
    const auto mode = input.read_byte() % 3U;
    if (mode == 0)
    {
        fuzz_valid_round_trip(input);
    }
    else if (mode == 1)
    {
        fuzz_malformed_runs(input);
    }
    else
    {
        fuzz_deserialization(input);
    }
    return 0;
}
