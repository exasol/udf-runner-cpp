#pragma once

#if !defined(__linux__)
#error "exasol::udf::v2::WaitableQueueNotification requires Linux eventfd"
#endif

#include <cstdint>

namespace exasol::udf::v2
{

class WaitableQueueNotification
{
public:
    WaitableQueueNotification();
    ~WaitableQueueNotification();

    WaitableQueueNotification(const WaitableQueueNotification&)            = delete;
    WaitableQueueNotification& operator=(const WaitableQueueNotification&) = delete;

    WaitableQueueNotification(WaitableQueueNotification&& other) noexcept;
    WaitableQueueNotification& operator=(WaitableQueueNotification&& other) noexcept;

    [[nodiscard]] int native_handle() const noexcept;
    void notify() const;
    [[nodiscard]] std::uint64_t drain() const;

private:
    int notification_fd_;
};

} // namespace exasol::udf::v2
