#include <exasol/udf/v2/internal/control_stream_impl.hpp>

#include "../common/interface_mocks.hpp"

#include <chrono>
#include <memory>
#include <utility>

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
        std::unique_ptr<interface::ControlMessageBuilder> message =
            std::make_unique<testing::NiceMock<test::MockControlMessageBuilder>>();
        EXPECT_EQ(stream.send(std::move(message)).status,
                  interface::OperationStatus::ContextClosed);
        EXPECT_NE(message, nullptr);
    }

} // namespace
} // namespace exasol::udf::v2::internal
