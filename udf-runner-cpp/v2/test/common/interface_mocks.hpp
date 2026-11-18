#pragma once

#include <exasol/udf/v2/interface/interface.hpp>

#include <gmock/gmock.h>

namespace exasol::udf::v2::test {

class MockCall : public interface::Call {
public:
  MOCK_METHOD(interface::MessageStatus, receive_status, (), (const, override));
  MOCK_METHOD(std::unique_ptr<interface::CallMessageBuilder>,
              create_message_builder, (), (const, override));
  MOCK_METHOD((interface::Result<std::unique_ptr<interface::CallMessageView>>),
              receive, (), (override));
  MOCK_METHOD((interface::Result<std::unique_ptr<interface::CallMessageView>>),
              receive, (interface::Timeout timeout), (override));
  MOCK_METHOD((interface::Result<void>), send,
              (std::unique_ptr<interface::CallMessageBuilder> && message),
              (override));
  MOCK_METHOD((interface::Result<void>), send,
              (std::unique_ptr<interface::CallMessageBuilder> && message,
               interface::Timeout timeout),
              (override));
};

class MockControlStream : public interface::ControlStream {
public:
  MOCK_METHOD(interface::MessageStatus, receive_status, (), (const, override));
  MOCK_METHOD(std::unique_ptr<interface::ControlMessageBuilder>,
              create_message_builder, (), (const, override));
  MOCK_METHOD(
      (interface::Result<std::unique_ptr<interface::ControlMessageView>>),
      receive, (), (override));
  MOCK_METHOD(
      (interface::Result<std::unique_ptr<interface::ControlMessageView>>),
      receive, (interface::Timeout timeout), (override));
  MOCK_METHOD((interface::Result<void>), send,
              (std::unique_ptr<interface::ControlMessageBuilder> && message),
              (override));
};

class MockContext : public interface::Context {
public:
  MOCK_METHOD(interface::MessageStatus, receive_status, (), (const, override));
  MOCK_METHOD(bool, is_new_inbound_call_available, (), (const, override));
  MOCK_METHOD(interface::MessageStatus, wait_for_message, (), (override));
  MOCK_METHOD(interface::MessageStatus, wait_for_message,
              (interface::Timeout timeout), (override));
  MOCK_METHOD((interface::Result<std::unique_ptr<interface::Call>>),
              accept_call, (), (override));
  MOCK_METHOD(std::unique_ptr<interface::OpeningCallMessageBuilder>,
              create_opening_call_message_builder, (), (override));
  MOCK_METHOD((interface::Result<std::unique_ptr<interface::Call>>), open_call,
              (std::unique_ptr<interface::OpeningCallMessageBuilder> &&
               opening_message),
              (override));
  MOCK_METHOD(interface::ControlStream &, control_stream, (), (override));
};

} // namespace exasol::udf::v2::test
