#include "mull_template_example/queue_like.hpp"

#include <deque>
#include <utility>

#include <gtest/gtest.h>

namespace
{

class SimpleStorage
{
public:
    explicit SimpleStorage(bool accepting = true) : accepting_(accepting) {}

    template <typename T>
    bool enqueue(T&& value)
    {
        if (!accepting_)
        {
            return false;
        }
        values_.push_back(std::forward<T>(value));
        return true;
    }

    bool try_dequeue(int& value)
    {
        if (values_.empty())
        {
            return false;
        }
        value = values_.front();
        values_.pop_front();
        return true;
    }

private:
    std::deque<int> values_;
    bool accepting_;
};

using QueueLike = mull_template_example::QueueLike<SimpleStorage>;

TEST(QueueLikeTest, EnqueueNotifiesAndStoresValue)
{
    QueueLike queue{SimpleStorage{}};

    EXPECT_TRUE(queue.enqueue(42));
    EXPECT_TRUE(queue.notified());

    int value = 0;
    ASSERT_TRUE(queue.try_dequeue(value));
    EXPECT_EQ(value, 42);
}

TEST(QueueLikeTest, EmptyStorageReportsFailure)
{
    QueueLike queue{SimpleStorage{}};
    int value = 0;

    EXPECT_FALSE(queue.try_dequeue(value));
    EXPECT_FALSE(queue.notified());
}

TEST(QueueLikeTest, FullStorageDoesNotNotify)
{
    QueueLike queue{SimpleStorage{false}};

    EXPECT_FALSE(queue.enqueue(42));
    EXPECT_FALSE(queue.notified());
}

} // namespace
