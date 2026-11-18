#include "../common/interface_mocks.hpp"

#include <gtest/gtest.h>

#include <type_traits>

namespace exasol::udf::v2::interface {

static_assert(std::is_abstract_v<ControlStream>);

TEST(ControlStreamTest, AbstractInterfaceOperationsCanBeImplemented) {
  testing::StrictMock<test::MockControlStream> control_stream;
  EXPECT_CALL(control_stream, receive_status())
      .WillOnce(testing::Return(MessageStatus::NoMessage));
  EXPECT_CALL(control_stream, create_message_builder()).WillOnce([] {
    return std::unique_ptr<ControlMessageBuilder>{};
  });
  EXPECT_CALL(control_stream, receive()).WillOnce([] {
    return Result<std::unique_ptr<ControlMessageView>>::success(nullptr);
  });
  EXPECT_CALL(control_stream, receive(testing::A<Timeout>())).WillOnce([] {
    return Result<std::unique_ptr<ControlMessageView>>::success(nullptr);
  });
  EXPECT_CALL(control_stream, send(testing::_)).WillOnce([] {
    return Result<void>::success();
  });

  EXPECT_EQ(control_stream.receive_status(), MessageStatus::NoMessage);
  [[maybe_unused]] auto builder = control_stream.create_message_builder();
  EXPECT_TRUE(control_stream.receive());
  EXPECT_TRUE(control_stream.receive(Timeout{}));
  EXPECT_TRUE(control_stream.send(std::unique_ptr<ControlMessageBuilder>{}));
}

} // namespace exasol::udf::v2::interface
