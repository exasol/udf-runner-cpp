#include <exasol/udf/v2/interface/error_info.hpp>

#include <gtest/gtest.h>

namespace exasol::udf::v2
{

TEST(ErrorInfoTest, IsCopyConstructibleButNotAssignable)
{
    const ErrorInfo original("code", "message");
    ErrorInfo copy(original);
    EXPECT_EQ(copy.code(), "code");
    EXPECT_EQ(copy.message(), "message");
}

} // namespace exasol::udf::v2
