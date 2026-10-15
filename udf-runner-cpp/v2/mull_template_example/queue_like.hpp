#pragma once

#include <utility>

namespace mull_template_example
{

// This intentionally mirrors the shape of WaitableQueue without depending on
// eventfd, threading, GMock, or a third-party queue. Its implementation is
// header-only, its storage type is dependent, and its forwarding member
// function is instantiated in the test translation unit.
template <typename Storage>
class QueueLike
{
public:
    explicit QueueLike(Storage storage) : storage_(std::move(storage)) {}

    template <typename T>
    bool enqueue(T&& value)
    {
        if (!storage_.enqueue(std::forward<T>(value)))
        {
            return false;
        }
        notify();
        return true;
    }

    template <typename T>
    bool try_dequeue(T& value)
    {
        return storage_.try_dequeue(value);
    }

    [[nodiscard]] bool notified() const noexcept
    {
        return notified_;
    }

private:
    void notify() noexcept
    {
        notified_ = true;
    }

    Storage storage_;
    bool notified_ = false;
};

} // namespace mull_template_example
