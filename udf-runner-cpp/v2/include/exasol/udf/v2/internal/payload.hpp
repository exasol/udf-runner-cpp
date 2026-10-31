#pragma once

#include <string>

namespace exasol::udf::v2::internal
{

/// One named text or binary payload retained by a message builder.
struct Payload
{
    std::string name;
    std::string value;
    bool binary{false};
};

} // namespace exasol::udf::v2::internal
