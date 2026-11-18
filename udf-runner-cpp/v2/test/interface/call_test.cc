#include "../common/interface_mocks.hpp"

#include <gtest/gtest.h>

#include <type_traits>

namespace exasol::udf::v2::interface
{

static_assert(std::is_abstract_v<Call>);

TEST(CallTest, AbstractInterfaceOperationsCanBeImplemented)
{
    testing::StrictMock<test::MockCall> call;
    EXPECT_CALL(call, receive_status()).WillOnce(testing::Return(MessageStatus::MessageAvailable));
    EXPECT_CALL(call, create_message_builder()).WillOnce([] {
        return std::unique_ptr<CallMessageBuilder>{};
    });
    EXPECT_CALL(call, receive()).WillOnce([] {
        return Result<std::unique_ptr<CallMessageView>>::success(nullptr);
    });
    EXPECT_CALL(call, receive(testing::A<Timeout>())).WillOnce([] {
        return Result<std::unique_ptr<CallMessageView>>::success(nullptr);
    });
    EXPECT_CALL(call, send(testing::_)).WillOnce([] { return Result<void>::success(); });
    EXPECT_CALL(call, send(testing::_, testing::A<Timeout>())).WillOnce([] {
        return Result<void>::success();
    });

    EXPECT_EQ(call.receive_status(), MessageStatus::MessageAvailable);
    [[maybe_unused]] auto builder = call.create_message_builder();
    EXPECT_TRUE(call.receive());
    EXPECT_TRUE(call.receive(Timeout{}));
    EXPECT_TRUE(call.send(std::unique_ptr<CallMessageBuilder>{}));
    EXPECT_TRUE(call.send(std::unique_ptr<CallMessageBuilder>{}, Timeout{}));
}

} // namespace exasol::udf::v2::interface
