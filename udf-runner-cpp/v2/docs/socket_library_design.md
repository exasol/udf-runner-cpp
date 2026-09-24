# Socket Library Design

## Purpose

This document defines the public design for the v2 stream-socket library.  Its
first implementation supports Unix-domain stream sockets addressed by filesystem
paths.  The API is deliberately shaped so that TCP and TLS can be added without
changing code that consumes a `Socket`.

The design takes the useful separation between a common socket abstraction,
stream sockets, and server sockets from [Poco Net's socket package][poco]. It
uses explicit move-only ownership instead of shared or copyable socket state.

[poco]: https://docs.pocoproject.org/current/package-Net.Sockets.html

## Goals

- Provide an owning, move-only socket whose destructor closes its file
  descriptor.
- Provide a listener that accepts a connection as an owning `UnixSocket`.
- Make a descriptor available for integration with `poll`, `epoll`, and other
  POSIX readiness mechanisms.
- Provide partial-progress single-buffer and scatter/gather I/O.
- Keep the common stream contract implementable by future TCP and TLS
  transports.

## Scope boundaries

The following capabilities are needed by the broader networking stack, but
required by transport-specific or higher-level components rather than the core
`Socket` abstraction:

- TCP, TLS, datagram sockets, abstract-namespace Unix addresses, name
  resolution, and transport-specific socket-option policy.
- Timeouts, polling loops, and protocol framing.

Here the socket abstraction does not provide implicit unlinking of a filesystem path
before bind or during destruction, or thread safety
for concurrent mutation of an individual socket object.

The library is Linux/POSIX-specific. This follows the v2 library's existing use
of Linux readiness descriptors.

## Namespace and public headers

Public types belong to `exasol::udf::v2::socket`. The implementation should provide a
small public header surface, for example:

```
include/exasol/udf/v2/socket.hpp
include/exasol/udf/v2/unix_socket.hpp
```

The core header must not expose a TLS implementation or a TCP address type.

## Buffer types and vectored I/O

The API uses `std::span` rather than exposing `iovec` in its public contract.
This keeps the contract usable by TLS, whose I/O does not map one-for-one to
`readv` and `writev`.

```cpp
namespace exasol::udf::v2 {

enum class Shutdown { receive, send, both };

class OwnedFileDescriptor {
public:
    OwnedFileDescriptor() noexcept;
    ~OwnedFileDescriptor();

    // Takes ownership of a valid raw file descriptor.
    [[nodiscard]] static OwnedFileDescriptor adopt_native_handle(int owned_fd);

    OwnedFileDescriptor(const OwnedFileDescriptor&) = delete;
    OwnedFileDescriptor& operator=(const OwnedFileDescriptor&) = delete;
    OwnedFileDescriptor(OwnedFileDescriptor&&) noexcept;
    OwnedFileDescriptor& operator=(OwnedFileDescriptor&&) noexcept;

    [[nodiscard]] int native_handle() const noexcept;
    [[nodiscard]] bool is_open() const noexcept;
    void close() noexcept;
    [[nodiscard]] int release_native_handle() noexcept;

private:
    explicit OwnedFileDescriptor(int owned_fd) noexcept;
};

class Socket {
public:
    virtual ~Socket() = default;

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    [[nodiscard]] virtual int native_handle() const noexcept = 0;
    [[nodiscard]] virtual bool is_open() const noexcept = 0;

    virtual void close() noexcept = 0;
    [[nodiscard]] virtual OwnedFileDescriptor release_native_handle() noexcept = 0;
    virtual void shutdown(Shutdown how) = 0;

    virtual std::size_t read_some(
        std::span<const std::span<std::byte>> buffers) = 0;
    virtual std::size_t write_some(
        std::span<const std::span<const std::byte>> buffers) = 0;

protected:
    Socket() = default;
    Socket(Socket&&) = default;
    Socket& operator=(Socket&&) = default;
};

} // namespace exasol::udf::v2
```

The outer span and each contained byte span are non-owning. Their memory and
the array of byte spans passed to an I/O operation must remain valid until that
call returns. A call never changes caller-owned spans; its byte-count result
tells the caller how far to advance through the buffers.

For convenience, the implementation may offer non-virtual single-buffer
overloads that forward a one-element `std::span` to the virtual operations:

```cpp
std::size_t read_some(std::span<std::byte> buffer);
std::size_t write_some(std::span<const std::byte> buffer);
```

## `Socket` behavioral contract

`Socket` is the common, non-copyable stream interface. Concrete transports,
not the abstract base, are move-only RAII values. `OwnedFileDescriptor` is the
move-only RAII value used when ownership must leave a socket or listener.

- `native_handle()` returns a **borrowed** descriptor. It is suitable for
  readiness registration, but callers must neither close it nor retain it
  after the owning socket has been closed, moved, released, or destroyed.
- `release_native_handle()` transfers ownership to an `OwnedFileDescriptor`
  and leaves the socket closed. It returns an empty object when the socket is
  already closed. This is the only ownership escape hatch, so transferred
  descriptors are still closed automatically unless ownership is explicitly
  released again.
- `OwnedFileDescriptor::adopt_native_handle()` takes ownership of a valid raw
  descriptor received from an external API or another process. The caller must
  not close the raw descriptor after adoption; the wrapper closes it when it is
  destroyed unless ownership is moved or explicitly released.
- `close()` is idempotent and `noexcept`. It marks the object closed and makes
  a best-effort POSIX `close`; destruction invokes the same behavior. A close
  error cannot safely be retried and is not reported by this API.
- `shutdown()` maps to `::shutdown`. It does not relinquish descriptor
  ownership; `close()` remains necessary. System-call failure throws
  `std::system_error`.
- A `read_some` operation returns the number of bytes read. `0` is returned
  only when the peer has orderly shut down its send direction, or when every
  supplied buffer has zero length. A caller must distinguish those cases from
  the input it supplied.
- `write_some` returns the number of bytes written. It returns `0` only when
  every supplied buffer has zero length; a non-empty write either makes
  progress or throws.
- I/O validates the platform `IOV_MAX` limit and total byte count. Closed
  sockets and syscall failures throw `std::system_error` using
  `std::generic_category()`.
- I/O retries `EINTR`. It otherwise performs at most one kernel transfer
  operation, so partial progress is normal. `EAGAIN`/`EWOULDBLOCK` is reported
  as `std::system_error`; it is never converted to a zero-byte result.
- The library preserves the descriptor's blocking mode. It does not set
  `O_NONBLOCK`; callers select that mode and use `native_handle()` with
  `poll`/`epoll` as needed.

The first Unix implementation should map non-empty vector operations to
`readv` and `writev` (or equivalent `recvmsg`/`sendmsg` where needed). Exact
or all-buffer transfer helpers are intentionally not part of the base
interface; protocol layers can compose `*_some` calls with their own timeout
and cancellation policy.

## Unix sockets

```cpp
namespace exasol::udf::v2 {

class UnixSocket final : public Socket {
public:
    UnixSocket() noexcept;
    ~UnixSocket() override;

    UnixSocket(const UnixSocket&) = delete;
    UnixSocket& operator=(const UnixSocket&) = delete;
    UnixSocket(UnixSocket&&) noexcept;
    UnixSocket& operator=(UnixSocket&&) noexcept;

    [[nodiscard]] static UnixSocket connect(const std::filesystem::path& path);
    [[nodiscard]] static UnixSocket adopt_native_handle(
        OwnedFileDescriptor owned_fd);

    // Socket overrides
};

class UnixSocketListener {
public:
    UnixSocketListener() noexcept;
    ~UnixSocketListener();

    UnixSocketListener(const UnixSocketListener&) = delete;
    UnixSocketListener& operator=(const UnixSocketListener&) = delete;
    UnixSocketListener(UnixSocketListener&&) noexcept;
    UnixSocketListener& operator=(UnixSocketListener&&) noexcept;

    [[nodiscard]] static UnixSocketListener bind(
        const std::filesystem::path& path, int backlog = SOMAXCONN);

    [[nodiscard]] int native_handle() const noexcept;
    [[nodiscard]] bool is_open() const noexcept;
    void close() noexcept;
    [[nodiscard]] OwnedFileDescriptor release_native_handle() noexcept;
    [[nodiscard]] UnixSocket accept();
    void unlink_path();
};

} // namespace exasol::udf::v2
```

`UnixSocket::connect` creates a close-on-exec stream socket and connects it to
the supplied filesystem path. `adopt_native_handle` consumes an
`OwnedFileDescriptor` containing a valid Unix stream descriptor; it is the
explicit boundary for descriptor passing or interoperability. If adoption
fails, the wrapper remains responsible for closing the descriptor.

For example, a descriptor received from another process is adopted into RAII
ownership before it is converted to a socket:

```cpp
auto owned_fd = OwnedFileDescriptor::adopt_native_handle(received_fd);
auto socket = UnixSocket::adopt_native_handle(std::move(owned_fd));
```

If the second operation fails, the parameter object is destroyed and closes
the descriptor. The socket library does not define how `received_fd` is
obtained; that is the responsibility of the process-communication layer.

`UnixSocketListener::bind` creates a close-on-exec listener, binds it, and
starts listening. It rejects empty and overlong paths before the syscall. It
never removes an existing directory entry: an `EADDRINUSE` bind error is
returned to the caller rather than risking removal of another process's
socket. `accept()` accepts exactly one connection and returns it as an owning,
close-on-exec `UnixSocket`. In nonblocking mode it throws `std::system_error`
for would-block, just like stream I/O.

The listener retains the path supplied at bind only to support `unlink_path()`.
That operation is explicit and must be called only after closing the listener
and only when the caller knows it still owns the pathname. Destruction and
`close()` close the descriptor but never unlink the pathname.

## Future TCP and TLS

A future `TcpSocket final : Socket` will implement the same stream operations,
adding TCP endpoint and connect/listen types without changing `Socket` or its
buffer contract. TCP-specific settings such as keepalive and `TCP_NODELAY`
remain off the common base interface.

A future `TlsSocket final : Socket` will own a transport using
`std::unique_ptr<Socket>`. Its `native_handle()` exposes the underlying
transport descriptor as a borrowed readiness handle. TLS handshaking,
certificate configuration, and encrypted I/O remain TLS-specific APIs. The
TLS implementation must honor `read_some` and `write_some` for vector buffers,
even if it internally processes one segment at a time or uses temporary
contiguous storage. It must not reject vector I/O merely because the underlying
TLS engine lacks `readv`/`writev`.

## Concurrency and lifecycle

Socket and listener objects are not safe for simultaneous close, move, release,
or I/O from multiple threads. Different sockets may be used concurrently.
Callers that use one descriptor from multiple threads must serialize lifecycle
changes and define their own read/write concurrency policy.

All factory-created and accepted descriptors use close-on-exec. The API does
not silently change a caller-provided descriptor's blocking mode. Callers that
need a descriptor after object destruction must first call
`release_native_handle()` and retain the returned `OwnedFileDescriptor`.

## Implementation acceptance tests

The eventual implementation must cover:

1. Move construction, move assignment, explicit close, release, and destruction
   each close an owned descriptor at most once.
2. Bind, connect, accept, bidirectional data transfer, refusal to overwrite an
   existing path, and explicit pathname cleanup.
3. Single-buffer and multi-buffer transfers, including empty segments, partial
   writes, and transfers that stop in the middle of a buffer segment.
4. Peer EOF, directional shutdown, closed/invalid descriptor errors, nonblocking
   would-block errors, interrupted syscalls, and Unix-path length validation.
5. Shared contract tests that future TCP and TLS implementations can reuse to
   prove `Socket` substitutability, including vector I/O support.
6. Adoption of a received raw descriptor into `OwnedFileDescriptor`, successful
   conversion to `UnixSocket`, and cleanup when conversion fails.

## Legacy-library lessons retained in this design

The legacy socket code uses raw Unix listener descriptors, copies descriptors
without ownership semantics, mutates `iovec` values while completing a whole
transfer, and has TLS wrappers that cannot implement vectored direct reads and
writes. This design replaces those behaviors with explicit RAII ownership,
partial-progress buffer contracts, and a transport-neutral vector-I/O
requirement.
