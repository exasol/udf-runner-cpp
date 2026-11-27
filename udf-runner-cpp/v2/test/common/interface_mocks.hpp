#pragma once

#include <exasol/udf/v2/interface/interface.hpp>

#include <bit>
#include <cstdint>
#include <string_view>

#include <gmock/gmock.h>

namespace exasol::udf::v2::test
{

class MockOpeningCallMessageBuilder : public interface::OpeningCallMessageBuilder
{
public:
    MOCK_METHOD(interface::OpeningCallMessageBuilder&,
                open_call,
                (std::string_view name),
                (override));
    MOCK_METHOD(interface::CallMessageBuilderBase&,
                add_payload,
                (std::string_view name, std::string_view value, bool binary),
                (override));
    MOCK_METHOD(interface::CallMessageBuilderBase&,
                data_schema,
                (interface::ArrowSchemaPtr schema, bool has_group_id, bool has_row_id),
                (override));
    MOCK_METHOD(interface::CallMessageBuilderBase&, reset, (std::uint64_t row_id), (override));
    MOCK_METHOD(interface::CallMessageBuilderBase&,
                record_batch,
                (interface::ArrowArrayPtr array, bool is_end_of_group),
                (override));
    MOCK_METHOD(interface::CallMessageBuilderBase&,
                error,
                (std::string_view code, std::string_view text),
                (override));
    MOCK_METHOD(interface::CallMessageBuilderBase&, close_call, (), (override));
};

class MockCallMessageBuilder : public interface::CallMessageBuilder
{
public:
    MOCK_METHOD(interface::CallMessageBuilderBase&,
                add_payload,
                (std::string_view name, std::string_view value, bool binary),
                (override));
    MOCK_METHOD(interface::CallMessageBuilderBase&,
                data_schema,
                (interface::ArrowSchemaPtr schema, bool has_group_id, bool has_row_id),
                (override));
    MOCK_METHOD(interface::CallMessageBuilderBase&, reset, (std::uint64_t row_id), (override));
    MOCK_METHOD(interface::CallMessageBuilderBase&,
                record_batch,
                (interface::ArrowArrayPtr array, bool is_end_of_group),
                (override));
    MOCK_METHOD(interface::CallMessageBuilderBase&,
                error,
                (std::string_view code, std::string_view text),
                (override));
    MOCK_METHOD(interface::CallMessageBuilderBase&, close_call, (), (override));
};

class MockControlMessageBuilder : public interface::ControlMessageBuilder
{
public:
    MOCK_METHOD(interface::ControlMessageBuilder&,
                server_capabilities,
                (std::uint32_t major,
                 std::uint32_t minor,
                 std::endian endianness,
                 std::uint32_t number_of_supported_workers),
                (override));
    MOCK_METHOD(interface::ControlMessageBuilder&, keep_alive, (), (override));
    MOCK_METHOD(interface::ControlMessageBuilder&,
                add_payload,
                (std::string_view name, std::string_view value, bool binary),
                (override));
    MOCK_METHOD(interface::ControlMessageBuilder&,
                error,
                (std::string_view code, std::string_view text),
                (override));
    MOCK_METHOD(interface::ControlMessageBuilder&, close_connection, (), (override));
};

class MockCall : public interface::Call
{
public:
    MOCK_METHOD(interface::MessageStatus, receive_status, (), (const, override));
    MOCK_METHOD(std::unique_ptr<interface::CallMessageBuilder>,
                create_message_builder,
                (),
                (const, override));
    MOCK_METHOD((interface::Result<std::unique_ptr<interface::CallMessageView>>),
                receive,
                (),
                (override));
    MOCK_METHOD((interface::Result<std::unique_ptr<interface::CallMessageView>>),
                receive,
                (interface::Timeout timeout),
                (override));
    MOCK_METHOD((interface::Result<void>),
                send,
                (std::unique_ptr<interface::CallMessageBuilder> && message),
                (override));
    MOCK_METHOD((interface::Result<void>),
                send,
                (std::unique_ptr<interface::CallMessageBuilder> && message,
                 interface::Timeout timeout),
                (override));
};

class MockControlStream : public interface::ControlStream
{
public:
    MOCK_METHOD(interface::MessageStatus, receive_status, (), (const, override));
    MOCK_METHOD(std::unique_ptr<interface::ControlMessageBuilder>,
                create_message_builder,
                (),
                (const, override));
    MOCK_METHOD((interface::Result<std::unique_ptr<interface::ControlMessageView>>),
                receive,
                (),
                (override));
    MOCK_METHOD((interface::Result<std::unique_ptr<interface::ControlMessageView>>),
                receive,
                (interface::Timeout timeout),
                (override));
    MOCK_METHOD((interface::Result<void>),
                send,
                (std::unique_ptr<interface::ControlMessageBuilder> && message),
                (override));
};

class MockContext : public interface::Context
{
public:
    MOCK_METHOD(interface::MessageStatus, receive_status, (), (const, override));
    MOCK_METHOD(bool, is_new_inbound_call_available, (), (const, override));
    MOCK_METHOD(interface::MessageStatus, wait_for_message, (), (override));
    MOCK_METHOD(interface::MessageStatus,
                wait_for_message,
                (interface::Timeout timeout),
                (override));
    MOCK_METHOD((interface::Result<std::unique_ptr<interface::Call>>), accept_call, (), (override));
    MOCK_METHOD(std::unique_ptr<interface::OpeningCallMessageBuilder>,
                create_opening_call_message_builder,
                (),
                (override));
    MOCK_METHOD((interface::Result<std::unique_ptr<interface::Call>>),
                open_call,
                (std::unique_ptr<interface::OpeningCallMessageBuilder> && opening_message),
                (override));
    MOCK_METHOD(interface::ControlStream&, control_stream, (), (override));
};

} // namespace exasol::udf::v2::test
