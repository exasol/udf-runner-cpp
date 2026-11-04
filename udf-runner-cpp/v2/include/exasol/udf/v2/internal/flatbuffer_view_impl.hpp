#pragma once

#include <exasol/udf/v2/interface/message_view/flatbuffer_views.hpp>
#include <exasol/udf/v2/udf_protocol.hpp>

#include <memory>

namespace exasol::udf::v2::internal
{

std::unique_ptr<interface::OpenCallView> make_open_call_view(
    std::shared_ptr<const protocol::DecodedFrame> frame, const protocol::OpenCall* value);
std::unique_ptr<interface::PayloadsView> make_payloads_view(
    std::shared_ptr<const protocol::DecodedFrame> frame, const protocol::Payloads* value);
std::unique_ptr<interface::ResetView> make_reset_view(
    std::shared_ptr<const protocol::DecodedFrame> frame, const protocol::Reset* value);
std::unique_ptr<interface::ErrorView> make_error_view(
    std::shared_ptr<const protocol::DecodedFrame> frame, const protocol::Error* value);
std::unique_ptr<interface::CloseCallView> make_close_call_view(
    std::shared_ptr<const protocol::DecodedFrame> frame, const protocol::CloseCall* value);
std::unique_ptr<interface::ServerCapabilitiesView> make_server_capabilities_view(
    std::shared_ptr<const protocol::DecodedFrame> frame, const protocol::ServerCapabilities* value);
std::unique_ptr<interface::KeepAliveView> make_keep_alive_view(
    std::shared_ptr<const protocol::DecodedFrame> frame, const protocol::KeepAlive* value);
std::unique_ptr<interface::CloseConnectionView> make_close_connection_view(
    std::shared_ptr<const protocol::DecodedFrame> frame, const protocol::CloseConnection* value);

} // namespace exasol::udf::v2::internal
