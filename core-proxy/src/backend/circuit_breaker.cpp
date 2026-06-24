#include "edgeshield/backend/circuit_breaker.hpp"

namespace edgeshield {
using namespace std;

bool circuit_allows(const BackendRuntime& backend) {
  return backend.circuit_open_until <= chrono::steady_clock::now();
}

void record_backend_result(BackendRuntime& backend, bool ok, const CircuitBreakerConfig& breaker) {
  if (ok) {
    backend.healthy = backend.manually_enabled;
    backend.consecutive_failures = 0;
    backend.circuit_open_until = {};
    return;
  }

  backend.healthy = false;
  ++backend.consecutive_failures;
  if (breaker.enabled && backend.consecutive_failures >= breaker.failure_threshold) {
    backend.circuit_open_until =
        chrono::steady_clock::now() + chrono::seconds(breaker.cooldown_seconds);
  }
}

}  // namespace edgeshield
