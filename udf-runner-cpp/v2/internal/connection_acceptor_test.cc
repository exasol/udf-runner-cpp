#include <exasol/udf/v2/internal/caller_thread_socket_connection_acceptor.hpp>
#include <exasol/udf/v2/internal/connection.hpp>
#include <exasol/udf/v2/linux_epoll.hpp>
#include <exasol/udf/v2/posix/posix.hpp>

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace
{

using exasol::udf::v2::EpollInterest;
using exasol::udf::v2::EpollReadyEvent;
using exasol::udf::v2::EpollRegistration;
using exasol::udf::v2::EventFd;
using exasol::udf::v2::LinuxEpoll;
using exasol::udf::v2::interface::Timeout;
using exasol::udf::v2::internal::CallerThreadSocketConnectionAcceptor;
using exasol::udf::v2::internal::ConnectionType;
using exasol::udf::v2::internal::SocketConnection;
using exasol::udf::v2::socket::OwnedFileDescriptor;
using exasol::udf::v2::socket::Shutdown;
using exasol::udf::v2::socket::Socket;
using exasol::udf::v2::socket::SocketListener;

class FakeSocket final : public Socket
{
public:
    using Socket::read_some;
    using Socket::write_some;

    [[nodiscard]] int native_handle() const noexcept override
    {
        return -1;
    }

    [[nodiscard]] bool is_open() const noexcept override
    {
        return false;
    }

    void close() noexcept override
    {
        // The fake does not own a native resource.
    }

    [[nodiscard]] OwnedFileDescriptor release_native_handle() noexcept override
    {
        return OwnedFileDescriptor(std::make_shared<exasol::udf::v2::posix::Native>());
    }

    void shutdown(const Shutdown how) override
    {
        static_cast<void>(how);
    }

    std::size_t read_some(const std::span<const std::span<std::byte>> buffers) override
    {
        static_cast<void>(buffers);
        return 0;
    }

    std::size_t write_some(const std::span<const std::span<const std::byte>> buffers) override
    {
        static_cast<void>(buffers);
        return 0;
    }
};

class FakeSocketListener final : public SocketListener
{
public:
    FakeSocketListener(std::unique_ptr<Socket> socket, int* destruction_count)
        : socket(std::move(socket)), destruction_count(destruction_count)
    {
    }

    ~FakeSocketListener() override
    {
        ++*this->destruction_count;
    }

    FakeSocketListener(const FakeSocketListener&)            = delete;
    FakeSocketListener& operator=(const FakeSocketListener&) = delete;
    FakeSocketListener(FakeSocketListener&&)                 = delete;
    FakeSocketListener& operator=(FakeSocketListener&&)      = delete;

    [[nodiscard]] int native_handle() const noexcept override
    {
        return 42;
    }

    [[nodiscard]] std::unique_ptr<Socket> accept_socket() override
    {
        return std::move(this->socket);
    }

private:
    std::unique_ptr<Socket> socket;
    int* destruction_count;
};

class FakeEpollRegistration final : public EpollRegistration
{
public:
    void modify(const EpollInterest interest, const std::uint64_t token) override
    {
        static_cast<void>(interest);
        static_cast<void>(token);
    }
};

class FakeEpoll final : public LinuxEpoll
{
public:
    [[nodiscard]] std::unique_ptr<EpollRegistration> add(const int file_descriptor,
                                                         const EpollInterest interest,
                                                         const std::uint64_t token) override
    {
        static_cast<void>(file_descriptor);
        static_cast<void>(interest);
        static_cast<void>(token);
        return std::make_unique<FakeEpollRegistration>();
    }

    void wait(std::vector<EpollReadyEvent>& target,
              const std::optional<std::chrono::milliseconds> timeout) override
    {
        static_cast<void>(timeout);
        target = std::move(events);
    }

    void set_events(std::vector<EpollReadyEvent> ready_events)
    {
        events = std::move(ready_events);
    }

private:
    std::vector<EpollReadyEvent> events;
};

class FakeEventFd final : public EventFd
{
public:
    [[nodiscard]] int native_handle() const noexcept override
    {
        return 43;
    }

    std::uint64_t read_notification() override
    {
        return 1;
    }

    void write_notification() override
    {
        ++write_count;
    }

    [[nodiscard]] int get_write_count() const noexcept
    {
        return write_count;
    }

private:
    int write_count = 0;
};

class BlockingEpoll final : public LinuxEpoll
{
public:
    [[nodiscard]] std::unique_ptr<EpollRegistration> add(const int file_descriptor,
                                                         const EpollInterest interest,
                                                         const std::uint64_t token) override
    {
        static_cast<void>(file_descriptor);
        static_cast<void>(interest);
        static_cast<void>(token);
        return std::make_unique<FakeEpollRegistration>();
    }

    void wait(std::vector<EpollReadyEvent>& target,
              const std::optional<std::chrono::milliseconds> timeout) override
    {
        static_cast<void>(timeout);
        std::unique_lock lock(this->mutex);
        this->waiting = true;
        this->state_changed.notify_all();
        this->state_changed.wait(lock, [this] { return this->shutdown_signalled; });
        target = {{.token = 1, .readable = true}};
    }

    void wait_until_waiting()
    {
        std::unique_lock lock(this->mutex);
        this->state_changed.wait(lock, [this] { return this->waiting; });
    }

    void signal_shutdown()
    {
        {
            std::lock_guard lock(this->mutex);
            this->shutdown_signalled = true;
        }
        this->state_changed.notify_all();
    }

private:
    std::condition_variable state_changed;
    std::mutex mutex;
    bool waiting            = false;
    bool shutdown_signalled = false;
};

class SignallingEventFd final : public EventFd
{
public:
    explicit SignallingEventFd(BlockingEpoll& epoll) : epoll(epoll)
    {
    }

    [[nodiscard]] int native_handle() const noexcept override
    {
        return 43;
    }

    std::uint64_t read_notification() override
    {
        return 1;
    }

    void write_notification() override
    {
        this->epoll.signal_shutdown();
    }

private:
    BlockingEpoll& epoll;
};

TEST(SocketConnectionTest, TransfersSocketOwnershipOnce)
{
    auto socket                   = std::make_unique<FakeSocket>();
    Socket* const expected_socket = socket.get();
    SocketConnection connection(ConnectionType::Tcp, std::move(socket));

    EXPECT_EQ(connection.get_type(), ConnectionType::Tcp);
    std::unique_ptr<Socket> transferred_socket = connection.take_socket();
    EXPECT_EQ(transferred_socket.get(), expected_socket);
    EXPECT_EQ(connection.take_socket(), nullptr);
}

TEST(CallerThreadSocketConnectionAcceptorTest, WrapsAcceptedSocketAsUnixConnection)
{
    int destruction_count         = 0;
    auto socket                   = std::make_unique<FakeSocket>();
    Socket* const expected_socket = socket.get();
    auto epoll                    = std::make_unique<FakeEpoll>();
    epoll->set_events({{.token = 0, .readable = true}});
    auto event = std::make_unique<FakeEventFd>();
    CallerThreadSocketConnectionAcceptor acceptor(
        std::make_unique<FakeSocketListener>(std::move(socket), &destruction_count),
        std::move(epoll), std::move(event));

    std::unique_ptr<exasol::udf::v2::internal::Connection> connection =
        acceptor.get_next_connection(Timeout{});

    ASSERT_NE(connection, nullptr);
    EXPECT_EQ(connection->get_type(), ConnectionType::UnixDomainSocket);
    auto* socket_connection = dynamic_cast<SocketConnection*>(connection.get());
    ASSERT_NE(socket_connection, nullptr);
    std::unique_ptr<Socket> transferred_socket = socket_connection->take_socket();
    EXPECT_EQ(transferred_socket.get(), expected_socket);
    EXPECT_EQ(destruction_count, 0);
}

TEST(CallerThreadSocketConnectionAcceptorTest, ShutdownSignalsButDoesNotDestroyListener)
{
    int destruction_count                  = 0;
    auto event                             = std::make_unique<FakeEventFd>();
    const FakeEventFd* const event_pointer = event.get();
    {
        CallerThreadSocketConnectionAcceptor acceptor(
            std::make_unique<FakeSocketListener>(nullptr, &destruction_count),
            std::make_unique<FakeEpoll>(), std::move(event));

        acceptor.shutdown();
        EXPECT_EQ(event_pointer->get_write_count(), 1);
        EXPECT_EQ(destruction_count, 0);
        EXPECT_EQ(acceptor.get_next_connection(Timeout{}), nullptr);
    }
    EXPECT_EQ(destruction_count, 1);
}

TEST(CallerThreadSocketConnectionAcceptorTest, ShutdownWakesAnUnlimitedWait)
{
    int destruction_count              = 0;
    auto epoll                         = std::make_unique<BlockingEpoll>();
    BlockingEpoll* const epoll_pointer = epoll.get();
    CallerThreadSocketConnectionAcceptor acceptor(
        std::make_unique<FakeSocketListener>(nullptr, &destruction_count), std::move(epoll),
        std::make_unique<SignallingEventFd>(*epoll_pointer));

    std::promise<bool> connection_result;
    std::future<bool> waiting_connection = connection_result.get_future();
    std::thread waiting_thread([&acceptor, &connection_result] {
        connection_result.set_value(acceptor.get_next_connection(Timeout{}) == nullptr);
    });
    epoll_pointer->wait_until_waiting();

    acceptor.shutdown();

    waiting_thread.join();
    EXPECT_TRUE(waiting_connection.get());
}

} // namespace
