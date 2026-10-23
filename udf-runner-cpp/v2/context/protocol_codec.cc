#include <exasol/udf/v2/context/protocol_codec.hpp>

#include <exasol/udf/v2/udf_protocol.hpp>

#include <algorithm>
#include <bit>
#include <limits>
#include <string>
#include <utility>

namespace exasol::udf::v2::context::protocol
{

using interface::CallMessage;
using interface::ControlMessage;
using interface::ErrorInfo;
using interface::Next;
using interface::OpenCall;
using interface::OperationStatus;
using interface::Payload;
using interface::Payloads;
using interface::Result;
using interface::ServerCapabilities;

namespace wire = exasol::udf::protocol;
using Builder  = exasol::udf::v2::third_party::flatbuffers::FlatBufferBuilder;
using Verifier = exasol::udf::v2::third_party::flatbuffers::Verifier;

namespace
{

    [[nodiscard]] Result<EncodedFrame> failure(std::string code, std::string message)
    {
        return Result<EncodedFrame>::failure(OperationStatus::ProtocolError,
                                             ErrorInfo{std::move(code), std::move(message)});
    }

    [[nodiscard]] Result<DecodedFrame> decode_failure(std::string message)
    {
        return Result<DecodedFrame>::failure(OperationStatus::ProtocolError,
                                             ErrorInfo{"invalid_frame", std::move(message)});
    }

    [[nodiscard]] bool has_unsupported_arrow(const CallMessage& message)
    {
        return message.data_schema.has_value() || message.record_batch.has_value();
    }

    [[nodiscard]] exasol::udf::v2::third_party::flatbuffers::Offset<wire::Payloads> make_payloads(
        Builder& builder, const Payloads& payloads)
    {
        std::vector<exasol::udf::v2::third_party::flatbuffers::Offset<wire::Payload>> values;
        values.reserve(payloads.size());
        for (const auto& payload : payloads)
        {
            const auto name = builder.CreateString(payload.name);
            exasol::udf::v2::third_party::flatbuffers::Offset<void> value;
            wire::PayloadValue type;
            if (payload.binary)
            {
                const auto bytes = builder.CreateVector(
                    reinterpret_cast<const std::uint8_t*>(payload.value.data()),
                    payload.value.size());
                value = wire::CreateBinaryPayload(builder, bytes).Union();
                type  = wire::PayloadValue_BinaryPayload;
            }
            else
            {
                value =
                    wire::CreateStringPayload(builder, builder.CreateString(payload.value)).Union();
                type = wire::PayloadValue_StringPayload;
            }
            values.emplace_back(wire::CreatePayload(builder, name, type, value));
        }
        return wire::CreatePayloads(builder, builder.CreateVector(values));
    }

    [[nodiscard]] exasol::udf::v2::third_party::flatbuffers::Offset<wire::Error> make_error(
        Builder& builder, const ErrorInfo& error)
    {
        return wire::CreateError(builder, builder.CreateString(error.code()),
                                 builder.CreateString(error.message()));
    }

