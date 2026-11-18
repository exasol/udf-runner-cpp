#pragma once

#include <exasol/udf/v2/protocol/arrow_types/range_run.hpp>

#include <arrow/array/array_primitive.h>
#include <arrow/array/builder_primitive.h>
#include <arrow/type.h>
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace exasol::udf::v2::protocol::arrow_types::correlation
{

inline std::shared_ptr<arrow::Array> make_uint64(const std::vector<std::uint64_t>& values)
{
    arrow::UInt64Builder builder;
    EXPECT_TRUE(builder.AppendValues(values).ok());
    std::shared_ptr<arrow::Array> array;
    EXPECT_TRUE(builder.Finish(&array).ok());
    return array;
}

inline std::shared_ptr<arrow::Array> make_int64(const std::vector<std::int64_t>& values)
{
    arrow::Int64Builder builder;
    EXPECT_TRUE(builder.AppendValues(values).ok());
    std::shared_ptr<arrow::Array> array;
    EXPECT_TRUE(builder.Finish(&array).ok());
    return array;
}

inline std::shared_ptr<arrow::Array> make_int32(const std::vector<std::int32_t>& values)
{
    arrow::Int32Builder builder;
    EXPECT_TRUE(builder.AppendValues(values).ok());
    std::shared_ptr<arrow::Array> array;
    EXPECT_TRUE(builder.Finish(&array).ok());
    return array;
}

// Supplies a configurable name and storage type to test rejection of incompatible
// RangeRun arrays and conflicting extension registrations.
class TestRangeRunType final : public arrow::ExtensionType
{
public:
    TestRangeRunType(std::string name, std::shared_ptr<arrow::DataType> storage_type)
        : arrow::ExtensionType(std::move(storage_type)), name(std::move(name))
    {
    }

    std::string extension_name() const override
    {
        return this->name;
    }

    bool ExtensionEquals(const arrow::ExtensionType& other) const override
    {
        return this->name == other.extension_name() && storage_type()->Equals(other.storage_type());
    }

    std::shared_ptr<arrow::Array> MakeArray(std::shared_ptr<arrow::ArrayData> data) const override
    {
        return std::make_shared<RangeRunArray>(std::move(data));
    }

    arrow::Result<std::shared_ptr<arrow::DataType>> Deserialize(
        [[maybe_unused]] std::shared_ptr<arrow::DataType> storage_type,
        [[maybe_unused]] const std::string& serialized_data) const override
    {
        return arrow::Status::NotImplemented("test extension type cannot be deserialized");
    }

    std::string Serialize() const override
    {
        return {};
    }

private:
    std::string name;
};

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

} // namespace exasol::udf::v2::protocol::arrow_types::correlation
