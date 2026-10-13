#include <exasol/udf/v2/socket/unix_socket_listener.hpp>

#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
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
        auto directory_template = std::to_array(P_tmpdir "/udf-runner-cpp-socket-XXXXXX");
        char* directory         = ::mkdtemp(directory_template.data());
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

TEST_F(UnixSocketPathTest, ValidatesPathsAndCleansUpOnDestruction)
{
    EXPECT_THROW(static_cast<void>(UnixSocketListener::bind({})), std::invalid_argument);
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

TEST(UnixSocketTest, SupportsScatterGatherIoAndPeerEof)
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
    writer.close();
    std::array<std::byte, 1> eof_buffer{};
    // Closing the writer must make the reader observe peer EOF.
    EXPECT_EQ(reader.read_some(eof_buffer), 0);
}

// A nonblocking read reports EAGAIN when no data is available.
TEST(UnixSocketTest, NonblockingReadWouldBlock)
{
    std::array<int, 2> sockets{};
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets.data()), 0);
    UnixSocket reader =
        UnixSocket::adopt_native_handle(OwnedFileDescriptor::adopt_native_handle(sockets[0]));
    // With no data and O_NONBLOCK enabled, read_some must fail without waiting.
    ASSERT_NE(::fcntl(reader.native_handle(), F_SETFL, O_NONBLOCK), -1);
    std::array<std::byte, 1> buffer{};
    expect_system_error([&reader, &buffer] { static_cast<void>(reader.read_some(buffer)); },
                        std::errc::resource_unavailable_try_again);
    ASSERT_EQ(::close(sockets[1]), 0);
}

TEST(UnixSocketTest, SupportsShutdownAndDescriptorRelease)
{
    std::array<int, 2> sockets{};
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets.data()), 0);
    UnixSocket writer =
        UnixSocket::adopt_native_handle(OwnedFileDescriptor::adopt_native_handle(sockets[0]));
    UnixSocket reader =
        UnixSocket::adopt_native_handle(OwnedFileDescriptor::adopt_native_handle(sockets[1]));
    // Shutting down only the send direction should produce EOF on the peer.
    writer.shutdown(Shutdown::Send);
    std::array<std::byte, 1> buffer{};
    EXPECT_EQ(reader.read_some(buffer), 0);
    OwnedFileDescriptor released = reader.release_native_handle();
    // Releasing transfers the descriptor out without closing it.
    EXPECT_FALSE(reader.is_open());
    EXPECT_TRUE(released.is_open());
}

} // namespace
