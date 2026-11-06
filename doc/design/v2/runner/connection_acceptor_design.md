# Connection Acceptor Design

`ConnectionAcceptor` supplies accepted connections to `Runner` without tying
the runner to Unix sockets. The first implementation accepts Unix-domain stream
sockets; TCP and TLS are reserved connection types for future implementations.

```text
client -> ConnectionAcceptor -> Runner -> worker -> ContextManager
```

## Interfaces and ownership

```cpp
enum class ConnectionType { UnixDomainSocket, Tcp, Tls };

class Connection {
public:
    virtual ConnectionType get_type() const noexcept = 0;
};

class ConnectionAcceptor {
public:
    virtual std::unique_ptr<Connection>
    get_next_connection(interface::Timeout timeout) = 0;
    virtual void shutdown() noexcept = 0;
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
    virtual std::unique_ptr<socket::Socket>
    get_next_socket(interface::Timeout timeout) = 0;
};
```

`UnixSocketListener` waits for the listening Unix descriptor, accepts one
client, and returns it as an owning `socket::Socket`. The
`CallerThreadSocketConnectionAcceptor` owns that listener and performs waiting
on the thread calling `get_next_connection`. It wraps an accepted socket in a
`SocketConnection` with `ConnectionType::UnixDomainSocket`.

`shutdown()` destroys the owned `SocketListener`. For a Unix listener this
closes the listener and removes its bound socket path. The caller must not race
`shutdown()` with `get_next_connection`; shutdown takes effect before the next
acceptance attempt rather than interrupting an in-progress one.

## Context handoff

The worker passes the accepted `Connection` to `ContextManager`. For socket
connection types, `ContextManager` obtains the `SocketConnection`, calls
`take_socket()`, and gives the resulting owning socket to the new context. A
future non-socket connection type may provide a separate `Connection`
implementation and context-creation branch.
