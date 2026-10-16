#include <exasol/udf/v2/interface/result.hpp>

#include <gtest/gtest.h>

namespace exasol::udf::v2
{

TEST(ResultTest, RepresentsSuccessAndFailure)
{
    const auto result = Result<int>::success(23);
    ASSERT_TRUE(result);
    EXPECT_EQ(result.value, 23);
    EXPECT_EQ(Result<void>::failure(OperationStatus::TimedOut).status, OperationStatus::TimedOut);

    const ErrorInfo detail("code", "message");
    const auto failed = Result<int>::failure(OperationStatus::ProtocolError, detail);
    EXPECT_FALSE(failed);
    EXPECT_TRUE(failed.error.has_value());
    EXPECT_TRUE(Result<void>::success());
}

} // namespace exasol::udf::v2
