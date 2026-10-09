#include <unistd.h>

#include <array>
#include <cerrno>
#include <stdexcept>
#include <utility>

#include <exasol/udf/v2/socket/owned_file_descriptor.hpp>

#include <gtest/gtest.h>

namespace
{

using exasol::udf::v2::socket::OwnedFileDescriptor;

TEST(SocketTest, OwnedDescriptorClosesExactlyOnceAfterMoveAndRelease)
{
    std::array<int, 2> pipe_fds{};
    ASSERT_EQ(::pipe(pipe_fds.data()), 0);
    OwnedFileDescriptor descriptor = OwnedFileDescriptor::adopt_native_handle(pipe_fds[0]);
    OwnedFileDescriptor moved(std::move(descriptor));
    EXPECT_EQ(descriptor.release_native_handle(), -1); // NOLINT(bugprone-use-after-move,clang-analyzer-cplusplus.Move):
                                                       // Verifies the moved-from state.
    const int released_fd = moved.release_native_handle();
    EXPECT_FALSE(moved.is_open());
    ASSERT_EQ(::close(released_fd), 0);
    EXPECT_EQ(::close(released_fd), -1);
    EXPECT_EQ(errno, EBADF);
    ASSERT_EQ(::close(pipe_fds[1]), 0);
}

TEST(SocketTest, RejectsInvalidDescriptorAdoption)
{
    EXPECT_THROW(static_cast<void>(OwnedFileDescriptor::adopt_native_handle(-1)),
                 std::invalid_argument);
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
    EXPECT_EQ(source.release_native_handle(), -1); // NOLINT(bugprone-use-after-move,clang-analyzer-cplusplus.Move):
                                                   // Verifies the moved-from state.
    EXPECT_EQ(destination.native_handle(), first_pipe[0]);
    EXPECT_EQ(::close(second_pipe[0]), -1);
    EXPECT_EQ(errno, EBADF);
    ASSERT_EQ(::close(destination.release_native_handle()), 0);
    ASSERT_EQ(::close(first_pipe[1]), 0);
    ASSERT_EQ(::close(second_pipe[1]), 0);
}

} // namespace
