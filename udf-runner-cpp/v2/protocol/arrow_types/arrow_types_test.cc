#include <exasol/udf/v2/protocol/arrow_types/range_run.hpp>
#include <exasol/udf/v2/protocol/arrow_types/run_end_encoded.hpp>

#include <arrow/array/array_primitive.h>
#include <arrow/array/array_run_end.h>
#include <arrow/array/builder_primitive.h>
#include <arrow/c/bridge.h>
#include <arrow/type.h>
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace exasol::udf::v2::protocol::arrow_types::correlation
{
namespace
{

    std::shared_ptr<arrow::Array> make_uint64(const std::vector<std::uint64_t>& values)
    {
        arrow::UInt64Builder builder;
        EXPECT_TRUE(builder.AppendValues(values).ok());
        std::shared_ptr<arrow::Array> array;
        EXPECT_TRUE(builder.Finish(&array).ok());
        return array;
    }

    std::shared_ptr<arrow::Array> make_int64(const std::vector<std::int64_t>& values)
    {
        arrow::Int64Builder builder;
        EXPECT_TRUE(builder.AppendValues(values).ok());
        std::shared_ptr<arrow::Array> array;
        EXPECT_TRUE(builder.Finish(&array).ok());
        return array;
    }

    template <typename ArrayType>
    void expect_all_values(const RunArrayView<ArrayType>& view,
                           const std::vector<std::uint64_t>& expected)
    {
        EXPECT_EQ((std::vector<std::uint64_t>(view.begin(), view.end())), expected);
        for (std::size_t position = 0; position < expected.size(); ++position)
        {
            SCOPED_TRACE(position);
            EXPECT_EQ(*view.find(static_cast<std::int64_t>(position)), expected[position]);
        }
        EXPECT_EQ(view.find(static_cast<std::int64_t>(expected.size())), view.end());
    }

    TEST(RunEndEncodedTest, EncodesAndDecodesRepeatedIds)
    {
        const std::vector<std::uint64_t> ids{10, 10, 20, 20};
        const std::vector<std::uint64_t> run_values{10, 20};
        const std::vector<std::uint64_t> counts{2, 2};
        auto result = make_run_end(run_values, counts);
        ASSERT_TRUE(result.ok()) << result.status();
        const auto& encoded = result.ValueOrDie();
        const auto ends     = std::static_pointer_cast<arrow::Int64Array>(encoded->run_ends());
        const auto values   = std::static_pointer_cast<arrow::UInt64Array>(encoded->values());
        ASSERT_EQ(ends->length(), 2);
        EXPECT_EQ(ends->Value(0), 2);
        EXPECT_EQ(ends->Value(1), 4);
        EXPECT_EQ(values->Value(0), 10);
        EXPECT_EQ(values->Value(1), 20);

        RunArrayView<arrow::RunEndEncodedArray> view(encoded);
        expect_all_values(view, ids);
        EXPECT_THROW(static_cast<void>(view.find(-1)), std::out_of_range);
        EXPECT_THROW(static_cast<void>(view.find(5)), std::out_of_range);
    }

    TEST(RunEndEncodedTest, SupportsEmptyRuns)
    {
        const std::vector<std::uint64_t> no_values;
        auto empty = make_run_end(no_values, no_values);
        ASSERT_TRUE(empty.ok()) << empty.status();
        EXPECT_EQ(empty.ValueOrDie()->run_ends()->length(), 0);
        EXPECT_EQ(empty.ValueOrDie()->values()->length(), 0);
        RunArrayView<arrow::RunEndEncodedArray> view(empty.ValueOrDie());
        expect_all_values(view, no_values);
    }

    TEST(RunEndEncodedTest, IteratesSlicedRuns)
    {
        auto encoded = make_run_end(std::vector<std::uint64_t>{10, 20, 30},
                                    std::vector<std::uint64_t>{2, 3, 2});
        ASSERT_TRUE(encoded.ok()) << encoded.status();
        auto sliced =
            std::static_pointer_cast<arrow::RunEndEncodedArray>(encoded.ValueOrDie()->Slice(1, 5));
        RunArrayView<arrow::RunEndEncodedArray> view(sliced);
        expect_all_values(view, {10, 20, 20, 20, 30});
    }

    TEST(RunEndEncodedTest, RejectsMalformedRuns)
    {
        const auto malformed = std::make_shared<arrow::RunEndEncodedArray>(
            arrow::run_end_encoded(arrow::int64(), arrow::uint64()), 3, make_int64({2, 2}),
            make_uint64({10, 20}));
        EXPECT_THROW((RunArrayView<arrow::RunEndEncodedArray>(malformed)), std::invalid_argument);
    }

    TEST(RunEndEncodedTest, RejectsWrongValueType)
    {
        auto wrong_values = arrow::RunEndEncodedArray::Make(2, make_int64({2}), make_int64({10}));
        ASSERT_TRUE(wrong_values.ok()) << wrong_values.status();
        EXPECT_THROW((RunArrayView<arrow::RunEndEncodedArray>(wrong_values.ValueOrDie())),
                     std::invalid_argument);
    }

    TEST(RunEndEncodedTest, RejectsNullEncodedValues)
    {
        arrow::UInt64Builder builder;
        ASSERT_TRUE(builder.AppendNull().ok());
        std::shared_ptr<arrow::Array> values;
        ASSERT_TRUE(builder.Finish(&values).ok());
        auto encoded = std::make_shared<arrow::RunEndEncodedArray>(
            arrow::run_end_encoded(arrow::int64(), arrow::uint64()), 1, make_int64({1}), values);
        EXPECT_THROW((RunArrayView<arrow::RunEndEncodedArray>(encoded)), std::invalid_argument);
    }

    TEST(RunEndEncodedTest, RejectsNullArray)
    {
        EXPECT_THROW((RunArrayView<arrow::RunEndEncodedArray>(nullptr)), std::invalid_argument);
    }

    TEST(RunEndEncodedTest, PreservesExplicitRuns)
    {
        const std::vector<std::uint64_t> values{10, 10};
        const std::vector<std::uint64_t> counts{1, 2};
        auto result = make_run_end(values, counts);
        ASSERT_TRUE(result.ok()) << result.status();
        const auto ends =
            std::static_pointer_cast<arrow::Int64Array>(result.ValueOrDie()->run_ends());
        const auto encoded_values =
            std::static_pointer_cast<arrow::UInt64Array>(result.ValueOrDie()->values());
        ASSERT_EQ(ends->length(), 2);
        ASSERT_EQ(encoded_values->length(), 2);
        EXPECT_EQ(ends->Value(0), 1);
        EXPECT_EQ(ends->Value(1), 3);
        EXPECT_EQ(encoded_values->Value(0), 10);
        EXPECT_EQ(encoded_values->Value(1), 10);
        RunArrayView<arrow::RunEndEncodedArray> view(result.ValueOrDie());
        expect_all_values(view, {10, 10, 10});
    }

    TEST(RunEndEncodedTest, RejectsMismatchedLengths)
    {
        const std::vector<std::uint64_t> values{10, 10};
        const std::vector<std::uint64_t> mismatched{1};
        EXPECT_FALSE(make_run_end(values, mismatched).ok());
    }

    TEST(RunEndEncodedTest, RejectsZeroCount)
    {
        const std::vector<std::uint64_t> values{10, 10};
        const std::vector<std::uint64_t> zero_count{1, 0};
        EXPECT_FALSE(make_run_end(values, zero_count).ok());
    }

    TEST(RunEndEncodedTest, RejectsTotalLengthOverflow)
    {
        const std::vector<std::uint64_t> values{10, 10};
        const std::vector<std::uint64_t> excessive{
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()), 1};
        EXPECT_FALSE(make_run_end(values, excessive).ok());
    }

    TEST(RangeRunTest, PreservesExplicitBoundaries)
    {
        const std::vector<std::uint64_t> ids{101, 102, 103, 104};
        const std::vector<std::uint64_t> run_values{101, 103};
        const std::vector<std::uint64_t> counts{2, 2};
        auto result = make_range_run(run_values, counts);
        ASSERT_TRUE(result.ok()) << result.status();
        const auto& encoded = result.ValueOrDie();
        const auto storage =
            std::static_pointer_cast<arrow::RunEndEncodedArray>(encoded->storage());
        const auto ends   = std::static_pointer_cast<arrow::Int64Array>(storage->run_ends());
        const auto values = std::static_pointer_cast<arrow::UInt64Array>(storage->values());
        ASSERT_EQ(ends->length(), 2);
        EXPECT_EQ(ends->Value(0), 2);
        EXPECT_EQ(ends->Value(1), 4);
        EXPECT_EQ(values->Value(0), 101);
        EXPECT_EQ(values->Value(1), 103);

        RunArrayView<RangeRunArray> view(encoded);
        expect_all_values(view, ids);
        EXPECT_THROW(static_cast<void>(view.find(-1)), std::out_of_range);
        EXPECT_THROW(static_cast<void>(view.find(5)), std::out_of_range);
    }

    TEST(RangeRunTest, ExpandsOverlappingConsecutiveRanges)
    {
        const std::vector<std::uint64_t> values{1, 3};
        const std::vector<std::uint64_t> counts{3, 4};
        auto result = make_range_run(values, counts);
        ASSERT_TRUE(result.ok()) << result.status();
        const auto storage =
            std::static_pointer_cast<arrow::RunEndEncodedArray>(result.ValueOrDie()->storage());
        const auto ends = std::static_pointer_cast<arrow::Int64Array>(storage->run_ends());
        const auto encoded_values = std::static_pointer_cast<arrow::UInt64Array>(storage->values());
        ASSERT_EQ(ends->length(), 2);
        ASSERT_EQ(encoded_values->length(), 2);
        EXPECT_EQ(ends->Value(0), 3);
        EXPECT_EQ(ends->Value(1), 7);
        EXPECT_EQ(encoded_values->Value(0), 1);
        EXPECT_EQ(encoded_values->Value(1), 3);
        RunArrayView<RangeRunArray> view(result.ValueOrDie());
        expect_all_values(view, {1, 2, 3, 3, 4, 5, 6});
    }

    TEST(RangeRunTest, RejectsMismatchedLengths)
    {
        const std::vector<std::uint64_t> ids{1, 2, 3};
        const std::vector<std::uint64_t> mismatched{1, 1};
        EXPECT_FALSE(make_range_run(ids, mismatched).ok());
    }

    TEST(RangeRunTest, RejectsZeroCount)
    {
        const std::vector<std::uint64_t> ids{1, 2, 3};
        const std::vector<std::uint64_t> zero_count{1, 1, 0};
        EXPECT_FALSE(make_range_run(ids, zero_count).ok());
    }

    TEST(RangeRunTest, RejectsTotalLengthOverflow)
    {
        const std::vector<std::uint64_t> ids{1, 2, 3};
        const std::vector<std::uint64_t> excessive{
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()), 1, 1};
        EXPECT_FALSE(make_range_run(ids, excessive).ok());
    }

    TEST(RangeRunTest, RejectsValueOverflow)
    {
        const std::vector<std::uint64_t> max_value{std::numeric_limits<std::uint64_t>::max()};
        const std::vector<std::uint64_t> two{2};
        EXPECT_FALSE(make_range_run(max_value, two).ok());
    }

    TEST(RangeRunTest, SupportsEmptyRuns)
    {
        const std::vector<std::uint64_t> no_values;
        auto empty = make_range_run(no_values, no_values);
        ASSERT_TRUE(empty.ok()) << empty.status();
        const auto empty_storage =
            std::static_pointer_cast<arrow::RunEndEncodedArray>(empty.ValueOrDie()->storage());
        EXPECT_EQ(empty_storage->run_ends()->length(), 0);
        EXPECT_EQ(empty_storage->values()->length(), 0);
        RunArrayView<RangeRunArray> view(empty.ValueOrDie());
        expect_all_values(view, no_values);
    }

    TEST(RangeRunTest, SupportsSingleRange)
    {
        const std::vector<std::uint64_t> ids{42, 43, 44};
        const std::vector<std::uint64_t> start{42};
        const std::vector<std::uint64_t> count{3};
        auto encoded = make_range_run(start, count);
        ASSERT_TRUE(encoded.ok()) << encoded.status();
        const auto storage =
            std::static_pointer_cast<arrow::RunEndEncodedArray>(encoded.ValueOrDie()->storage());
        const auto ends   = std::static_pointer_cast<arrow::Int64Array>(storage->run_ends());
        const auto values = std::static_pointer_cast<arrow::UInt64Array>(storage->values());
        ASSERT_EQ(ends->length(), 1);
        ASSERT_EQ(values->length(), 1);
        EXPECT_EQ(ends->Value(0), 3);
        EXPECT_EQ(values->Value(0), 42);
        RunArrayView<RangeRunArray> view(encoded.ValueOrDie());
        expect_all_values(view, ids);
    }

    TEST(RangeRunTest, IteratesSlicedRuns)
    {
        auto encoded =
            make_range_run(std::vector<std::uint64_t>{42, 100}, std::vector<std::uint64_t>{3, 3});
        ASSERT_TRUE(encoded.ok()) << encoded.status();
        const auto& array = encoded.ValueOrDie();
        RunArrayView<RangeRunArray> within_run(
            std::static_pointer_cast<RangeRunArray>(array->Slice(1, 2)));
        expect_all_values(within_run, {43, 44});

        RunArrayView<RangeRunArray> across_runs(
            std::static_pointer_cast<RangeRunArray>(array->Slice(2, 3)));
        expect_all_values(across_runs, {44, 100, 101});

        RunArrayView<RangeRunArray> from_zero(
            std::static_pointer_cast<RangeRunArray>(array->Slice(0, 2)));
        expect_all_values(from_zero, {42, 43});

        RunArrayView<RangeRunArray> empty(
            std::static_pointer_cast<RangeRunArray>(array->Slice(3, 0)));
        expect_all_values(empty, {});
    }

    TEST(RangeRunTest, RejectsMalformedRuns)
    {
        const auto malformed = std::make_shared<arrow::RunEndEncodedArray>(
            arrow::run_end_encoded(arrow::int64(), arrow::uint64()), 3, make_int64({2, 2}),
            make_uint64({10, 20}));
        const auto encoded =
            arrow::ExtensionType::WrapArray(std::make_shared<RangeRunType>(), malformed);
        EXPECT_THROW(
            (RunArrayView<RangeRunArray>(std::static_pointer_cast<RangeRunArray>(encoded))),
            std::invalid_argument);
    }

    TEST(RangeRunTest, RejectsWrongExtensionStorage)
    {
        RangeRunType type;
        EXPECT_FALSE(
            type.Deserialize(arrow::run_end_encoded(arrow::int32(), arrow::uint64()), "").ok());
    }

    TEST(RangeRunTest, RejectsUnexpectedExtensionMetadata)
    {
        RangeRunType type;
        EXPECT_FALSE(type.Deserialize(type.storage_type(), "unexpected").ok());
    }

