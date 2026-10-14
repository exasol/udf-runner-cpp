#include <exasol/udf/v2/socket/unix_socket_listener.hpp>

#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <span>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <gtest/gtest.h>

namespace
{

using exasol::udf::v2::socket::OwnedFileDescriptor;
using exasol::udf::v2::socket::Shutdown;
using exasol::udf::v2::socket::UnixSocket;
using exasol::udf::v2::socket::UnixSocketListener;

void expect_system_error(const auto& function, const std::errc expected)
{
    try
    {
        function();
        ADD_FAILURE() << "expected std::system_error";
    }
    catch (const std::system_error& error)
    {
        EXPECT_EQ(error.code(), std::make_error_code(expected));
    }
}

class UnixSocketPathTest : public testing::Test
{
protected:
    void SetUp() override
    {
        // Each test gets an isolated directory so socket paths cannot collide.
        auto directory_template = std::to_array(P_tmpdir "/udf-runner-cpp-socket-XXXXXX"); // NOSONAR
        char* directory         = ::mkdtemp(directory_template.data()); // NOSONAR
        ASSERT_NE(directory, nullptr);
        temporary_directory = directory;
    }

    void TearDown() override
    {
        // The listener owns its path; the test removes its private directory.
        std::error_code error;
        std::filesystem::remove_all(temporary_directory, error);
        EXPECT_FALSE(error) << error.message();
    }

