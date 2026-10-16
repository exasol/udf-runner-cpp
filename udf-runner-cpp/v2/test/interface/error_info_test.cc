#include <exasol/udf/v2/interface/error_info.hpp>

#include <gtest/gtest.h>

namespace exasol::udf::v2::interface
{

TEST(ErrorInfoTest, IsCopyConstructibleButNotAssignable)
{
    const ErrorInfo original("code", "message");
    EXPECT_EQ(ErrorInfo(original).code(), "code");
    EXPECT_EQ(ErrorInfo(original).message(), "message");
}

} // namespace exasol::udf::v2::interface