    [[nodiscard]] Result<EncodedFrame> encode(
        std::uint64_t stream_id, const std::variant<ControlMessage, CallMessage>& message)
    {
        Builder builder;
        exasol::udf::v2::third_party::flatbuffers::Offset<wire::Payloads> payloads;
        exasol::udf::v2::third_party::flatbuffers::Offset<wire::Next> next;
        exasol::udf::v2::third_party::flatbuffers::Offset<wire::Error> error;
        exasol::udf::v2::third_party::flatbuffers::Offset<wire::OpenCall> open_call;
        exasol::udf::v2::third_party::flatbuffers::Offset<wire::CloseControlMessage> close;
        wire::ControlMessageValue value_type = wire::ControlMessageValue_NONE;
        exasol::udf::v2::third_party::flatbuffers::Offset<void> value;

        if (const auto* control = std::get_if<ControlMessage>(&message))
        {
            if (stream_id != 0)
            {
                return failure("control_stream_id", "control messages must use stream 0");
            }
            if (control->payloads)
            {
                payloads = make_payloads(builder, *control->payloads);
            }
            if (control->error)
            {
                error = make_error(builder, *control->error);
            }
            if (control->server_capabilities)
            {
                const auto& capabilities = *control->server_capabilities;
                const auto version =
                    wire::CreateVersion(builder, capabilities.major, capabilities.minor);
                const auto endian = capabilities.endianness == std::endian::little
                                        ? wire::Endianness_Little
                                        : wire::Endianness_Big;
                value_type        = wire::ControlMessageValue_ServerCapabilities;
                value = wire::CreateServerCapabilities(builder, version, endian,
                                                       capabilities.number_of_supported_workers)
                            .Union();
            }
            else if (control->keep_alive)
            {
                value_type = wire::ControlMessageValue_KeepAlive;
                value      = wire::CreateKeepAlive(builder).Union();
            }
            if (control->close_connection)
            {
                if (value_type != wire::ControlMessageValue_NONE)
                {
                    return failure(
                        "multiple_wire_variants",
                        "close_connection cannot share a frame with another wire variant");
                }
                close      = wire::CreateCloseControlMessage(builder, 0,
                                                             wire::CreateCloseConnection(builder));
                value_type = wire::ControlMessageValue_CloseControlMessage;
                value      = close.Union();
            }
        }
        else
        {
            const auto* call = std::get_if<CallMessage>(&message);
            if (stream_id == 0)
            {
                return failure("call_stream_id", "call messages cannot use stream 0");
            }
            if (has_unsupported_arrow(*call))
            {
                return failure("arrow_transport_unimplemented",
                               "Arrow schema and record-batch transport is not implemented");
            }
            if (call->payloads)
            {
                payloads = make_payloads(builder, *call->payloads);
            }
            if (call->outbound_next)
            {
                next = wire::CreateNext(builder, call->outbound_next->byte_budget,
                                        call->outbound_next->reset, call->outbound_next->row_id);
            }
            if (call->error)
            {
                error = make_error(builder, *call->error);
            }
            if (call->open_call)
            {
                open_call =
                    wire::CreateOpenCall(builder, builder.CreateString(call->open_call->call_name));
                value_type = wire::ControlMessageValue_OpenCall;
                value      = open_call.Union();
            }
            if (call->close_call)
            {
                if (value_type != wire::ControlMessageValue_NONE)
                {
                    return failure("multiple_wire_variants",
                                   "open_call and close_call cannot share a frame");
                }
                close = wire::CreateCloseControlMessage(builder, wire::CreateCloseCall(builder), 0);
                value_type = wire::ControlMessageValue_CloseControlMessage;
                value      = close.Union();
            }
        }

        const auto control =
            wire::CreateControlMessage(builder, payloads, next, 0, error, value_type, value);
        const auto frame = wire::CreateFrame(builder, stream_id, control, 0);
        wire::FinishSizePrefixedFrameBuffer(builder, frame);
        const auto* data = reinterpret_cast<const std::byte*>(builder.GetBufferPointer());
        return Result<EncodedFrame>::success(
            EncodedFrame{std::vector<std::byte>(data, data + builder.GetSize())});
    }

    [[nodiscard]] Result<Payloads> decode_payloads(const wire::Payloads* values)
    {
        Payloads payloads;
        if (values == nullptr || values->payloads() == nullptr)
        {
            return Result<Payloads>::success(std::move(payloads));
        }
        payloads.reserve(values->payloads()->size());
        for (const auto* value : *values->payloads())
        {
            if (value == nullptr || value->name() == nullptr || value->payload() == nullptr)
            {
                return Result<Payloads>::failure(
                    OperationStatus::ProtocolError,
                    ErrorInfo{"invalid_payload", "payload is incomplete"});
            }
            Payload payload;
            payload.name = value->name()->str();
            if (value->payload_type() == wire::PayloadValue_StringPayload)
            {
                const auto* string_value = value->payload_as_StringPayload();
                if (string_value == nullptr || string_value->value() == nullptr)
                {
                    return Result<Payloads>::failure(
                        OperationStatus::ProtocolError,
                        ErrorInfo{"invalid_payload", "string payload is incomplete"});
                }
                payload.value = string_value->value()->str();
            }
            else if (value->payload_type() == wire::PayloadValue_BinaryPayload)
            {
                const auto* binary_value = value->payload_as_BinaryPayload();
                if (binary_value == nullptr || binary_value->value() == nullptr)
                {
                    return Result<Payloads>::failure(
                        OperationStatus::ProtocolError,
                        ErrorInfo{"invalid_payload", "binary payload is incomplete"});
                }
                payload.binary = true;
                payload.value.assign(reinterpret_cast<const char*>(binary_value->value()->Data()),
                                     binary_value->value()->size());
            }
            else
            {
                return Result<Payloads>::failure(
                    OperationStatus::ProtocolError,
                    ErrorInfo{"invalid_payload", "unknown payload type"});
            }
            payloads.emplace_back(std::move(payload));
        }
        return Result<Payloads>::success(std::move(payloads));
    }

