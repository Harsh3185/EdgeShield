#pragma once

#include "edgeshield/config/config.hpp"

#include <chrono>
#include <cstdint>

namespace edgeshield {

struct BackendRuntime {
  BackendConfig config;
  bool healthy = true;
  bool manually_enabled = true;
  int consecutive_failures = 0;
  std::chrono::steady_clock::time_point circuit_open_until{};
  std::uint64_t selected_count = 0;
};

}  // namespace edgeshield
