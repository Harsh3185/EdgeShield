#include "edgeshield/backend/backend_selector.hpp"

#include "edgeshield/backend/circuit_breaker.hpp"

namespace edgeshield {
using namespace std;

optional<BackendConfig> choose_round_robin_backend(const RouteConfig& route,
                                                   unordered_map<string, BackendRuntime>& backends,
                                                   unordered_map<string, size_t>& rr_index) {
  auto& cursor = rr_index[route.prefix];
  if (route.backend_pool.empty()) {
    return nullopt;
  }

  for (size_t attempt = 0; attempt < route.backend_pool.size(); ++attempt) {
    const auto index = (cursor + attempt) % route.backend_pool.size();
    auto found = backends.find(route.backend_pool[index]);
    if (found == backends.end()) {
      continue;
    }
    auto& backend = found->second;
    if (backend.manually_enabled && backend.healthy && circuit_allows(backend)) {
      cursor = (index + 1) % route.backend_pool.size();
      ++backend.selected_count;
      return backend.config;
    }
  }
  return nullopt;
}

}  // namespace edgeshield
