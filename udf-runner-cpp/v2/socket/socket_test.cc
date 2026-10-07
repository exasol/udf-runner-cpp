#include <exasol/udf/v2/socket/unix_socket.hpp>

#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace
{

using exasol::udf::v2::socket::OwnedFileDescriptor;
using exasol::udf::v2::socket::Shutdown;
using exasol::udf::v2::socket::UnixSocket;
using exasol::udf::v2::socket::UnixSocketListener;

std::filesystem::path unique_socket_path()
{
    constexpr std::size_t maximum_temporary_root_length = 64;
    const char* temporary_root                          = P_tmpdir;
    if (const char* configured_temporary_root = std::getenv("TMPDIR");
        configured_temporary_root != nullptr &&
        std::strlen(configured_temporary_root) <= maximum_temporary_root_length)
    {
        temporary_root = configured_temporary_root;
    }
    const std::string template_path =
        (std::filesystem::path(temporary_root) / "udf-runner-cpp-socket-XXXXXX").string();
    std::vector<char> directory_template(template_path.begin(), template_path.end());
    directory_template.push_back('\0');
    char* directory = ::mkdtemp(directory_template.data());
    if (directory == nullptr)
    {
        ADD_FAILURE() << "mkdtemp failed";
        return {};
    }
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

TEST(SocketTest, RejectsInvalidDescriptorAdoption)
{
    expect_system_error([] { static_cast<void>(OwnedFileDescriptor::adopt_native_handle(-1)); },
                        std::errc::bad_file_descriptor);
}

TEST(SocketTest, MoveAssignmentClosesReplacedDescriptor)
{
    std::array<int, 2> first_pipe{};
    std::array<int, 2> second_pipe{};
    ASSERT_EQ(::pipe(first_pipe.data()), 0);
    ASSERT_EQ(::pipe(second_pipe.data()), 0);
    OwnedFileDescriptor source      = OwnedFileDescriptor::adopt_native_handle(first_pipe[0]);
    OwnedFileDescriptor destination = OwnedFileDescriptor::adopt_native_handle(second_pipe[0]);
    destination                     = std::move(source);
    EXPECT_EQ(::close(second_pipe[0]), -1);
    EXPECT_EQ(errno, EBADF);
    ASSERT_EQ(::close(destination.release_native_handle()), 0);
    ASSERT_EQ(::close(first_pipe[1]), 0);
    ASSERT_EQ(::close(second_pipe[1]), 0);
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
    expect_system_error([path] { static_cast<void>(UnixSocketListener::bind(path)); },
                        std::errc::address_in_use);
    listener.close();
    listener.unlink_path();
    std::filesystem::remove(path.parent_path());
}

TEST(SocketTest, ValidatesPathsAndRequiresClosureBeforeUnlinking)
{
    expect_system_error([] { static_cast<void>(UnixSocketListener::bind({})); },
                        std::errc::filename_too_long);
    const std::filesystem::path path = unique_socket_path();
    UnixSocketListener listener      = UnixSocketListener::bind(path);
    EXPECT_THROW(listener.unlink_path(), std::logic_error);
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
    expect_system_error([&reader, &buffer] { static_cast<void>(reader.read_some(buffer)); },
                        std::errc::resource_unavailable_try_again);
    ASSERT_EQ(::close(sockets[1]), 0);
}

TEST(SocketTest, SupportsShutdownAndDescriptorRelease)
{
    std::array<int, 2> sockets{};
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets.data()), 0);
    UnixSocket writer =
        UnixSocket::adopt_native_handle(OwnedFileDescriptor::adopt_native_handle(sockets[0]));
    UnixSocket reader =
        UnixSocket::adopt_native_handle(OwnedFileDescriptor::adopt_native_handle(sockets[1]));
    writer.shutdown(Shutdown::Send);
    std::array<std::byte, 1> buffer{};
    EXPECT_EQ(reader.read_some(buffer), 0);
    OwnedFileDescriptor released = reader.release_native_handle();
    EXPECT_FALSE(reader.is_open());
    EXPECT_TRUE(released.is_open());
}

} // namespace