#ifndef NDEBUG
    TEST(RangeRunTest, RejectsDecodedValueOverflow)
    {
        auto storage = arrow::RunEndEncodedArray::Make(
            2, make_int64({2}), make_uint64({std::numeric_limits<std::uint64_t>::max()}));
        ASSERT_TRUE(storage.ok()) << storage.status();
        const auto encoded =
            arrow::ExtensionType::WrapArray(std::make_shared<RangeRunType>(), storage.ValueOrDie());
        EXPECT_THROW(
            (RunArrayView<RangeRunArray>(std::static_pointer_cast<RangeRunArray>(encoded))),
            std::invalid_argument);
    }
#endif

    TEST(RangeRunTest, RegistersExtensionIdempotently)
    {
        ASSERT_TRUE(register_range_run_type().ok());
        EXPECT_TRUE(register_range_run_type().ok());
    }

    TEST(RangeRunTest, RoundTripsThroughArrowCData)
    {
        ASSERT_TRUE(register_range_run_type().ok());
        const std::vector<std::uint64_t> ids{1, 2, 3, 7, 8};
        const std::vector<std::uint64_t> starts{1, 7};
        const std::vector<std::uint64_t> counts{3, 2};
        auto encoded = make_range_run(starts, counts);
        ASSERT_TRUE(encoded.ok()) << encoded.status();

        ArrowArray c_array{};
        ArrowSchema c_schema{};
        ASSERT_TRUE(arrow::ExportArray(*encoded.ValueOrDie(), &c_array, &c_schema).ok());
        // Arrow 25 exports one unused parent buffer for REE, while its importer
        // requires the C Data layout's zero parent buffers.
        ASSERT_EQ(c_array.n_buffers, 1);
        ASSERT_EQ(c_array.buffers[0], nullptr);
        c_array.n_buffers = 0;
        auto imported     = arrow::ImportArray(&c_array, &c_schema);
        ASSERT_TRUE(imported.ok()) << imported.status();
        const auto& array = imported.ValueOrDie();
        ASSERT_EQ(array->type_id(), arrow::Type::EXTENSION);
        const auto& extension = dynamic_cast<const arrow::ExtensionArray&>(*array);
        EXPECT_EQ(extension.extension_type()->extension_name(), "exasol.udf.range_run");
        EXPECT_EQ(extension.extension_type()->storage_type()->id(), arrow::Type::RUN_END_ENCODED);
        RunArrayView<RangeRunArray> view(std::static_pointer_cast<RangeRunArray>(array));
        expect_all_values(view, ids);
    }

} // namespace
} // namespace exasol::udf::v2::protocol::arrow_types::correlation