    [[nodiscard]] std::filesystem::path get_socket_path() const
    {
        return temporary_directory / "socket";
    }

private:
    std::filesystem::path temporary_directory;
};

TEST_F(UnixSocketPathTest, ListenerConnectsAcceptsAndCleansUpPath)
{
    const std::filesystem::path path = get_socket_path();
    UnixSocketListener listener      = UnixSocketListener::bind(path);
    ASSERT_TRUE(std::filesystem::exists(path));
    // Connecting and accepting through the path verifies both listener endpoints.
    UnixSocket client = UnixSocket::connect(path);
    UnixSocket server = listener.accept();

    const std::array<std::byte, 2> sent{std::byte{'o'}, std::byte{'k'}};
    ASSERT_EQ(client.write_some(sent), sent.size());
    std::array<std::byte, 2> received{};
    ASSERT_EQ(server.read_some(received), received.size());
    EXPECT_EQ(received, sent);

    listener.close();
    EXPECT_FALSE(std::filesystem::exists(path));
}

TEST_F(UnixSocketPathTest, AcceptUsesCallerProvidedFlags)
{
    UnixSocketListener listener = UnixSocketListener::bind(get_socket_path());

    UnixSocket blocking_client          = UnixSocket::connect(get_socket_path());
    UnixSocket blocking_server          = listener.accept(0);
    const int blocking_status_flags     = ::fcntl(blocking_server.native_handle(), F_GETFL);
    const int blocking_descriptor_flags = ::fcntl(blocking_server.native_handle(), F_GETFD);
    ASSERT_NE(blocking_status_flags, -1);
    ASSERT_NE(blocking_descriptor_flags, -1);
    const auto blocking_status     = static_cast<unsigned int>(blocking_status_flags);
    const auto blocking_descriptor = static_cast<unsigned int>(blocking_descriptor_flags);
    EXPECT_EQ(blocking_status & static_cast<unsigned int>(O_NONBLOCK), 0U);
    EXPECT_EQ(blocking_descriptor & static_cast<unsigned int>(FD_CLOEXEC), 0U);

    UnixSocket nonblocking_client          = UnixSocket::connect(get_socket_path());
    UnixSocket nonblocking_server          = listener.accept(SOCK_NONBLOCK | SOCK_CLOEXEC);
    const int nonblocking_status_flags     = ::fcntl(nonblocking_server.native_handle(), F_GETFL);
    const int nonblocking_descriptor_flags = ::fcntl(nonblocking_server.native_handle(), F_GETFD);
    ASSERT_NE(nonblocking_status_flags, -1);
    ASSERT_NE(nonblocking_descriptor_flags, -1);
    const auto nonblocking_status     = static_cast<unsigned int>(nonblocking_status_flags);
    const auto nonblocking_descriptor = static_cast<unsigned int>(nonblocking_descriptor_flags);
    EXPECT_NE(nonblocking_status & static_cast<unsigned int>(O_NONBLOCK), 0U);
    EXPECT_NE(nonblocking_descriptor & static_cast<unsigned int>(FD_CLOEXEC), 0U);
}

TEST_F(UnixSocketPathTest, ConnectReportsAddressError)
{
    const std::filesystem::path path = get_socket_path().parent_path() / "missing" / "socket";
    expect_system_error([path] { static_cast<void>(UnixSocket::connect(path)); },
                        std::errc::no_such_file_or_directory);
}

TEST_F(UnixSocketPathTest, ListenerDoesNotOverwriteExistingPath)
{
    const std::filesystem::path path = get_socket_path();
    UnixSocketListener listener      = UnixSocketListener::bind(path);
    expect_system_error([path] { static_cast<void>(UnixSocketListener::bind(path)); },
                        std::errc::address_in_use);
    listener.close();
    EXPECT_FALSE(std::filesystem::exists(path));
}

TEST_F(UnixSocketPathTest, BindReportsAddressError)
{
    const std::filesystem::path path = get_socket_path().parent_path() / "missing" / "socket";
    expect_system_error([path] { static_cast<void>(UnixSocketListener::bind(path)); },
                        std::errc::no_such_file_or_directory);
}

TEST_F(UnixSocketPathTest, DefaultListenerCanBeInspectedAndReleased)
{
    UnixSocketListener listener;
    EXPECT_FALSE(listener.is_open());
    EXPECT_EQ(listener.native_handle(), -1);
    EXPECT_FALSE(listener.release_native_handle().is_open());
}

TEST_F(UnixSocketPathTest, AcceptOnClosedListenerReportsError)
{
    UnixSocketListener listener;
    expect_system_error([&listener] { static_cast<void>(listener.accept()); },
                        std::errc::bad_file_descriptor);
}

TEST_F(UnixSocketPathTest, SelfMoveAssignmentDoesNothing)
{
    UnixSocketListener listener = UnixSocketListener::bind(get_socket_path());
    listener = std::move(listener);
    EXPECT_TRUE(listener.is_open());
    EXPECT_TRUE(std::filesystem::exists(get_socket_path()));
}

TEST_F(UnixSocketPathTest, RejectsEmptyPath)
{
    EXPECT_THROW(static_cast<void>(UnixSocketListener::bind({})), std::invalid_argument);
}

TEST_F(UnixSocketPathTest, DestructionCleansUpPath)
{
    const std::filesystem::path path = get_socket_path();
    {
        UnixSocketListener listener = UnixSocketListener::bind(path);
        EXPECT_TRUE(std::filesystem::exists(path));
    }
    EXPECT_FALSE(std::filesystem::exists(path));
}

TEST_F(UnixSocketPathTest, ReleasingDescriptorCleansUpPath)
{
    const std::filesystem::path path = get_socket_path();
    UnixSocketListener listener      = UnixSocketListener::bind(path);

    OwnedFileDescriptor released = listener.release_native_handle();

    EXPECT_TRUE(released.is_open());
    EXPECT_FALSE(std::filesystem::exists(path));
}

TEST_F(UnixSocketPathTest, MoveAssignmentCleansUpReplacedPath)
{
    const std::filesystem::path first_path  = get_socket_path();
    const std::filesystem::path second_path = first_path.parent_path() / "second_socket";
    UnixSocketListener source               = UnixSocketListener::bind(first_path);
    UnixSocketListener destination          = UnixSocketListener::bind(second_path);

    destination = std::move(source);

    EXPECT_TRUE(std::filesystem::exists(first_path));
    EXPECT_FALSE(std::filesystem::exists(second_path));
    destination.close();
    EXPECT_FALSE(std::filesystem::exists(first_path));
}

TEST(UnixSocketTest, SupportsScatterGatherIo)
{
    std::array<int, 2> sockets{};
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets.data()), 0);
    // Wrap both ends so descriptor ownership is transferred to UnixSocket.
    UnixSocket writer =
        UnixSocket::adopt_native_handle(OwnedFileDescriptor::adopt_native_handle(sockets[0]));
    UnixSocket reader =
        UnixSocket::adopt_native_handle(OwnedFileDescriptor::adopt_native_handle(sockets[1]));
    const std::array<std::byte, 2> first{std::byte{'a'}, std::byte{'b'}};
    const std::array<std::byte, 2> second{std::byte{'c'}, std::byte{'d'}};
    // Send two buffers in one operation and verify that the peer sees them contiguously.
    const std::array<std::span<const std::byte>, 2> buffers{first, second};
    ASSERT_EQ(writer.write_some(buffers), 4);
    std::array<std::byte, 4> received{};
    ASSERT_EQ(reader.read_some(received), 4);
    EXPECT_EQ(received, (std::array<std::byte, 4>{std::byte{'a'}, std::byte{'b'}, std::byte{'c'},
                                                  std::byte{'d'}}));
}

