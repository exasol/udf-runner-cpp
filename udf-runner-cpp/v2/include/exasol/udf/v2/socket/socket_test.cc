#include <exasol/udf/v2/socket/unix_socket.hpp>

#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>

#include <gtest/gtest.h>

namespace
{

using exasol::udf::v2::socket::OwnedFileDescriptor;
using exasol::udf::v2::socket::UnixSocket;
using exasol::udf::v2::socket::UnixSocketListener;

std::filesystem::path unique_socket_path()
{
    std::array<char, 64> directory_template{"/tmp/udf-runner-cpp-socket-XXXXXX"};
    char* directory = ::mkdtemp(directory_template.data());
    EXPECT_NE(directory, nullptr);
    return std::filesystem::path(directory) / "socket";
}

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

TEST(SocketTest, OwnedDescriptorClosesExactlyOnceAfterMoveAndRelease)
{
    std::array<int, 2> pipe_fds{};
    ASSERT_EQ(::pipe(pipe_fds.data()), 0);
    OwnedFileDescriptor descriptor = OwnedFileDescriptor::adopt_native_handle(pipe_fds[0]);
    OwnedFileDescriptor moved(std::move(descriptor));
    const int released_fd = moved.release_native_handle();
    EXPECT_FALSE(moved.is_open());
    ASSERT_EQ(::close(released_fd), 0);
    EXPECT_EQ(::close(released_fd), -1);
    EXPECT_EQ(errno, EBADF);
    ASSERT_EQ(::close(pipe_fds[1]), 0);
}

TEST(SocketTest, ListenerConnectsAcceptsAndRequiresExplicitCleanup)
{
    const std::filesystem::path path = unique_socket_path();
    UnixSocketListener listener      = UnixSocketListener::bind(path);
    ASSERT_TRUE(std::filesystem::exists(path));
    UnixSocket client = UnixSocket::connect(path);
    UnixSocket server = listener.accept();

    const std::array<std::byte, 2> sent{std::byte{'o'}, std::byte{'k'}};
    ASSERT_EQ(client.write_some(sent), sent.size());
    std::array<std::byte, 2> received{};
    ASSERT_EQ(server.read_some(received), received.size());
    EXPECT_EQ(received, sent);

    listener.close();
    EXPECT_TRUE(std::filesystem::exists(path));
    listener.unlink_path();
    EXPECT_FALSE(std::filesystem::exists(path));
    std::filesystem::remove(path.parent_path());
}

TEST(SocketTest, ListenerDoesNotOverwriteExistingPath)
{
    const std::filesystem::path path = unique_socket_path();
    UnixSocketListener listener      = UnixSocketListener::bind(path);
    expect_system_error([&] { static_cast<void>(UnixSocketListener::bind(path)); },
                        std::errc::address_in_use);
    listener.close();
    listener.unlink_path();
    std::filesystem::remove(path.parent_path());
}

TEST(SocketTest, SupportsScatterGatherIoAndPeerEof)
{
    std::array<int, 2> sockets{};
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets.data()), 0);
    UnixSocket writer =
        UnixSocket::adopt_native_handle(OwnedFileDescriptor::adopt_native_handle(sockets[0]));
    UnixSocket reader =
        UnixSocket::adopt_native_handle(OwnedFileDescriptor::adopt_native_handle(sockets[1]));
    const std::array<std::byte, 2> first{std::byte{'a'}, std::byte{'b'}};
    const std::array<std::byte, 2> second{std::byte{'c'}, std::byte{'d'}};
    const std::array<std::span<const std::byte>, 2> buffers{first, second};
    ASSERT_EQ(writer.write_some(buffers), 4);
    std::array<std::byte, 4> received{};
    ASSERT_EQ(reader.read_some(received), 4);
    EXPECT_EQ(received, (std::array<std::byte, 4>{std::byte{'a'}, std::byte{'b'}, std::byte{'c'},
                                                  std::byte{'d'}}));
    writer.close();
    std::array<std::byte, 1> eof_buffer{};
    EXPECT_EQ(reader.read_some(eof_buffer), 0);
}

TEST(SocketTest, NonblockingReadReportsWouldBlock)
{
    std::array<int, 2> sockets{};
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets.data()), 0);
    UnixSocket reader =
        UnixSocket::adopt_native_handle(OwnedFileDescriptor::adopt_native_handle(sockets[0]));
    ASSERT_NE(::fcntl(reader.native_handle(), F_SETFL, O_NONBLOCK), -1);
    std::array<std::byte, 1> buffer{};
    expect_system_error([&] { static_cast<void>(reader.read_some(buffer)); },
                        std::errc::resource_unavailable_try_again);
    ASSERT_EQ(::close(sockets[1]), 0);
}

} // namespace
