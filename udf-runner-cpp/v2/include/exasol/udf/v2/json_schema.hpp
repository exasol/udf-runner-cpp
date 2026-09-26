#pragma once

#if defined(nlohmann) || defined(INCLUDE_NLOHMANN_JSON_HPP_)
#error "Include exasol/udf/v2/json_schema.hpp before any nlohmann JSON header."
#endif

// Keep the upstream JSON types out of the global nlohmann namespace.
#define nlohmann exasol::udf::v2::third_party::nlohmann
#include <nlohmann/json.hpp>
#include <nlohmann/json-schema.hpp>
#undef nlohmann