TEST(UnixSocketTest, ReadReturnsZeroAtPeerEof)
{
    std::array<int, 2> sockets{};
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets.data()), 0);
    UnixSocket writer =
        UnixSocket::adopt_native_handle(OwnedFileDescriptor::adopt_native_handle(sockets[0]));
    UnixSocket reader =
        UnixSocket::adopt_native_handle(OwnedFileDescriptor::adopt_native_handle(sockets[1]));
    writer.close();
    std::array<std::byte, 1> eof_buffer{};
    // Closing the writer must make the reader observe peer EOF.
    EXPECT_EQ(reader.read_some(eof_buffer), 0);
}

// A nonblocking read maps EAGAIN to resource_unavailable_try_again.
TEST(UnixSocketTest, HandlesNonblockingRead)
{
    std::array<int, 2> sockets{};
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets.data()), 0);
    UnixSocket reader =
        UnixSocket::adopt_native_handle(OwnedFileDescriptor::adopt_native_handle(sockets[0]));
    // Enable nonblocking mode so read_some returns immediately when no data is available.
    ASSERT_NE(::fcntl(reader.native_handle(), F_SETFL, O_NONBLOCK), -1);
    std::array<std::byte, 1> buffer{};
    expect_system_error([&reader, &buffer] { static_cast<void>(reader.read_some(buffer)); },
                        std::errc::resource_unavailable_try_again);
    ASSERT_EQ(::close(sockets[1]), 0);
}

TEST(UnixSocketTest, SendShutdownDisablesOnlyWrites)
{
    std::array<int, 2> sockets{};
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets.data()), 0);
    UnixSocket writer =
        UnixSocket::adopt_native_handle(OwnedFileDescriptor::adopt_native_handle(sockets[0]));
    UnixSocket reader =
        UnixSocket::adopt_native_handle(OwnedFileDescriptor::adopt_native_handle(sockets[1]));
    writer.shutdown(Shutdown::Send);

    const std::array<std::byte, 1> data{std::byte{'x'}};
    expect_system_error([&writer, &data] { static_cast<void>(writer.write_some(data)); },
                        std::errc::broken_pipe);

    ASSERT_EQ(reader.write_some(data), data.size());
    std::array<std::byte, 1> received{};
    ASSERT_EQ(writer.read_some(received), received.size());
    EXPECT_EQ(received, data);
}

