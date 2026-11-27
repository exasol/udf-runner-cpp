#include "../common/interface_mocks.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <type_traits>
#include <utility>

namespace exasol::udf::v2::interface
{

static_assert(std::is_abstract_v<Context>);

TEST(ContextTest, ConvenienceMethodsDelegateToVirtualOperations)
{
    testing::StrictMock<test::MockContext> context;
    testing::StrictMock<test::MockControlStream> control_stream;
    auto message_to_open =
        std::make_unique<testing::NiceMock<test::MockOpeningCallMessageBuilder>>();
    const auto* expected_opening_message = message_to_open.get();
    EXPECT_CALL(context, receive_status()).WillOnce(testing::Return(MessageStatus::NoMessage));
    EXPECT_CALL(context, is_new_inbound_call_available()).WillOnce(testing::Return(false));
    EXPECT_CALL(context, wait_for_message()).WillOnce(testing::Return(MessageStatus::TimedOut));
    EXPECT_CALL(context, wait_for_message(testing::A<Timeout>()))
        .WillOnce(testing::Return(MessageStatus::TimedOut));
    EXPECT_CALL(context, accept_call()).WillOnce([] {
        return Result<std::unique_ptr<Call>>::failure(OperationStatus::Cancelled);
    });
    EXPECT_CALL(context, create_opening_call_message_builder()).WillOnce([] {
        return std::unique_ptr<OpeningCallMessageBuilder>{};
    });
    EXPECT_CALL(context, open_call(testing::_))
        .WillOnce([expected_opening_message](
                      std::unique_ptr<OpeningCallMessageBuilder> received_message) {
            EXPECT_EQ(received_message.get(), expected_opening_message);
            return Result<std::unique_ptr<Call>>::failure(OperationStatus::Cancelled);
        });
    EXPECT_CALL(context, control_stream()).WillOnce(testing::ReturnRef(control_stream));

    EXPECT_EQ(context.receive_status(), MessageStatus::NoMessage);
    EXPECT_FALSE(context.is_new_inbound_call_available());
    EXPECT_EQ(context.wait_for_message(), MessageStatus::TimedOut);
    EXPECT_EQ(context.wait_for_message(Timeout{}), MessageStatus::TimedOut);
    EXPECT_EQ(context.accept_call().status, OperationStatus::Cancelled);
    [[maybe_unused]] auto opening_message = context.create_opening_call_message_builder();
    EXPECT_EQ(context.open_call(std::move(message_to_open)).status, OperationStatus::Cancelled);
    auto& returned_control_stream = context.control_stream();
    EXPECT_EQ(&returned_control_stream, &control_stream);
}

} // namespace exasol::udf::v2::interface
