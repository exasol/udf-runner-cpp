#include <exasol/udf/v2/context/context_factory.hpp>
#include <exasol/udf/v2/context/context_manager.hpp>

#include <gtest/gtest.h>

#include <sys/socket.h>

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <unistd.h>

namespace exasol::udf::v2
{
namespace
{

    struct State
    {
        bool started{false};
        bool stop_requested{false};
        bool joined{false};
        bool received_connection{false};
    };

    class ControlStreamStub final : public interface::ControlStream
    {
    public:
        [[nodiscard]] interface::MessageStatus receive_status() const override
        {
            return interface::MessageStatus::NoMessage;
        }

        [[nodiscard]] interface::Result<interface::ControlMessageView> receive(
            interface::Timeout) override
        {
            return interface::Result<interface::ControlMessageView>::failure(
                interface::OperationStatus::TimedOut);
        }

        [[nodiscard]] interface::Result<void> send(interface::ControlMessageBuilder) override
        {
            return interface::Result<void>::success();
        }
    };

    class BackgroundThreadStub final : public context::BackgroundIoThreadInterface
    {
    public:
        BackgroundThreadStub(std::shared_ptr<State> state, socket::OwnedFileDescriptor connection)
            : state_(std::move(state)), connection_(std::move(connection))
        {
            state_->received_connection = connection_.is_open();
        }

        ~BackgroundThreadStub() override = default;

        void start() override
        {
            state_->started = true;
        }

        void request_stop() noexcept override
        {
            state_->stop_requested = true;
        }

        void join() noexcept override
        {
            state_->joined = true;
        }

        [[nodiscard]] interface::MessageStatus receive_status() const override
        {
            return interface::MessageStatus::MessageAvailable;
        }

        [[nodiscard]] bool is_new_inbound_call_available() const override
        {
            return true;
        }

        [[nodiscard]] interface::MessageStatus wait_for_message(interface::Timeout) override
        {
            return interface::MessageStatus::TimedOut;
        }

        [[nodiscard]] interface::Result<std::unique_ptr<interface::Call>> accept_call() override
        {
            return interface::Result<std::unique_ptr<interface::Call>>::failure(
                interface::OperationStatus::Cancelled);
        }

        [[nodiscard]] interface::Result<std::unique_ptr<interface::Call>> open_call(
            interface::CallMessageBuilder) override
        {
            return interface::Result<std::unique_ptr<interface::Call>>::failure(
                interface::OperationStatus::ProtocolError);
        }

        [[nodiscard]] interface::ControlStream& control_stream() override
        {
            return control_stream_;
        }

        [[nodiscard]] interface::Result<void> send_call(std::uint64_t,
                                                        interface::CallMessageBuilder,
                                                        interface::Timeout) override
        {
            return interface::Result<void>::success();
        }

        [[nodiscard]] interface::Result<void> send_control(
            interface::ControlMessageBuilder) override
        {
            return interface::Result<void>::success();
        }

    private:
        std::shared_ptr<State> state_;
        socket::OwnedFileDescriptor connection_;
        ControlStreamStub control_stream_;
    };

    class BackgroundThreadFactoryStub final : public context::BackgroundIoThreadFactory
    {
    public:
        explicit BackgroundThreadFactoryStub(std::shared_ptr<State> state)
            : state_(std::move(state))
        {
        }

        [[nodiscard]] std::unique_ptr<context::BackgroundIoThreadInterface> create(
            socket::OwnedFileDescriptor connection) override
        {
            return std::make_unique<BackgroundThreadStub>(state_, std::move(connection));
        }

    private:
        std::shared_ptr<State> state_;
    };

    socket::OwnedFileDescriptor make_test_connection()
    {
        return socket::OwnedFileDescriptor::adopt_native_handle(::dup(STDIN_FILENO));
    }

    std::pair<socket::OwnedFileDescriptor, socket::OwnedFileDescriptor> make_socket_pair()
    {
        int descriptors[2]{};
        if (::socketpair(AF_UNIX, SOCK_STREAM, 0, descriptors) == -1)
        {
            throw std::system_error(errno, std::generic_category(), "create test socket pair");
        }
        return {socket::OwnedFileDescriptor::adopt_native_handle(descriptors[0]),
                socket::OwnedFileDescriptor::adopt_native_handle(descriptors[1])};
    }

} // namespace

TEST(ContextFactoryTest, InjectsFactoryAndOwnsBackgroundThreadLifetime)
{
    const auto state = std::make_shared<State>();
    auto context     = create_context(make_test_connection(),
                                      std::make_unique<BackgroundThreadFactoryStub>(state));

    EXPECT_TRUE(state->received_connection);
    EXPECT_TRUE(state->started);
    EXPECT_EQ(context->receive_status(), interface::MessageStatus::MessageAvailable);
    EXPECT_TRUE(context->is_new_inbound_call_available());
    EXPECT_EQ(context->wait_for_message(), interface::MessageStatus::TimedOut);
    EXPECT_EQ(context->accept_call().status, interface::OperationStatus::Cancelled);
    EXPECT_EQ(context->open_call(interface::CallMessageBuilder{}).status,
              interface::OperationStatus::ProtocolError);
    EXPECT_NE(&context->control_stream(), nullptr);

    context.reset();

    EXPECT_TRUE(state->stop_requested);
    EXPECT_TRUE(state->joined);
}

TEST(ContextFactoryTest, RejectsNullFactory)
{
    EXPECT_THROW(static_cast<void>(create_context(make_test_connection(), nullptr)),
                 std::invalid_argument);
}

TEST(ContextManagerTest, CreatesLowLevelContextFromOwnedConnection)
{
    const auto state = std::make_shared<State>();
    context::ContextManager manager(std::make_unique<BackgroundThreadFactoryStub>(state));
    auto context = manager.create_context(make_test_connection());

    EXPECT_TRUE(state->received_connection);
    EXPECT_TRUE(state->started);
    context.reset();
    EXPECT_TRUE(state->stop_requested);
    EXPECT_TRUE(state->joined);
}

TEST(ContextFactoryTest, BackgroundThreadReportsPeerClose)
{
    auto [connection, peer] = make_socket_pair();
    auto context            = create_context(std::move(connection));
    peer.close();

    EXPECT_EQ(context->wait_for_message(std::chrono::seconds(1)),
              interface::MessageStatus::PeerClosed);
}

TEST(ContextFactoryTest, BackgroundThreadRejectsOversizedFrame)
{
    auto [connection, peer]                   = make_socket_pair();
    auto context                              = create_context(std::move(connection));
    constexpr std::uint32_t oversized_payload = 64U * 1024U * 1024U + 1U;
    ASSERT_EQ(::send(peer.native_handle(), &oversized_payload, sizeof(oversized_payload), 0),
              sizeof(oversized_payload));

    EXPECT_EQ(context->wait_for_message(std::chrono::seconds(1)),
              interface::MessageStatus::ConnectionError);
}

} // namespace exasol::udf::v2
