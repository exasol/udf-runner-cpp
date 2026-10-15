#include <exasol/udf/v2/interface/interface.hpp>

#include <cassert>
#include <chrono>
#include <memory>
#include <utility>

#include <arrow/api.h>
#include <arrow/c/bridge.h>

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

    [[nodiscard]] Result<void> send(ControlMessageBuilder message) override // NOSONAR
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
        .add_payload("second_request", "value", true)
        .data_schema(ArrowSchemaPtr(&schema), true, true)
        .next(1024, true, 42)
        .record_batch(ArrowArrayPtr(&array), true)
        .error("E_TEST", "failure")
        .close_call();

    assert(call.message().open_call.has_value());
    assert(call.message().data_schema.has_value());
    assert(call.message().data_schema.value().schema.get() == &schema);
    assert(call.message().outbound_next.has_value());
    assert(call.message().outbound_next.value().byte_budget == 1024);
    assert(call.message().record_batch.has_value());
    assert(call.message().record_batch.value().array.get() == &array);
    auto taken_call = std::move(call).take();
    DataSchemaView data_schema_view(std::move(taken_call.data_schema.value()));
    assert(data_schema_view.has_group_id());
    assert(data_schema_view.has_row_id());
    assert(std::move(data_schema_view).take_schema().get() == &schema);
    RecordBatchView record_batch_view(std::move(taken_call.record_batch.value()));
    assert(record_batch_view.is_end_of_group());
    assert(std::move(record_batch_view).take_array().get() == &array);
    assert(taken_call.error.has_value());
    assert(taken_call.close_call);

    CallMessageView empty_call_view;
    assert(!empty_call_view.has_open_call());
    assert(!empty_call_view.open_call());
    assert(!empty_call_view.has_payloads());
    assert(!empty_call_view.payloads());
    assert(!empty_call_view.has_data_schema());
    assert(std::move(empty_call_view.data_schema()).take_schema() == nullptr);
    assert(!empty_call_view.data_schema().has_group_id());
    assert(!empty_call_view.data_schema().has_row_id());
    assert(!empty_call_view.has_next());
    assert(!empty_call_view.next());
    assert(!empty_call_view.has_record_batch());
    assert(std::move(empty_call_view.record_batch()).take_array() == nullptr);
    assert(!empty_call_view.record_batch().is_end_of_group());
    assert(!empty_call_view.has_error());
    assert(!empty_call_view.error());
    assert(!empty_call_view.has_close_call());

    ControlMessageBuilder control;
    control.server_capabilities(2, 1, std::endian::little, 4)
        .keep_alive()
        .add_payload("control", "value")
        .add_payload("second_control", "value", true)
        .error("E_CONTROL", "control failure")
        .close_connection();
    assert(control.message().server_capabilities.has_value());
    assert(control.message().error.has_value());
    assert(control.message().close_connection);

    const ControlMessageView empty_control_view;
    assert(!empty_control_view.has_server_capabilities());
    assert(!empty_control_view.server_capabilities());
    assert(!empty_control_view.has_payloads());
    assert(!empty_control_view.payloads());
    assert(!empty_control_view.has_error());
    assert(!empty_control_view.error());
    assert(!empty_control_view.has_keep_alive());
    assert(!empty_control_view.has_close_connection());

    const auto taken_control = std::move(control).take();
    assert(taken_control.keep_alive);
    assert(taken_call.close_call);
}

