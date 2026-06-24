#pragma once

#include "edgeshield/config/config.hpp"

#include <string>
#include <vector>

namespace edgeshield {

bool starts_with(const std::string& value, const std::string& prefix);
const RouteConfig* match_route(const std::vector<RouteConfig>& routes, const std::string& target);

}  // namespace edgeshield
