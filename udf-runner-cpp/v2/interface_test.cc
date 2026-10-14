#include <exasol/udf/v2/interface.hpp>

#include <cassert>
#include <chrono>
#include <memory>
#include <utility>

namespace exasol::udf::v2
{

void count_schema_release(ArrowSchema* schema)
{
    auto* count = static_cast<int*>(schema->private_data);
    ++*count;
    schema->release = nullptr;
}

void count_array_release(ArrowArray* array)
{
    auto* count = static_cast<int*>(array->private_data);
    ++*count;
    array->release = nullptr;
}

class TestCall final : public Call
{
public:
    using Call::receive;
    using Call::send;

    [[nodiscard]] MessageStatus receive_status() const override
    {
        return MessageStatus::MessageAvailable;
    }

    [[nodiscard]] Result<CallMessageView> receive(Timeout timeout) override
    {
        (void)timeout;
        return Result<CallMessageView>::success(CallMessageView{});
    }

    [[nodiscard]] Result<void> send(CallMessageBuilder message, Timeout timeout) override
    {
        (void)message;
        (void)timeout;
        return Result<void>::success();
    }

private:
    CallMessage message_;
};

class TestControlStream final : public ControlStream
{
public:
    using ControlStream::receive;

    [[nodiscard]] MessageStatus receive_status() const override
    {
        return MessageStatus::NoMessage;
    }

    [[nodiscard]] Result<ControlMessageView> receive(Timeout timeout) override
    {
        (void)timeout;
        return Result<ControlMessageView>::success(ControlMessageView{});
    }

    [[nodiscard]] Result<void> send(ControlMessageBuilder message) override
    {
        (void)message;
        return Result<void>::success();
    }

private:
    ControlMessage message_;
};

class TestContext final : public Context
{
public:
    using Context::wait_for_message;

    [[nodiscard]] MessageStatus receive_status() const override
    {
        return MessageStatus::NoMessage;
    }

    [[nodiscard]] bool is_new_inbound_call_available() const override
    {
        return false;
    }

    [[nodiscard]] MessageStatus wait_for_message(Timeout timeout) override
    {
        (void)timeout;
        return MessageStatus::TimedOut;
    }

    [[nodiscard]] Result<std::unique_ptr<Call>> accept_call() override
    {
        return Result<std::unique_ptr<Call>>::failure(OperationStatus::Cancelled);
    }

    [[nodiscard]] Result<std::unique_ptr<Call>> open_call(
        CallMessageBuilder opening_message) override
    {
        (void)opening_message;
        return Result<std::unique_ptr<Call>>::failure(OperationStatus::Cancelled);
    }

