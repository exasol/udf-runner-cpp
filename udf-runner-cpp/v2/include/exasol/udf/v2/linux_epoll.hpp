#pragma once

#if !defined(__linux__)
#error "exasol::udf::v2::LinuxEpoll requires Linux epoll"
#endif

#include <sys/epoll.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include <exasol/udf/v2/posix/posix.hpp>

namespace exasol::udf::v2
{

/** Specifies the readiness interests for an epoll registration. */
struct EpollInterest
{
    bool readable = false;
    bool writable = false;
};

/** Describes one readiness event returned by LinuxEpoll::wait(). */
struct EpollReadyEvent
{
    std::uint64_t token = 0;
    bool readable       = false;
    bool writable       = false;
    bool error          = false;
    bool hangup         = false;
    bool peer_closed    = false;
};

/** Owns a single epoll registration for a borrowed file descriptor. */
class EpollRegistration
{
public:
    virtual ~EpollRegistration() = default;

    EpollRegistration(const EpollRegistration&)            = delete;
    EpollRegistration& operator=(const EpollRegistration&) = delete;
    EpollRegistration(EpollRegistration&&)                 = delete;
    EpollRegistration& operator=(EpollRegistration&&)      = delete;

    /** Updates this registration's readiness interest and event token. */
    virtual void modify(EpollInterest interest, std::uint64_t token) = 0;

    /** Deregisters the file descriptor. This operation is idempotent. */
    virtual void remove() noexcept = 0;

    /** Returns whether this object still owns an active registration. */
    [[nodiscard]] virtual bool is_registered() const noexcept = 0;

protected:
    EpollRegistration() = default;
};

/** Provides Linux epoll readiness waiting. */
class LinuxEpoll
{
public:
    virtual ~LinuxEpoll() = default;

    LinuxEpoll(const LinuxEpoll&)            = delete;
    LinuxEpoll& operator=(const LinuxEpoll&) = delete;
    LinuxEpoll(LinuxEpoll&&)                 = delete;
    LinuxEpoll& operator=(LinuxEpoll&&)      = delete;

    /** Adds a borrowed file descriptor and returns its owning registration. */
    [[nodiscard]] virtual std::unique_ptr<EpollRegistration> add(int file_descriptor,
                                                                 EpollInterest interest,
                                                                 std::uint64_t token) = 0;

    /** Waits for readiness events, returning the number written to @p events. */
    [[nodiscard]] virtual std::size_t wait(std::span<EpollReadyEvent> events,
                                           std::optional<std::chrono::milliseconds> timeout) = 0;

protected:
    LinuxEpoll() = default;
};

namespace detail
{
    class NativeEpollState;
}

/** Implements LinuxEpoll using native Linux epoll APIs. */
class NativeLinuxEpoll final : public LinuxEpoll
{
public:
    NativeLinuxEpoll();
    explicit NativeLinuxEpoll(std::shared_ptr<posix::Posix> posix_facade);
    ~NativeLinuxEpoll() override;

    NativeLinuxEpoll(const NativeLinuxEpoll&)            = delete;
    NativeLinuxEpoll& operator=(const NativeLinuxEpoll&) = delete;
    NativeLinuxEpoll(NativeLinuxEpoll&& other) noexcept;
    NativeLinuxEpoll& operator=(NativeLinuxEpoll&& other) noexcept;

    [[nodiscard]] std::unique_ptr<EpollRegistration> add(int file_descriptor,
                                                         EpollInterest interest,
                                                         std::uint64_t token) override;
    [[nodiscard]] std::size_t wait(std::span<EpollReadyEvent> events,
                                   std::optional<std::chrono::milliseconds> timeout) override;

    /** Returns the owned epoll descriptor, or -1 when closed. */
    [[nodiscard]] int native_handle() const noexcept;

    /** Returns whether this object owns an open epoll descriptor. */
    [[nodiscard]] bool is_open() const noexcept;

    /** Closes the epoll descriptor without closing registered descriptors. */
    void close() noexcept;

private:
    std::shared_ptr<detail::NativeEpollState> state;
    std::vector<epoll_event> native_events;
};

} // namespace exasol::udf::v2
