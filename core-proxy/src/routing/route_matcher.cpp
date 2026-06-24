#include "edgeshield/routing/route_matcher.hpp"

namespace edgeshield {
using namespace std;

bool starts_with(const string& value, const string& prefix) {
  return value.rfind(prefix, 0) == 0;
}

const RouteConfig* match_route(const vector<RouteConfig>& routes, const string& target) {
  const RouteConfig* best = nullptr;
  for (const auto& route : routes) {
    if (starts_with(target, route.prefix) && (!best || route.prefix.size() > best->prefix.size())) {
      best = &route;
    }
  }
  return best;
}

}  // namespace edgeshield
