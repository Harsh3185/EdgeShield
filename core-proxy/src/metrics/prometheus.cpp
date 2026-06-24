#include "edgeshield/metrics/prometheus.hpp"

#include <sstream>

namespace edgeshield {
using namespace std;

string prometheus_text(const MetricsSnapshot& metrics, const unordered_map<string, BackendRuntime>& backends) {
  ostringstream out;
  out << "# HELP edgeshield_total_requests Total proxied requests\n";
  out << "# TYPE edgeshield_total_requests counter\n";
  out << "edgeshield_total_requests " << metrics.total_requests << "\n";
  out << "edgeshield_active_requests " << metrics.active_requests << "\n";
  out << "edgeshield_backend_errors " << metrics.backend_errors << "\n";
  out << "edgeshield_timeouts " << metrics.timeouts << "\n";
  out << "edgeshield_rate_limited " << metrics.rate_limited << "\n";
  out << "edgeshield_cache_hits " << metrics.cache_hits << "\n";
  out << "edgeshield_cache_misses " << metrics.cache_misses << "\n";
  out << "edgeshield_bytes_in " << metrics.bytes_in << "\n";
  out << "edgeshield_bytes_out " << metrics.bytes_out << "\n";
  out << "edgeshield_p95_latency_ms " << metrics.p95_latency_ms << "\n";

  for (const auto& [id, backend] : backends) {
    out << "edgeshield_backend_selected_total{backend=\"" << id << "\"} " << backend.selected_count << "\n";
    out << "edgeshield_backend_healthy{backend=\"" << id << "\"} " << (backend.healthy ? 1 : 0) << "\n";
    out << "edgeshield_backend_enabled{backend=\"" << id << "\"} " << (backend.manually_enabled ? 1 : 0) << "\n";
    out << "edgeshield_backend_consecutive_failures{backend=\"" << id << "\"} " << backend.consecutive_failures
        << "\n";
  }
  return out.str();
}

}  // namespace edgeshield
