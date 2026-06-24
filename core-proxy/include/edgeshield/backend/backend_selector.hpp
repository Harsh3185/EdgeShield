#pragma once

#include "edgeshield/backend/backend_runtime.hpp"
#include "edgeshield/config/config.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>

namespace edgeshield {

std::optional<BackendConfig> choose_round_robin_backend(
    const RouteConfig& route, std::unordered_map<std::string, BackendRuntime>& backends,
    std::unordered_map<std::string, std::size_t>& rr_index);

}  // namespace edgeshield
