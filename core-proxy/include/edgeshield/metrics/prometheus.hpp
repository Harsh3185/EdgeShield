#pragma once

#include "edgeshield/backend/backend_runtime.hpp"
#include "edgeshield/metrics/metrics.hpp"

#include <string>
#include <unordered_map>

namespace edgeshield {

std::string prometheus_text(const MetricsSnapshot& metrics,
                            const std::unordered_map<std::string, BackendRuntime>& backends);

}  // namespace edgeshield