    [[nodiscard]] Result<DecodedFrame> decode_verified(const wire::Frame& frame)
    {
        const auto* wire_control = frame.control_message();
        if (wire_control == nullptr)
        {
            return decode_failure("frame has no control message");
        }
        const bool is_control = frame.stream_id() == 0;
        if (is_control)
        {
            ControlMessage message;
            if (wire_control->payloads())
            {
                auto payloads = decode_payloads(wire_control->payloads());
                if (!payloads)
                {
                    return Result<DecodedFrame>::failure(payloads.status, payloads.error);
                }
                message.payloads = std::move(*payloads.value);
            }
            if (wire_control->error())
            {
                message.error.emplace(wire_control->error()->code()->str(),
                                      wire_control->error()->message()->str());
            }
            if (wire_control->value_type() == wire::ControlMessageValue_ServerCapabilities)
            {
                const auto* capabilities = wire_control->value_as_ServerCapabilities();
                if (capabilities == nullptr || capabilities->supported_version() == nullptr)
                {
                    return decode_failure("server capabilities are incomplete");
                }
                message.server_capabilities = ServerCapabilities{
                    capabilities->supported_version()->major(),
                    capabilities->supported_version()->minor(),
                    capabilities->endianness() == wire::Endianness_Little ? std::endian::little
                                                                          : std::endian::big,
                    capabilities->number_of_supported_workers()};
            }
            else if (wire_control->value_type() == wire::ControlMessageValue_KeepAlive)
            {
                message.keep_alive = true;
            }
            else if (wire_control->value_type() == wire::ControlMessageValue_CloseControlMessage)
            {
                const auto* close        = wire_control->value_as_CloseControlMessage();
                message.close_connection = close != nullptr && close->close_connection() != nullptr;
                if (close != nullptr && close->close_call() != nullptr)
                {
                    return decode_failure("close_call is not valid on stream 0");
                }
            }
            if (wire_control->next() != nullptr || wire_control->data_schema() != nullptr)
            {
                return decode_failure("call fields are not valid on stream 0");
            }
            return Result<DecodedFrame>::success(DecodedFrame{0, std::move(message)});
        }

        CallMessage message;
        if (wire_control->payloads())
        {
            auto payloads = decode_payloads(wire_control->payloads());
            if (!payloads)
            {
                return Result<DecodedFrame>::failure(payloads.status, payloads.error);
            }
            message.payloads = std::move(*payloads.value);
        }
        if (wire_control->next())
        {
            message.next = Next{wire_control->next()->reset(), wire_control->next()->row_id()};
        }
        if (wire_control->error())
        {
            message.error.emplace(wire_control->error()->code()->str(),
                                  wire_control->error()->message()->str());
        }
        switch (wire_control->value_type())
        {
            case wire::ControlMessageValue_NONE:
                break;
            case wire::ControlMessageValue_OpenCall:
            {
                const auto* open = wire_control->value_as_OpenCall();
                if (open == nullptr || open->call_name() == nullptr)
                {
                    return decode_failure("open_call is incomplete");
                }
                message.open_call = OpenCall{open->call_name()->str()};
                break;
            }
            case wire::ControlMessageValue_CloseControlMessage:
            {
                const auto* close = wire_control->value_as_CloseControlMessage();
                if (close == nullptr || close->close_connection() != nullptr)
                {
                    return decode_failure("close_connection is not valid on a call stream");
                }
                message.close_call = close->close_call() != nullptr;
                break;
            }
            default:
                return decode_failure("control-only value on a call stream");
        }
        if (wire_control->data_schema() != nullptr || frame.data_record_batch_metadata() != nullptr)
        {
            return decode_failure("Arrow schema and record-batch transport is not implemented");
        }
        return Result<DecodedFrame>::success(DecodedFrame{frame.stream_id(), std::move(message)});
    }

} // namespace

Result<EncodedFrame> encode_control(ControlMessage message)
{
    return encode(0, std::move(message));
}

Result<EncodedFrame> encode_call(const std::uint64_t stream_id, CallMessage message)
{
    return encode(stream_id, std::move(message));
}

Result<DecodedFrame> decode(const std::span<const std::byte> frame)
{
    if (frame.empty() || frame.size() > std::numeric_limits<std::uint32_t>::max())
    {
        return decode_failure("frame is empty or too large");
    }
    Verifier verifier(reinterpret_cast<const std::uint8_t*>(frame.data()), frame.size());
    if (!wire::VerifySizePrefixedFrameBuffer(verifier))
    {
        return decode_failure("FlatBuffer verification failed");
    }
    const auto* decoded = wire::GetSizePrefixedFrame(frame.data());
    if (decoded == nullptr)
    {
        return decode_failure("FlatBuffer root is missing");
    }
    return decode_verified(*decoded);
}

} // namespace exasol::udf::v2::context::protocol
