#pragma once

#if !defined(__linux__)
#error "exasol::udf::v2::WaitableQueue requires Linux eventfd"
#endif

#include <sys/eventfd.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <iterator>
#include <system_error>
#include <utility>

#include <exasol/udf/v2/mpmc_queue.hpp>
#include <exasol/udf/v2/spsc_queue.hpp>

namespace exasol::udf::v2 {

// Adds an epoll-compatible readiness descriptor to a queue. The descriptor
// signals that one or more queue elements may be available; it is not a
// one-to-one mapping between eventfd counter values and queue elements.
template <typename Queue>
class WaitableQueue {
   public:
    using queue_type = Queue;

    WaitableQueue()
        : notification_fd_(::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) {
        if (notification_fd_ == -1) {
            throw std::system_error(errno, std::generic_category(),
                                    "eventfd");
        }
    }

    explicit WaitableQueue(Queue queue)
        : queue_(std::move(queue)),
          notification_fd_(::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) {
        if (notification_fd_ == -1) {
            throw std::system_error(errno, std::generic_category(),
                                    "eventfd");
        }
    }

    ~WaitableQueue() {
        if (notification_fd_ != -1) {
            ::close(notification_fd_);
        }
    }

    WaitableQueue(const WaitableQueue&) = delete;
    WaitableQueue& operator=(const WaitableQueue&) = delete;

    WaitableQueue(WaitableQueue&& other) noexcept
        : queue_(std::move(other.queue_)),
          notification_fd_(std::exchange(other.notification_fd_, -1)) {}

    WaitableQueue& operator=(WaitableQueue&& other) noexcept {
        if (this != &other) {
            if (notification_fd_ != -1) {
                ::close(notification_fd_);
            }
            queue_ = std::move(other.queue_);
            notification_fd_ = std::exchange(other.notification_fd_, -1);
        }
        return *this;
    }

    [[nodiscard]] int native_handle() const noexcept {
        return notification_fd_;
    }

    template <typename T>
    [[nodiscard]] bool enqueue(T&& value) {
        if (!queue_.enqueue(std::forward<T>(value))) {
            return false;
        }
        notify();
        return true;
    }

    template <typename InputIt>
    std::size_t enqueue_batch(InputIt first, InputIt last) {
        std::size_t enqueued = 0;
        for (; first != last; ++first) {
            if (!queue_.enqueue(*first)) {
                break;
            }
            ++enqueued;
        }
        if (enqueued != 0) {
            notify();
        }
        return enqueued;
    }

    template <typename Output>
    [[nodiscard]] bool try_dequeue(Output& value) {
        return queue_.try_dequeue(value);
    }

    // Drains all eventfd notifications and returns their accumulated count.
    // Callers should then dequeue until the queue is empty and recheck it
    // before going back to epoll_wait().
    std::uint64_t drain_notifications() {
        std::uint64_t total = 0;
        for (;;) {
            std::uint64_t value = 0;
            const ssize_t result = ::read(notification_fd_, &value,
                                          sizeof(value));
            if (result == sizeof(value)) {
                total += value;
                continue;
            }
            if (result == -1 && errno == EINTR) {
                continue;
            }
            if (result == -1 && errno == EAGAIN) {
                return total;
            }
            if (result == -1) {
                throw std::system_error(errno, std::generic_category(),
                                        "read eventfd");
            }
            throw std::system_error(EIO, std::generic_category(),
                                    "short read from eventfd");
        }
    }

    Queue& queue() noexcept { return queue_; }
    const Queue& queue() const noexcept { return queue_; }

   private:
    void notify() {
        constexpr std::uint64_t signal = 1;
        for (;;) {
            const ssize_t result =
                ::write(notification_fd_, &signal, sizeof(signal));
            if (result == sizeof(signal)) {
                return;
            }
            if (result == -1 && errno == EINTR) {
                continue;
            }
            // A saturated eventfd is already readable. The queue item remains
            // available, so no additional notification is needed.
            if (result == -1 && errno == EAGAIN) {
                return;
            }
            if (result == -1) {
                throw std::system_error(errno, std::generic_category(),
                                        "write eventfd");
            }
            throw std::system_error(EIO, std::generic_category(),
                                    "short write to eventfd");
        }
    }

    Queue queue_;
    int notification_fd_;
};

template <typename T>
using WaitableSpscQueue = WaitableQueue<SpscQueue<T>>;

template <typename T>
using WaitableMpmcQueue = WaitableQueue<MpmcQueue<T>>;

}  // namespace exasol::udf::v2
