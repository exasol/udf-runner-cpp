#pragma once

#include <exasol/udf/v2/interface/message_view/call_message_views.hpp>
#include <exasol/udf/v2/udf_protocol.hpp>

#include <memory>

namespace exasol::udf::v2::internal
{

std::unique_ptr<interface::OpenCallView> make_open_call_view(
    std::shared_ptr<const protocol::DecodedFrame> frame, const protocol::OpenCall* value);
std::unique_ptr<interface::ResetView> make_reset_view(
    std::shared_ptr<const protocol::DecodedFrame> frame, const protocol::Reset* value);
std::unique_ptr<interface::CloseCallView> make_close_call_view(
    std::shared_ptr<const protocol::DecodedFrame> frame, const protocol::CloseCall* value);

} // namespace exasol::udf::v2::internal