    [[nodiscard]] ControlStream& control_stream() override
    {
        return control_stream_;
    }

private:
    TestControlStream control_stream_;
};

void builders_and_views_cover_present_and_absent_fields()
{
    ArrowSchema schema{};
    ArrowArray array{};
    CallMessageBuilder call;
    call.open_call("nested")
        .add_payload("request", "{}")
        .data_schema(ArrowSchemaOwner(&schema), true, true)
        .next(1024, true, 42)
        .record_batch(ArrowArrayOwner(&array), true)
        .error("E_TEST", "failure")
        .close_call();

    assert(call.message().open_call.has_value());
    assert(call.message().payloads.has_value());
    assert(call.message().data_schema.has_value());
    assert(call.message().data_schema.value().schema.get() == &schema);
    assert(call.message().outbound_next.has_value());
    assert(call.message().outbound_next.value().byte_budget == 1024);
    assert(call.message().record_batch.has_value());
    assert(call.message().record_batch.value().array.get() == &array);
    const DataSchemaView data_schema_view(call.message().data_schema.value());
    assert(data_schema_view.schema() == &schema);
    assert(data_schema_view.has_group_id());
    assert(data_schema_view.has_row_id());
    const RecordBatchView record_batch_view(call.message().record_batch.value());
    assert(record_batch_view.array() == &array);
    assert(record_batch_view.is_end_of_group());
    assert(call.message().error.has_value());
    assert(call.message().close_call);

    const CallMessageView empty_call_view;
    assert(!empty_call_view.has_open_call());
    assert(!empty_call_view.open_call());
    assert(!empty_call_view.has_payloads());
    assert(!empty_call_view.payloads());
    assert(!empty_call_view.has_data_schema());
    assert(empty_call_view.data_schema().schema() == nullptr);
    assert(!empty_call_view.data_schema().has_group_id());
    assert(!empty_call_view.data_schema().has_row_id());
    assert(!empty_call_view.has_next());
    assert(!empty_call_view.next());
    assert(!empty_call_view.has_record_batch());
    assert(empty_call_view.record_batch().array() == nullptr);
    assert(!empty_call_view.record_batch().is_end_of_group());
    assert(!empty_call_view.has_error());
    assert(!empty_call_view.error());
    assert(!empty_call_view.close_call());

    ControlMessageBuilder control;
    control.server_capabilities(2, 1, std::endian::little, 4)
        .keep_alive()
        .add_payload("control", "value")
        .error("E_CONTROL", "control failure")
        .close_connection();
    assert(control.message().server_capabilities.has_value());
    assert(control.message().payloads.has_value());
    assert(control.message().error.has_value());
    assert(control.message().close_connection);

    const ControlMessageView empty_control_view;
    assert(!empty_control_view.has_server_capabilities());
    assert(!empty_control_view.server_capabilities());
    assert(!empty_control_view.has_payloads());
    assert(!empty_control_view.payloads());
    assert(!empty_control_view.has_error());
    assert(!empty_control_view.error());
    assert(!empty_control_view.close_connection());

    const auto taken_control = std::move(control).take();
    assert(taken_control.keep_alive);
    const auto taken_call = std::move(call).take();
    assert(taken_call.close_call);
}

void arrow_owners_release_exactly_once()
{
    int schema_releases = 0;
    ArrowSchema schema{};
    schema.private_data = &schema_releases;
    schema.release      = count_schema_release;
    {
        ArrowSchemaOwner owner(&schema);
        ArrowSchemaOwner moved(std::move(owner));
        assert(moved.get() == &schema);
    }
    assert(schema_releases == 1);

    ArrowSchema first_schema{};
    ArrowSchema second_schema{};
    ArrowSchemaOwner assigned_schema(&first_schema);
    ArrowSchemaOwner replacement_schema(&second_schema);
    assigned_schema = std::move(replacement_schema);
    assert(assigned_schema.get() == &second_schema);

    int array_releases = 0;
    ArrowArray array{};
    array.private_data = &array_releases;
    array.release      = count_array_release;
    {
        ArrowArrayOwner owner(&array);
        ArrowArray* transferred = owner.release();
        assert(owner.get() == nullptr);
        assert(transferred == &array);
        transferred->release(transferred);
    }
    assert(array_releases == 1);

    ArrowArray first_array{};
    ArrowArray second_array{};
    ArrowArrayOwner assigned_array(&first_array);
    ArrowArrayOwner replacement_array(&second_array);
    assigned_array = std::move(replacement_array);
    assert(assigned_array.get() == &second_array);

    ArrowArray released_array{};
    {
        ArrowArrayOwner owner(&released_array);
        assert(owner.get() == &released_array);
    }
}

void convenience_methods_delegate_to_virtual_operations()
{
    TestCall call;
    assert(call.receive_status() == MessageStatus::MessageAvailable);
    assert(call.receive());
    assert(call.send(CallMessageBuilder{}));

    TestControlStream control_stream;
    assert(control_stream.receive_status() == MessageStatus::NoMessage);
    assert(control_stream.receive());
    assert(control_stream.send(ControlMessageBuilder{}));

    TestContext context;
    assert(context.receive_status() == MessageStatus::NoMessage);
    assert(!context.is_new_inbound_call_available());
    assert(context.wait_for_message() == MessageStatus::TimedOut);
    assert(context.accept_call().status == OperationStatus::Cancelled);
    assert(context.open_call(CallMessageBuilder{}).status == OperationStatus::Cancelled);
    (void)context.control_stream();
}

void result_and_timeout_types_are_usable()
{
    const auto result = Result<int>::success(23);
    assert(result && result.value == 23);
    assert(Result<void>::failure(OperationStatus::TimedOut).status == OperationStatus::TimedOut);
    static_assert(Timeout{std::chrono::milliseconds(1)}.has_value());

    const ErrorInfo original("code", "message");
    assert(ErrorInfo(original).code() == "code");
    assert(ErrorInfo(original).message() == "message");
}

} // namespace exasol::udf::v2

int main() noexcept // NOLINT(bugprone-exception-escape)
{
    using namespace exasol::udf::v2;
    builders_and_views_cover_present_and_absent_fields();
    arrow_owners_release_exactly_once();
    convenience_methods_delegate_to_virtual_operations();
    result_and_timeout_types_are_usable();
}
