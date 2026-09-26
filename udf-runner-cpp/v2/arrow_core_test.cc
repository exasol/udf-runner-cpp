#include <cstdint>
#include <memory>

#include <arrow/array/builder_primitive.h>
#include <gtest/gtest.h>

TEST(ArrowCoreTest, BuildsInt64Array)
{
    arrow::Int64Builder builder;
    ASSERT_TRUE(builder.Append(int64_t{42}).ok());

    std::shared_ptr<arrow::Array> array;
    ASSERT_TRUE(builder.Finish(&array).ok());
    EXPECT_EQ(array->length(), 1);
}
