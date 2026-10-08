#pragma once

#include <filesystem>
#include <optional>

#include <nlohmann/json.hpp>

namespace exasol::udf::simulator
{

using Json = nlohmann::json;

enum class Role
{
    Client,
    Runner,
};

Json default_steps(Role role);
Json load_steps(const std::filesystem::path& path);
void run(Role role, const std::filesystem::path& socket_path, const Json& steps,
         int timeout_seconds = 10);

} // namespace exasol::udf::simulator
