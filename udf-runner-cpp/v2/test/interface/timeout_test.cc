#include <exasol/udf/v2/interface/timeout.hpp>

#include <gtest/gtest.h>

#include <chrono>

namespace exasol::udf::v2
{

TEST(TimeoutTest, StoresOptionalDuration)
{
    EXPECT_TRUE(Timeout{std::chrono::milliseconds(1)}.has_value());
    EXPECT_FALSE(Timeout{}.has_value());
}

} // namespace exasol::udf::v2
