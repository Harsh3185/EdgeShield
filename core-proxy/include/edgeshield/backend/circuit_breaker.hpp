#pragma once

#include "edgeshield/backend/backend_runtime.hpp"
#include "edgeshield/config/config.hpp"

namespace edgeshield {

bool circuit_allows(const BackendRuntime& backend);
void record_backend_result(BackendRuntime& backend, bool ok, const CircuitBreakerConfig& breaker);

}  // namespace edgeshield
