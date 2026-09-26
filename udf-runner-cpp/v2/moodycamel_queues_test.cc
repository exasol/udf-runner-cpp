#include <exasol/udf/v2/mpmc_queue.hpp>
#include <exasol/udf/v2/spsc_queue.hpp>

#include <gtest/gtest.h>

TEST(MoodycamelQueuesTest, SupportsSpscQueue)
{
    exasol::udf::v2::SpscQueue<int> spsc;
    ASSERT_TRUE(spsc.enqueue(7));
    int value = 0;
    ASSERT_TRUE(spsc.try_dequeue(value));
    EXPECT_EQ(value, 7);
}

TEST(MoodycamelQueuesTest, SupportsSpscCircularBuffer)
{
    exasol::udf::v2::SpscCircularBuffer<int> circular(2);
    ASSERT_TRUE(circular.try_enqueue(8));
    int value = 0;
    ASSERT_TRUE(circular.try_dequeue(value));
    EXPECT_EQ(value, 8);
}

TEST(MoodycamelQueuesTest, SupportsMpmcQueue)
{
    exasol::udf::v2::MpmcQueue<int> mpmc;
    ASSERT_TRUE(mpmc.enqueue(9));
    int value = 0;
    ASSERT_TRUE(mpmc.try_dequeue(value));
    EXPECT_EQ(value, 9);
}

TEST(MoodycamelQueuesTest, SupportsBlockingMpmcQueue)
{
    exasol::udf::v2::BlockingMpmcQueue<int> blocking;
    ASSERT_TRUE(blocking.enqueue(10));
    int value = 0;
    ASSERT_TRUE(blocking.try_dequeue(value));
    EXPECT_EQ(value, 10);
}
