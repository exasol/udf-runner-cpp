#include <exasol/udf/v2/protocol/arrow_types/run_end_encoded.hpp>

#include <arrow/array/array_primitive.h>
#include <arrow/array/array_run_end.h>
#include <arrow/array/builder_primitive.h>
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

    std::shared_ptr<arrow::Array> make_int32(const std::vector<std::int32_t>& values)
    {
        arrow::Int32Builder builder;
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

    TEST(RunEndEncodedTest, PostfixIncrementPreservesPreviousPosition)
    {
        auto encoded =
            make_run_end(std::vector<std::uint64_t>{10, 20}, std::vector<std::uint64_t>{2, 1});
        ASSERT_TRUE(encoded.ok()) << encoded.status();
        RunArrayView<arrow::RunEndEncodedArray> view(encoded.ValueOrDie());
        auto iterator = view.begin();
        auto previous = iterator++;
        EXPECT_EQ(*previous, 10);
        EXPECT_EQ(*iterator, 10);
        previous = iterator++;
        EXPECT_EQ(*previous, 10);
        EXPECT_EQ(*iterator, 20);
    }

    TEST(RunEndEncodedTest, RejectsEndIteratorOperations)
    {
        auto encoded = make_run_end(std::vector<std::uint64_t>{10}, std::vector<std::uint64_t>{1});
        ASSERT_TRUE(encoded.ok()) << encoded.status();
        RunArrayView<arrow::RunEndEncodedArray> view(encoded.ValueOrDie());
        auto end = view.end();
        EXPECT_THROW(static_cast<void>(*end), std::out_of_range);
        EXPECT_THROW(++end, std::out_of_range);
    }

    TEST(RunEndEncodedTest, RejectsDefaultIteratorOperations)
    {
        RunArrayView<arrow::RunEndEncodedArray>::Iterator default_iterator;
        EXPECT_THROW(static_cast<void>(*default_iterator), std::out_of_range);
        EXPECT_THROW(++default_iterator, std::out_of_range);
    }

    TEST(RunEndEncodedTest, DistinguishesIteratorsFromDifferentArrays)
    {
        auto encoded = make_run_end(std::vector<std::uint64_t>{10}, std::vector<std::uint64_t>{1});
        ASSERT_TRUE(encoded.ok()) << encoded.status();
        RunArrayView<arrow::RunEndEncodedArray> view(encoded.ValueOrDie());
        auto other = make_run_end(std::vector<std::uint64_t>{10}, std::vector<std::uint64_t>{1});
        ASSERT_TRUE(other.ok()) << other.status();
        RunArrayView<arrow::RunEndEncodedArray> other_view(other.ValueOrDie());
        EXPECT_FALSE(view.begin() == other_view.begin());
    }

    TEST(RunEndEncodedTest, RejectsMalformedRuns)
    {
        const auto malformed = std::make_shared<arrow::RunEndEncodedArray>(
            arrow::run_end_encoded(arrow::int64(), arrow::uint64()), 3, make_int64({2, 2}),
            make_uint64({10, 20}));
        EXPECT_THROW((RunArrayView<arrow::RunEndEncodedArray>(malformed)), std::invalid_argument);
    }

    TEST(RunEndEncodedTest, RejectsMismatchedStoredRuns)
    {
        const auto malformed = std::make_shared<arrow::RunEndEncodedArray>(
            arrow::run_end_encoded(arrow::int64(), arrow::uint64()), 2, make_int64({2}),
            make_uint64({10, 20}));
        EXPECT_THROW((RunArrayView<arrow::RunEndEncodedArray>(malformed)), std::invalid_argument);
    }

    TEST(RunEndEncodedTest, RejectsUncoveredLogicalArray)
    {
        const auto malformed = std::make_shared<arrow::RunEndEncodedArray>(
            arrow::run_end_encoded(arrow::int64(), arrow::uint64()), 3, make_int64({2}),
            make_uint64({10}));
        EXPECT_THROW((RunArrayView<arrow::RunEndEncodedArray>(malformed)), std::invalid_argument);
    }

    TEST(RunEndEncodedTest, RejectsOffsetBeyondRuns)
    {
        const auto malformed = std::make_shared<arrow::RunEndEncodedArray>(
            arrow::run_end_encoded(arrow::int64(), arrow::uint64()), 1, make_int64({2}),
            make_uint64({10}), 3);
        EXPECT_THROW((RunArrayView<arrow::RunEndEncodedArray>(malformed)), std::invalid_argument);
    }

    TEST(RunEndEncodedTest, RejectsNegativeLogicalOffset)
    {
        const auto malformed = std::make_shared<arrow::RunEndEncodedArray>(
            arrow::run_end_encoded(arrow::int64(), arrow::uint64()), 1, make_int64({2}),
            make_uint64({10}), -1);
        EXPECT_THROW((RunArrayView<arrow::RunEndEncodedArray>(malformed)), std::invalid_argument);
    }

    TEST(RunEndEncodedTest, RejectsNullRunEnd)
    {
        arrow::Int64Builder builder;
        ASSERT_TRUE(builder.AppendNull().ok());
        std::shared_ptr<arrow::Array> ends;
        ASSERT_TRUE(builder.Finish(&ends).ok());
        const auto malformed = std::make_shared<arrow::RunEndEncodedArray>(
            arrow::run_end_encoded(arrow::int64(), arrow::uint64()), 1, ends, make_uint64({10}));
        EXPECT_THROW((RunArrayView<arrow::RunEndEncodedArray>(malformed)), std::invalid_argument);
    }

    TEST(RunEndEncodedTest, RejectsWrongValueType)
    {
        auto wrong_values = arrow::RunEndEncodedArray::Make(2, make_int64({2}), make_int64({10}));
        ASSERT_TRUE(wrong_values.ok()) << wrong_values.status();
        EXPECT_THROW((RunArrayView<arrow::RunEndEncodedArray>(wrong_values.ValueOrDie())),
                     std::invalid_argument);
    }

    TEST(RunEndEncodedTest, RejectsWrongRunEndType)
    {
        auto encoded = arrow::RunEndEncodedArray::Make(2, make_int32({2}), make_uint64({10}));
        ASSERT_TRUE(encoded.ok()) << encoded.status();
        EXPECT_THROW((RunArrayView<arrow::RunEndEncodedArray>(encoded.ValueOrDie())),
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

} // namespace
} // namespace exasol::udf::v2::protocol::arrow_types::correlation
