#include <exasol/udf/v2/protocol/arrow_types/range_run.hpp>
#include <exasol/udf/v2/protocol/arrow_types/run_end_encoded.hpp>

#include <arrow/array/array_primitive.h>
#include <arrow/array/array_run_end.h>
#include <arrow/array/builder_primitive.h>
#include <arrow/c/bridge.h>
#include <arrow/type.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <memory>
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

    TEST(RunEndEncodedTest, EncodesAndDecodesRepeatedIds)
    {
        const std::vector<std::uint64_t> ids{10, 10, 20, 20};
        auto result = create_run_end_encoded_correlation_ids(ids);
        ASSERT_TRUE(result.ok()) << result.status();
        const auto& encoded = result.ValueOrDie();
        const auto ends     = std::static_pointer_cast<arrow::Int64Array>(encoded->run_ends());
        const auto values   = std::static_pointer_cast<arrow::UInt64Array>(encoded->values());
        ASSERT_EQ(ends->length(), 2);
        EXPECT_EQ(ends->Value(0), 2);
        EXPECT_EQ(ends->Value(1), 4);
        EXPECT_EQ(values->Value(0), 10);
        EXPECT_EQ(values->Value(1), 20);

        auto decoded = decode_run_end_encoded_correlation_ids(*encoded);
        ASSERT_TRUE(decoded.ok()) << decoded.status();
        EXPECT_EQ(decoded.ValueOrDie(), ids);
    }

    TEST(RunEndEncodedTest, SupportsEmptyAndPlainColumns)
    {
        auto empty = create_run_end_encoded_correlation_ids({});
        ASSERT_TRUE(empty.ok()) << empty.status();
        auto decoded_empty = decode_run_end_encoded_correlation_ids(*empty.ValueOrDie());
        ASSERT_TRUE(decoded_empty.ok()) << decoded_empty.status();
        EXPECT_TRUE(decoded_empty.ValueOrDie().empty());

        const std::vector<std::uint64_t> ids{1, 2, 3};
        auto decoded_plain = decode_run_end_encoded_correlation_ids(*make_uint64(ids));
        ASSERT_TRUE(decoded_plain.ok()) << decoded_plain.status();
        EXPECT_EQ(decoded_plain.ValueOrDie(), ids);
    }

    TEST(RunEndEncodedTest, RejectsMalformedRunsAndWrongValueType)
    {
        const auto malformed = std::make_shared<arrow::RunEndEncodedArray>(
            arrow::run_end_encoded(arrow::int64(), arrow::uint64()), 3, make_int64({2, 2}),
            make_uint64({10, 20}));
        EXPECT_FALSE(decode_run_end_encoded_correlation_ids(*malformed).ok());

        auto wrong_values = arrow::RunEndEncodedArray::Make(2, make_int64({2}), make_int64({10}));
        ASSERT_TRUE(wrong_values.ok()) << wrong_values.status();
        EXPECT_FALSE(decode_run_end_encoded_correlation_ids(*wrong_values.ValueOrDie()).ok());
    }

    TEST(RangeRunTest, SplitsConsecutiveRowsAtGroupBoundaries)
    {
        const std::vector<std::uint64_t> ids{101, 102, 103, 104};
        const std::vector<std::uint64_t> group_ids{10, 10, 20, 20};
        auto result = create_row_id_range_run(ids, group_ids);
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

        auto decoded = decode_row_id_range_run(*encoded);
        ASSERT_TRUE(decoded.ok()) << decoded.status();
        EXPECT_EQ(decoded.ValueOrDie(), ids);
    }

    TEST(RangeRunTest, AcceptsPlainFallbackAndRejectsMismatchedGroupIds)
    {
        const std::vector<std::uint64_t> ids{1, 2, 3};
        auto decoded = decode_row_id_range_run(*make_uint64(ids));
        ASSERT_TRUE(decoded.ok()) << decoded.status();
        EXPECT_EQ(decoded.ValueOrDie(), ids);

        const std::vector<std::uint64_t> too_few_group_ids{10, 10};
        EXPECT_FALSE(create_row_id_range_run(ids, too_few_group_ids).ok());
        const std::vector<std::uint64_t> too_many_group_ids{10, 10, 20, 20};
        EXPECT_FALSE(create_row_id_range_run(ids, too_many_group_ids).ok());
    }

    TEST(RangeRunTest, SupportsEmptyAndSingleRange)
    {
        auto empty = create_row_id_range_run({});
        ASSERT_TRUE(empty.ok()) << empty.status();
        auto decoded_empty = decode_row_id_range_run(*empty.ValueOrDie());
        ASSERT_TRUE(decoded_empty.ok()) << decoded_empty.status();
        EXPECT_TRUE(decoded_empty.ValueOrDie().empty());

        const std::vector<std::uint64_t> ids{42, 43, 44};
        auto encoded = create_row_id_range_run(ids);
        ASSERT_TRUE(encoded.ok()) << encoded.status();
        const auto storage =
            std::static_pointer_cast<arrow::RunEndEncodedArray>(encoded.ValueOrDie()->storage());
        const auto ends = std::static_pointer_cast<arrow::Int64Array>(storage->run_ends());
        ASSERT_EQ(ends->length(), 1);
        EXPECT_EQ(ends->Value(0), 3);
        auto decoded = decode_row_id_range_run(*encoded.ValueOrDie());
        ASSERT_TRUE(decoded.ok()) << decoded.status();
        EXPECT_EQ(decoded.ValueOrDie(), ids);
    }

    TEST(RangeRunTest, RejectsMalformedRunsAndExtensionStorage)
    {
        const auto malformed = std::make_shared<arrow::RunEndEncodedArray>(
            arrow::run_end_encoded(arrow::int64(), arrow::uint64()), 3, make_int64({2, 2}),
            make_uint64({10, 20}));
        const auto encoded =
            arrow::ExtensionType::WrapArray(std::make_shared<RangeRunType>(), malformed);
        EXPECT_FALSE(decode_row_id_range_run(*encoded).ok());

        RangeRunType type;
        EXPECT_FALSE(
            type.Deserialize(arrow::run_end_encoded(arrow::int32(), arrow::uint64()), "").ok());
        EXPECT_FALSE(type.Deserialize(type.storage_type(), "unexpected").ok());
    }

    TEST(RangeRunTest, RejectsOverflowAndNullIds)
    {
        auto storage = arrow::RunEndEncodedArray::Make(
            2, make_int64({2}), make_uint64({std::numeric_limits<std::uint64_t>::max()}));
        ASSERT_TRUE(storage.ok()) << storage.status();
        const auto encoded =
            arrow::ExtensionType::WrapArray(std::make_shared<RangeRunType>(), storage.ValueOrDie());
        EXPECT_FALSE(decode_row_id_range_run(*encoded).ok());

        arrow::UInt64Builder builder;
        ASSERT_TRUE(builder.AppendNull().ok());
        std::shared_ptr<arrow::Array> nullable;
        ASSERT_TRUE(builder.Finish(&nullable).ok());
        EXPECT_FALSE(decode_row_id_range_run(*nullable).ok());
        EXPECT_FALSE(decode_run_end_encoded_correlation_ids(*nullable).ok());
    }

    TEST(RangeRunTest, RoundTripsThroughArrowCData)
    {
        ASSERT_TRUE(register_range_run_type().ok());
        ASSERT_TRUE(register_range_run_type().ok());
        const std::vector<std::uint64_t> ids{1, 2, 3, 7, 8};
        auto encoded = create_row_id_range_run(ids);
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
        auto decoded = decode_row_id_range_run(*array);
        ASSERT_TRUE(decoded.ok()) << decoded.status();
        EXPECT_EQ(decoded.ValueOrDie(), ids);
    }

} // namespace
} // namespace exasol::udf::v2::protocol::arrow_types::correlation
