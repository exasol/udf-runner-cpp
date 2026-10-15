#pragma once

#include <chrono>
#include <optional>

namespace exasol::udf::v2
{

/// Optional deadline duration used by receive and wait operations.
using Timeout = std::optional<std::chrono::steady_clock::duration>;

} // namespace exasol::udf::v2
