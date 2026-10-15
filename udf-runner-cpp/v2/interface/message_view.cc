#include <exasol/udf/v2/interface/message_view/call_message_view.hpp>
#include <exasol/udf/v2/interface/message_view/control_message_view.hpp>

#include <functional>
#include <optional>
#include <utility>

namespace exasol::udf::v2
{

namespace
{

    const protocol::ControlMessage* control_message(
        const protocol::DecodedFrame* decoded_frame) noexcept
    {
        return decoded_frame != nullptr ? decoded_frame->frame().control_message() : nullptr;
    }

    template <typename T>
    std::optional<std::reference_wrapper<const T>> optional_reference(const T* value) noexcept
    {
        return value != nullptr ? std::optional{std::cref(*value)} : std::nullopt;
    }

    CallMessage decode_call_message(const protocol::DecodedFrame* decoded_frame)
    {
        CallMessage message;
        if (decoded_frame == nullptr)
        {
            return message;
        }

        const auto* frame = &decoded_frame->frame();
        if (const auto* control = frame->control_message(); control != nullptr)
        {
            if (const auto* next = control->next(); next != nullptr)
            {
                message.next = Next{next->reset(), next->row_id()};
            }
            if (const auto* error = control->error(); error != nullptr)
            {
                message.error.emplace(error->code() != nullptr ? error->code()->str() : "",
                                      error->message() != nullptr ? error->message()->str() : "");
            }
            if (const auto* close = control->value_as_CloseControlMessage(); close != nullptr)
            {
                message.close_call = close->close_call() != nullptr;
            }
        }
        return message;
    }

    ControlMessage decode_control_message(const protocol::DecodedFrame* decoded_frame)
    {
        ControlMessage message;
        if (decoded_frame == nullptr)
        {
            return message;
        }

        const auto* control = decoded_frame->frame().control_message();
        if (control == nullptr)
        {
            return message;
        }

        if (const auto* capabilities = control->value_as_ServerCapabilities();
            capabilities != nullptr)
        {
            const auto* version         = capabilities->supported_version();
            message.server_capabilities = ServerCapabilities{
                version != nullptr ? version->major() : 0,
                version != nullptr ? version->minor() : 0,
                capabilities->endianness() == protocol::Endianness_Big ? std::endian::big
                                                                       : std::endian::little,
                capabilities->number_of_supported_workers()};
        }
        message.keep_alive = control->value_as_KeepAlive() != nullptr;
        if (const auto* error = control->error(); error != nullptr)
        {
            message.error.emplace(error->code() != nullptr ? error->code()->str() : "",
                                  error->message() != nullptr ? error->message()->str() : "");
        }
        if (const auto* close = control->value_as_CloseControlMessage(); close != nullptr)
        {
            message.close_connection = close->close_connection() != nullptr;
        }
        return message;
    }

} // namespace

bool CallMessageView::has_open_call() const noexcept
{
    return open_call().has_value();
}
std::optional<std::reference_wrapper<const protocol::OpenCall>> CallMessageView::open_call()
    const noexcept
{
    const auto* control = control_message(decoded_frame_.get());
    return optional_reference(control != nullptr ? control->value_as_OpenCall() : nullptr);
}
bool CallMessageView::has_payloads() const noexcept
{
    return payloads().has_value();
}
std::optional<std::reference_wrapper<const protocol::Payloads>> CallMessageView::payloads()
    const noexcept
{
    const auto* control = control_message(decoded_frame_.get());
    return optional_reference(control != nullptr ? control->payloads() : nullptr);
}
bool CallMessageView::has_data_schema() const noexcept
{
    return call_message_.has_value() && call_message_->data_schema.has_value();
}
DataSchemaView CallMessageView::data_schema() noexcept
{
    if (!call_message_.has_value() || !call_message_->data_schema.has_value())
    {
        return DataSchemaView{};
    }
    DataSchemaView view(std::move(*call_message_->data_schema));
    call_message_->data_schema.reset();
    return view;
}
bool CallMessageView::has_next() const noexcept
{
    return call_message_.has_value() && call_message_->next.has_value();
}
std::optional<std::reference_wrapper<const Next>> CallMessageView::next() const noexcept
{
    if (!call_message_.has_value() || !call_message_->next.has_value())
    {
        return std::nullopt;
    }
    return std::cref(*call_message_->next);
}
bool CallMessageView::has_record_batch() const noexcept
{
    return call_message_.has_value() && call_message_->record_batch.has_value();
}
RecordBatchView CallMessageView::record_batch() noexcept
{
    if (!call_message_.has_value() || !call_message_->record_batch.has_value())
    {
        return RecordBatchView{};
    }
    RecordBatchView view(std::move(*call_message_->record_batch));
    call_message_->record_batch.reset();
    return view;
}
bool CallMessageView::has_error() const noexcept
{
    return error().has_value();
}
std::optional<std::reference_wrapper<const protocol::Error>> CallMessageView::error() const noexcept
{
    const auto* control = control_message(decoded_frame_.get());
    return optional_reference(control != nullptr ? control->error() : nullptr);
}
CallMessageView::CallMessageView(std::shared_ptr<const protocol::DecodedFrame> frame)
    : call_message_(decode_call_message(frame.get())), decoded_frame_(std::move(frame))
{
}

bool CallMessageView::has_close_call() const noexcept
{
    return call_message_.has_value() && call_message_->close_call;
}

bool ControlMessageView::has_server_capabilities() const noexcept
{
    return server_capabilities().has_value();
}
std::optional<std::reference_wrapper<const protocol::ServerCapabilities>>
ControlMessageView::server_capabilities() const noexcept
{
    const auto* control = control_message(decoded_frame_.get());
    return optional_reference(control != nullptr ? control->value_as_ServerCapabilities()
                                                 : nullptr);
}
bool ControlMessageView::has_keep_alive() const noexcept
{
    return control_message_.has_value() && control_message_->keep_alive;
}
bool ControlMessageView::has_payloads() const noexcept
{
    return payloads().has_value();
}
std::optional<std::reference_wrapper<const protocol::Payloads>> ControlMessageView::payloads()
    const noexcept
{
    const auto* control = control_message(decoded_frame_.get());
    return optional_reference(control != nullptr ? control->payloads() : nullptr);
}
bool ControlMessageView::has_error() const noexcept
{
    return error().has_value();
}
std::optional<std::reference_wrapper<const protocol::Error>> ControlMessageView::error()
    const noexcept
{
    const auto* control = control_message(decoded_frame_.get());
    return optional_reference(control != nullptr ? control->error() : nullptr);
}
ControlMessageView::ControlMessageView(std::shared_ptr<const protocol::DecodedFrame> frame)
    : control_message_(decode_control_message(frame.get())), decoded_frame_(std::move(frame))
{
}

bool ControlMessageView::has_close_connection() const noexcept
{
    return control_message_.has_value() && control_message_->close_connection;
}

} // namespace exasol::udf::v2
