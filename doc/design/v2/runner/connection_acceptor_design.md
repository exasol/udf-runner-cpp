# Connection Acceptor Design

`ConnectionAcceptor` supplies accepted connections to `Runner` without tying
the runner to Unix sockets. The first implementation accepts Unix-domain stream
sockets; TCP is reserved connection type for future implementations.

```text
client -> ConnectionAcceptor -> Runner -> worker -> ContextManager
```

## Interfaces and ownership

```cpp
enum class ConnectionType { UnixDomainSocket, Tcp };
using ConnectionTimeout = std::optional<std::chrono::steady_clock::duration>;

class Connection {
public:
    virtual ConnectionType get_type() const noexcept = 0;
};

class ConnectionAcceptor {
public:
    virtual std::unique_ptr<Connection>
    get_next_connection(ConnectionTimeout timeout) = 0;
    virtual void shutdown() = 0;
};
```

`nullptr` from `get_next_connection` means the timeout elapsed or the acceptor
has been shut down. Listener failures are reported as `std::system_error`.
`std::unique_ptr` is used instead of `optional<Connection>` because accepted
connections are polymorphic and have a single owner.

`SocketConnection` is the first `Connection` implementation. It owns a
`std::unique_ptr<socket::Socket>` and identifies its transport with
`ConnectionType`. `take_socket()` transfers the socket to the context. The
context is then the socket's sole owner and may use its `read_some`,
`write_some`, `native_handle`, or descriptor-release operations directly.
There is no raw-file-descriptor handoff or socket factory in this path.

## Socket acceptance

`SocketListener` is the socket-specific boundary:

```cpp
class SocketListener {
public:
    virtual int native_handle() const noexcept = 0;
    virtual std::unique_ptr<socket::Socket> accept_socket() = 0;
};
```

`CallerThreadSocketConnectionAcceptor` owns a `SocketListener`, an epoll
instance, and a shutdown `EventFd`. During construction it registers the
listener's borrowed descriptor and the eventfd with epoll. On the caller
thread, `get_next_connection` waits on epoll for at most the requested timeout.
Listener readiness causes `accept_socket()` and wraps the result in a
`SocketConnection` with the `ConnectionType` supplied when the acceptor is
constructed (currently `ConnectionType::UnixDomainSocket`).

`shutdown()` is safe to call while a caller is waiting. It records the shutdown
state under a mutex and writes to the eventfd; epoll then wakes the waiting
caller, which returns `nullptr`. A shutdown event takes priority over listener
readiness, and all later calls return `nullptr` without waiting. Shutdown does
not close or destroy the listener: the acceptor destructor destroys the
listener only after its caller has stopped using the acceptor. Concurrent
`get_next_connection` calls are serialized.

## Context handoff

The worker passes the accepted `Connection` to `ContextManager`. For socket
connection types, `ContextManager` obtains the `SocketConnection`, calls
`take_socket()`, and gives the resulting owning socket to the new context. A
future non-socket connection type may provide a separate `Connection`
implementation and context-creation branch.
