# Linux epoll Abstraction Design

## Summary

Introduce two abstract v2/Linux interfaces:

- LinuxEpoll: owns the readiness-wait capability.
- EpollRegistration: owns one borrowed descriptor’s epoll registration and deregisters it on destruction.

NativeLinuxEpoll and its private native registration implementation use posix::Posix for every production system call. Tests may instead supply mock implementations of both public interfaces, without requiring real descriptors or POSIX interaction.

## Public interfaces

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

class EpollRegistration {
public:
    virtual ~EpollRegistration() = default;

    EpollRegistration(const EpollRegistration&) = delete;
    EpollRegistration& operator=(const EpollRegistration&) = delete;
    EpollRegistration(EpollRegistration&&) = delete;
    EpollRegistration& operator=(EpollRegistration&&) = delete;

    virtual void modify(EpollInterest interest, std::uint64_t token) = 0;
    virtual void remove() noexcept = 0;
    [[nodiscard]] virtual bool is_registered() const noexcept = 0;

protected:
    EpollRegistration() = default;
};

class LinuxEpoll {
public:
    virtual ~LinuxEpoll() = default;

    LinuxEpoll(const LinuxEpoll&) = delete;
    LinuxEpoll& operator=(const LinuxEpoll&) = delete;
    LinuxEpoll(LinuxEpoll&&) = delete;
    LinuxEpoll& operator=(LinuxEpoll&&) = delete;

    [[nodiscard]] virtual std::unique_ptr<EpollRegistration> add(
        int file_descriptor, EpollInterest interest, std::uint64_t token) = 0;

    [[nodiscard]] virtual std::size_t wait(
        std::span<EpollReadyEvent> events,
        std::optional<std::chrono::milliseconds> timeout) = 0;

protected:
    LinuxEpoll() = default;
};

class NativeLinuxEpoll final : public LinuxEpoll {
public:
    NativeLinuxEpoll();
    explicit NativeLinuxEpoll(std::shared_ptr<posix::Posix> posix_facade);
    ~NativeLinuxEpoll() override;

    [[nodiscard]] std::unique_ptr<EpollRegistration> add(
        int file_descriptor, EpollInterest interest, std::uint64_t token) override;
    [[nodiscard]] std::size_t wait(
        std::span<EpollReadyEvent> events,
        std::optional<std::chrono::milliseconds> timeout) override;

    NativeLinuxEpoll(const NativeLinuxEpoll&) = delete;
    NativeLinuxEpoll& operator=(const NativeLinuxEpoll&) = delete;
    NativeLinuxEpoll(NativeLinuxEpoll&&) noexcept;
    NativeLinuxEpoll& operator=(NativeLinuxEpoll&&) noexcept;

    [[nodiscard]] int native_handle() const noexcept;
    [[nodiscard]] bool is_open() const noexcept;
    void close() noexcept;
};

} // namespace exasol::udf::v2

LinuxEpoll has no public modify or remove: a registration is the sole lifecycle authority for its descriptor. EpollRegistration::modify() changes both readiness interest and token. remove() is idempotent, noexcept, and marks the registration inactive even if cleanup is already unnecessary.

## RAII and ownership

- NativeLinuxEpoll owns only the epoll descriptor created with EPOLL_CLOEXEC.
- add() receives a borrowed descriptor and returns std::unique_ptr<EpollRegistration>. The caller retains descriptor ownership.
- A live registration represents one successful EPOLL_CTL_ADD.
- Destroying a native registration performs best-effort EPOLL_CTL_DEL if its epoll instance remains open. Explicit remove() performs the same operation early.
- Destroying or closing NativeLinuxEpoll closes only the epoll descriptor. It never closes registered descriptors.
- Registrations may outlive their NativeLinuxEpoll safely: they retain a std::weak_ptr to internal epoll state. If the state has expired or its epoll descriptor is closed, removal becomes a no-op instead of accessing freed state or a reused descriptor.
- NativeLinuxEpoll uses std::exchange in its idempotent close() path. Destruction and move assignment both invoke that path. Move construction transfers the state and leaves the source closed.
- add() first creates an inactive registration object, performs EPOLL_CTL_ADD, then marks it active. Therefore, an allocation failure cannot leave an unowned kernel registration behind.

## Native implementation and POSIX facade

Add public headers for the two interfaces and NativeLinuxEpoll, plus implementation files for the native epoll and registration internals.

Extend posix::Posix and posix::Native with:

virtual int epoll_create1(int flags) = 0;
virtual int epoll_ctl(int epoll_fd, int operation, int file_descriptor,
                      epoll_event* event) = 0;
virtual int epoll_wait(int epoll_fd, epoll_event* events,
                       int maximum_events, int timeout) = 0;

The native implementation translates EpollInterest to level-triggered EPOLLIN / EPOLLOUT, stores tokens in data.u64, and maps readable, writable, error, hangup, and peer-close flags into EpollReadyEvent.

wait() rejects an empty result span and invalid finite timeout. It reuses an internal native-event buffer, retries EINTR while preserving a finite deadline, and throws std::system_error for other failures.

## Tests and integration

- Add mock LinuxEpoll and EpollRegistration tests to prove consumers can be tested without Linux or POSIX.
- Test native creation, level-trigger mapping, event/token translation, timeout, interrupted wait, and POSIX failures through an injected Posix.
- Test registration destruction, explicit idempotent removal, modification, move/destroy ordering, and a registration outliving its epoll instance.
- Verify epoll destruction never closes registered eventfd or socket descriptors.
- Extend every existing Posix mock/recording fake with the three epoll methods.
- Replace raw epoll setup in the waitable-queue integration test with NativeLinuxEpoll; retain raw benchmark epoll calls to preserve the benchmark’s kernel-only measurement.
- Add the approved ticket and changelog entry, and record the reviewed design under doc/design/v2/runner/.

## Assumptions

- v2/Linux only; v1 remains unchanged.
- The initial API exposes only level-triggered readable/writable interest.
- Caller tokens are the sole ready-event identity.
- No edge-triggered, one-shot, timer, callback registry, or thread-safety contract is included.
