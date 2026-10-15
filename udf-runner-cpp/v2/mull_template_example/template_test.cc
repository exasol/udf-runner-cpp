#include "mull_template_example/template_transform.hpp"

#include <gtest/gtest.h>

namespace
{

using mull_template_example::transform;

TEST(TemplateTransformTest, HandlesNegativeInt)
{
    EXPECT_EQ(transform(-7), 7);
}

TEST(TemplateTransformTest, IncrementsNonNegativeInt)
{
    EXPECT_EQ(transform(0), 1);
    EXPECT_EQ(transform(7), 8);
}

TEST(TemplateTransformTest, InstantiatesForLongLong)
{
    constexpr long long value = -42;
    static_assert(transform(value) == 42);
    EXPECT_EQ(transform(value), 42);
}

} // namespace
