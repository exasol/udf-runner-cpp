#pragma once

#include <exasol/udf/v2/interface/message_view/control_message_flatbuffer_views.hpp>
#include <exasol/udf/v2/udf_protocol.hpp>

#include <memory>

namespace exasol::udf::v2::internal
{

std::unique_ptr<interface::ServerCapabilitiesView> make_server_capabilities_view(
    std::shared_ptr<const protocol::DecodedFrame> frame, const protocol::ServerCapabilities* value);
std::unique_ptr<interface::KeepAliveView> make_keep_alive_view(
    std::shared_ptr<const protocol::DecodedFrame> frame, const protocol::KeepAlive* value);
std::unique_ptr<interface::CloseConnectionView> make_close_connection_view(
    std::shared_ptr<const protocol::DecodedFrame> frame, const protocol::CloseConnection* value);

} // namespace exasol::udf::v2::internal
