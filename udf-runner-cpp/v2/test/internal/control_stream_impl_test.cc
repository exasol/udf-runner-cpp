#include <exasol/udf/v2/internal/control_stream_impl.hpp>

#include <chrono>

#include <gtest/gtest.h>

namespace exasol::udf::v2::internal
{
namespace
{

    TEST(ControlStreamImplTest, ReportsContextClosedAfterStateExpires)
    {
        ControlStreamImpl stream{std::weak_ptr<ContextStateInterface>{}};

        EXPECT_EQ(stream.receive_status(), interface::MessageStatus::ContextClosed);
        EXPECT_EQ(stream.create_message_builder(), nullptr);
        EXPECT_EQ(stream.receive().status, interface::OperationStatus::ContextClosed);
        EXPECT_EQ(stream.receive(interface::Timeout{std::chrono::seconds{1}}).status,
                  interface::OperationStatus::ContextClosed);
        EXPECT_EQ(stream.send(nullptr).status, interface::OperationStatus::ContextClosed);
    }

} // namespace
} // namespace exasol::udf::v2::internal
