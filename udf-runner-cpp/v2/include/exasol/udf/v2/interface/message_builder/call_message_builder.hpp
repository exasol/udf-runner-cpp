#pragma once

#include <exasol/udf/v2/interface/message_builder/call_message_builder_base.hpp>

namespace exasol::udf::v2::interface
{

/// Builds one composite message for a call stream.
class CallMessageBuilder : public CallMessageBuilderBase
{
protected:
    CallMessageBuilder() = default;
};

/// Builds the first message on a newly opened call stream.
class OpeningCallMessageBuilder : public CallMessageBuilderBase
{
public:
    virtual OpeningCallMessageBuilder& open_call(std::string_view name) = 0;

protected:
    OpeningCallMessageBuilder() = default;
};

} // namespace exasol::udf::v2::interface
