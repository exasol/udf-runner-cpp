# Linux epoll Abstraction Design

## Summary

Add a v2-only LinuxEpoll move-only RAII type. It exclusively owns the epoll instance descriptor; every registered socket, eventfd, or other descriptor remains borrowed and owned by its caller.

All production POSIX calls go through posix::Posix: epoll_create1, epoll_ctl, epoll_wait, and close.

## Public interface

namespace exasol::udf::v2 {

struct EpollInterest {
    bool readable = false;
    bool writable = false;
};

struct EpollReadyEvent {
    std::uint64_t token = 0;
    bool readable = false;
    bool writable = false;
    bool error = false;
    bool hangup = false;
    bool peer_closed = false;
};

class LinuxEpoll final {
public:
    LinuxEpoll();
    explicit LinuxEpoll(std::shared_ptr<posix::Posix> posix_facade);
    ~LinuxEpoll() noexcept;

    LinuxEpoll(const LinuxEpoll&) = delete;
    LinuxEpoll& operator=(const LinuxEpoll&) = delete;
    LinuxEpoll(LinuxEpoll&& other) noexcept;
    LinuxEpoll& operator=(LinuxEpoll&& other) noexcept;

    [[nodiscard]] int native_handle() const noexcept;
    [[nodiscard]] bool is_open() const noexcept;
    void close() noexcept;

    void add(int file_descriptor, EpollInterest interest, std::uint64_t token);
    void modify(int file_descriptor, EpollInterest interest, std::uint64_t token);
    void remove(int file_descriptor);

    [[nodiscard]] std::size_t wait(
        std::span<EpollReadyEvent> events,
        std::optional<std::chrono::milliseconds> timeout);

private:
    int file_descriptor = -1;
    std::shared_ptr<posix::Posix> posix;
    std::vector<epoll_event> native_events;
};

} // namespace exasol::udf::v2

EpollInterest supports level-triggered readable and writable interest only. EpollReadyEvent::token is the caller-provided epoll_event.data.u64 value; it is not a descriptor.

## RAII and lifecycle contract

The private cleanup path is:

void LinuxEpoll::close() noexcept
{
    const int closed_fd = std::exchange(this->file_descriptor, -1);
    if (closed_fd != -1)
    {
        static_cast<void>(this->posix->close(closed_fd));
    }
}

- The destructor calls close().
- Move construction transfers the descriptor and POSIX facade, leaving the source closed (native_handle() == -1).
- Move assignment first calls close() on its current descriptor, then transfers the source; self-move is a no-op.
- close() is idempotent, best-effort, and noexcept.
- Creation uses EPOLL_CLOEXEC; failure throws std::system_error.
- Closing the epoll instance only releases the epoll descriptor and its kernel registration set. It never closes registered descriptors.
- add, modify, and remove accept borrowed descriptors. The caller remains responsible for their lifetime. The preferred lifecycle is to remove a registration before closing or reusing its descriptor.

## POSIX facade and behavior

Extend posix::Posix and posix::Native with epoll_create1, epoll_ctl, and epoll_wait. Native directly forwards each call.

LinuxEpoll translates interests to EPOLLIN / EPOLLOUT, stores tokens in data.u64, and maps EPOLLIN, EPOLLOUT, EPOLLERR, EPOLLHUP, and EPOLLRDHUP into EpollReadyEvent.

wait() validates a non-empty output span and a valid millisecond timeout. nullopt means infinite wait; 0ms polls. It reuses the private native_events buffer, avoiding repeated allocations once the largest requested output capacity has been reached. It retries EINTR while recomputing a finite timeout against a steady-clock deadline; other failures throw std::system_error.

## Tests and integration

- Test creation, RAII destruction, idempotent close, move construction, move assignment, and self-move with an injected recording facade.
- Verify that destination cleanup occurs exactly once before move assignment takes ownership.
- Test add, modify, remove, token forwarding, readiness-flag translation, invalid arguments, native failures, timeout, and interrupted wait.
- Add native integration coverage for separate eventfd and socket tokens.
- Extend all existing Posix mocks/recording fakes with the new pure virtual methods.
- Replace raw epoll setup in the waitable-queue integration test; keep raw epoll in the benchmark to preserve its kernel-only measurement.
- Add the approved ticket to the unreleased changelog and add this design under doc/design/v2/runner/ before implementation.

## Assumptions

- v2/Linux only; no v1 changes.
- No edge-triggered, one-shot, callback, timer, descriptor registry, or thread-safety feature in the first version.
- Registered descriptors outlive their registration and are never owned by LinuxEpoll.