TEST(UnixSocketTest, ReleasesDescriptor)
{
    std::array<int, 2> sockets{};
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets.data()), 0);
    UnixSocket socket =
        UnixSocket::adopt_native_handle(OwnedFileDescriptor::adopt_native_handle(sockets[0]));
    OwnedFileDescriptor peer = OwnedFileDescriptor::adopt_native_handle(sockets[1]);

    OwnedFileDescriptor released = socket.release_native_handle();
    // Releasing transfers the descriptor out without closing it.
    EXPECT_FALSE(socket.is_open());
    EXPECT_TRUE(released.is_open());
}

TEST(UnixSocketTest, SupportsMoveConstructionAndAssignment)
{
    std::array<int, 2> sockets{};
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets.data()), 0);
    UnixSocket source =
        UnixSocket::adopt_native_handle(OwnedFileDescriptor::adopt_native_handle(sockets[0]));
    UnixSocket moved(std::move(source));
    EXPECT_FALSE(source.is_open());
    EXPECT_TRUE(moved.is_open());

    UnixSocket destination =
        UnixSocket::adopt_native_handle(OwnedFileDescriptor::adopt_native_handle(sockets[1]));
    destination = std::move(moved);
    EXPECT_FALSE(moved.is_open());
    EXPECT_TRUE(destination.is_open());
}

TEST(UnixSocketTest, RejectsClosedDescriptor)
{
    EXPECT_THROW(static_cast<void>(UnixSocket::adopt_native_handle({})), std::system_error);
}

TEST(UnixSocketTest, RejectsNonUnixSocketDescriptor)
{
    const int descriptor = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    ASSERT_NE(descriptor, -1);
    OwnedFileDescriptor owned = OwnedFileDescriptor::adopt_native_handle(descriptor);
    EXPECT_THROW(static_cast<void>(UnixSocket::adopt_native_handle(std::move(owned))),
                 std::system_error);
}

TEST(UnixSocketTest, HandlesEmptyIoBuffers)
{
    std::array<int, 2> sockets{};
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets.data()), 0);
    UnixSocket socket =
        UnixSocket::adopt_native_handle(OwnedFileDescriptor::adopt_native_handle(sockets[0]));
    const std::span<const std::span<std::byte>> empty_read{};
    const std::span<const std::span<const std::byte>> empty_write{};
    EXPECT_EQ(socket.read_some(empty_read), 0U);
    EXPECT_EQ(socket.write_some(empty_write), 0U);
    ASSERT_EQ(::close(sockets[1]), 0);
}

TEST(UnixSocketTest, RejectsTooManyIoBuffers)
{
    std::array<int, 2> sockets{};
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets.data()), 0);
    UnixSocket socket =
        UnixSocket::adopt_native_handle(OwnedFileDescriptor::adopt_native_handle(sockets[0]));
    std::array<std::byte, 1> byte{};
    std::array<std::span<std::byte>, IOV_MAX + 1> buffers{};
    buffers.fill(byte);
    EXPECT_THROW(static_cast<void>(socket.read_some(buffers)), std::system_error);
    ASSERT_EQ(::close(sockets[1]), 0);
}

TEST(UnixSocketTest, HandlesAllShutdownDirectionsAndInvalidDirection)
{
    std::array<int, 2> sockets{};
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets.data()), 0);
    UnixSocket socket =
        UnixSocket::adopt_native_handle(OwnedFileDescriptor::adopt_native_handle(sockets[0]));
    EXPECT_NO_THROW(socket.shutdown(Shutdown::Receive));
    EXPECT_NO_THROW(socket.shutdown(Shutdown::Send));
    EXPECT_NO_THROW(socket.shutdown(Shutdown::Both));
    EXPECT_THROW(socket.shutdown(static_cast<Shutdown>(-1)), std::logic_error);
    ASSERT_EQ(::close(sockets[1]), 0);
}

TEST(UnixSocketTest, ReportsShutdownAndReleaseOnClosedSocket)
{
    UnixSocket socket;
    EXPECT_THROW(socket.shutdown(Shutdown::Both), std::system_error);
    EXPECT_FALSE(socket.release_native_handle().is_open());
}

} // namespace