void arrow_owners_release_exactly_once()
{
    int schema_releases = 0;
    ArrowSchema schema{};
    schema.private_data = &schema_releases;
    schema.release      = count_schema_release;
    {
        ArrowSchemaPtr owner(&schema);
        ArrowSchemaPtr moved(std::move(owner));
        assert(moved.get() == &schema);
    }
    assert(schema_releases == 1);

    schema_releases     = 0;
    schema.release      = count_schema_release;
    schema.private_data = &schema_releases;
    {
        DataSchemaView view(DataSchema{ArrowSchemaPtr(&schema), true, true});
        assert(view.has_group_id());
        assert(view.has_row_id());
        ArrowSchemaPtr transferred = std::move(view).take_schema();
        assert(transferred.get() == &schema);
    }
    assert(schema_releases == 1);

    ArrowSchema first_schema{};
    ArrowSchema second_schema{};
    ArrowSchemaPtr assigned_schema(&first_schema);
    ArrowSchemaPtr replacement_schema(&second_schema);
    assigned_schema = std::move(replacement_schema);
    assert(assigned_schema.get() == &second_schema);

    int array_releases = 0;
    ArrowArray array{};
    array.private_data = &array_releases;
    array.release      = count_array_release;
    {
        ArrowArrayPtr owner(&array);
        ArrowArray* transferred = owner.release();
        assert(owner.get() == nullptr);
        assert(transferred == &array);
        transferred->release(transferred);
    }
    assert(array_releases == 1);

    array_releases     = 0;
    array.release      = count_array_release;
    array.private_data = &array_releases;
    {
        RecordBatchView view(RecordBatch{ArrowArrayPtr(&array), true});
        assert(view.is_end_of_group());
        ArrowArrayPtr transferred = std::move(view).take_array();
        assert(transferred.get() == &array);
    }
    assert(array_releases == 1);

    ArrowArray first_array{};
    ArrowArray second_array{};
    ArrowArrayPtr assigned_array(&first_array);
    ArrowArrayPtr replacement_array(&second_array);
    assigned_array = std::move(replacement_array);
    assert(assigned_array.get() == &second_array);

    ArrowArray released_array{};
    {
        ArrowArrayPtr owner(&released_array);
        assert(owner.get() == &released_array);
    }
    {
        ArrowSchemaPtr empty_schema;
        ArrowArrayPtr empty_array;
        assert(empty_schema.get() == nullptr);
        assert(empty_array.get() == nullptr);
    }
}

void arrow_c_data_round_trip_through_interface()
{
    arrow::Int64Builder id_builder;
    assert(id_builder.Append(7).ok());
    assert(id_builder.Append(11).ok());

    std::shared_ptr<arrow::Array> ids;
    assert(id_builder.Finish(&ids).ok());
    const auto schema = arrow::schema({arrow::field("id", arrow::int64())});
    const auto source = arrow::RecordBatch::Make(schema, 2, {ids});

    ArrowSchema received_schema{};
    ArrowArray received_array{};
    assert(arrow::ExportRecordBatch(*source, &received_array, &received_schema).ok());

    DataSchemaView schema_view(DataSchema{ArrowSchemaPtr(&received_schema), false, false});
    RecordBatchView batch_view(RecordBatch{ArrowArrayPtr(&received_array), true});
    auto schema_owner = std::move(schema_view).take_schema();
    auto array_owner  = std::move(batch_view).take_array();
    assert(schema_owner != nullptr);
    assert(array_owner != nullptr);

    auto imported = arrow::ImportRecordBatch(array_owner.get(), schema_owner.get());
    assert(imported.ok());
    assert(imported.ValueOrDie()->num_rows() == 2);
    const auto imported_ids =
        std::static_pointer_cast<arrow::Int64Array>(imported.ValueOrDie()->column(0));
    assert(imported_ids->Value(0) == 7);
    assert(imported_ids->Value(1) == 11);

    ArrowSchema sent_schema{};
    ArrowArray sent_array{};
    assert(arrow::ExportRecordBatch(*imported.ValueOrDie(), &sent_array, &sent_schema).ok());
    {
        CallMessageBuilder outgoing;
        outgoing.data_schema(ArrowSchemaPtr(&sent_schema), false, false)
            .record_batch(ArrowArrayPtr(&sent_array), true);
        const auto sent = std::move(outgoing).take();
        assert(sent.data_schema.has_value());
        assert(sent.data_schema->schema.get() == &sent_schema);
        assert(sent.record_batch.has_value());
        assert(sent.record_batch->array.get() == &sent_array);
        assert(sent.record_batch->is_end_of_group);
    }
    assert(sent_schema.release == nullptr);
    assert(sent_array.release == nullptr);
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

    const auto failed = Result<int>::failure(OperationStatus::ProtocolError, original);
    assert(!failed);
    assert(failed.error.has_value());
    assert(Result<void>::success());
    assert(!Result<void>::failure(OperationStatus::TimedOut));
}

} // namespace exasol::udf::v2

int main() noexcept // NOLINT(bugprone-exception-escape)
{
    using namespace exasol::udf::v2;
    builders_and_views_cover_present_and_absent_fields();
    arrow_owners_release_exactly_once();
    arrow_c_data_round_trip_through_interface();
    convenience_methods_delegate_to_virtual_operations();
    result_and_timeout_types_are_usable();
}
