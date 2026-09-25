#include <unistd.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <system_error>
#include <utility>

#include <exasol/udf/v2/event_fd.hpp>

namespace
{

void test_check(bool condition, const char* message)
{
    if (!condition)
    {
        std::fprintf(stderr, "event fd test failure: %s\n", message);
        std::abort();
    }
}

template <typename Function>
void expect_system_error(Function&& function, std::errc expected, const char* message)
{
    try
    {
        function();
        test_check(false, message);
    }
    catch (const std::system_error& error)
    {
        test_check(error.code() == std::make_error_code(expected), "unexpected system error");
    }
}

template <typename Type>
void self_move_assign(Type& value)
{
    using move_assignment        = Type& (Type::*)(Type&&) noexcept;
    const move_assignment assign = &Type::operator=;
    (value.*assign)(std::move(value));
}

} // namespace

int main()
{
    try
    {
        exasol::udf::v2::LinuxEventFd event_fd;
        test_check(event_fd.native_handle() != -1, "eventfd construction failed");

        event_fd.write_notification();
        event_fd.write_notification();
        test_check(event_fd.read_notification() == 2, "eventfd did not accumulate notifications");
        expect_system_error([&event_fd] { event_fd.read_notification(); },
                            std::errc::resource_unavailable_try_again,
                            "empty eventfd read should report EAGAIN");

        const int moved_handle = event_fd.native_handle();
        exasol::udf::v2::LinuxEventFd move_constructed(std::move(event_fd));
        test_check(move_constructed.native_handle() == moved_handle,
                   "move construction changed the handle");

        exasol::udf::v2::LinuxEventFd move_assigned;
        move_assigned = std::move(move_constructed);
        test_check(move_assigned.native_handle() == moved_handle,
                   "move assignment changed the handle");
        self_move_assign(move_assigned);
        test_check(move_assigned.native_handle() == moved_handle,
                   "self move assignment changed the handle");

        {
            exasol::udf::v2::LinuxEventFd closed_event_fd;
            ::close(closed_event_fd.native_handle());
            expect_system_error([&closed_event_fd] { closed_event_fd.read_notification(); },
                                std::errc::bad_file_descriptor,
                                "closed eventfd read should be rejected");
        }

        {
            exasol::udf::v2::LinuxEventFd closed_event_fd;
            ::close(closed_event_fd.native_handle());
            expect_system_error([&closed_event_fd] { closed_event_fd.write_notification(); },
                                std::errc::bad_file_descriptor,
                                "closed eventfd write should be rejected");
        }
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "event fd test failure: %s\n", error.what());
        return 1;
    }
}
