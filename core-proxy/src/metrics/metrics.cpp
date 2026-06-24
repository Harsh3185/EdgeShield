#include "edgeshield/metrics/metrics.hpp"

#include <algorithm>
#include <vector>

namespace edgeshield {
using namespace std;
namespace {

constexpr size_t kMaxLatencies = 1024;

}

void Metrics::reset() {
  total_requests_ = 0;
  active_requests_ = 0;
  backend_errors_ = 0;
  timeouts_ = 0;
  rate_limited_ = 0;
  cache_hits_ = 0;
  cache_misses_ = 0;
  bytes_in_ = 0;
  bytes_out_ = 0;

  lock_guard lock(latency_mutex_);
  latencies_ms_.clear();
}

void Metrics::start_request() {
  ++total_requests_;
  ++active_requests_;
}

void Metrics::finish_request(long latency_ms, uint64_t bytes_in, uint64_t bytes_out) {
  auto active = active_requests_.load();
  while (active > 0 && !active_requests_.compare_exchange_weak(active, active - 1)) {
  }
  bytes_in_ += bytes_in;
  bytes_out_ += bytes_out;
  lock_guard lock(latency_mutex_);
  latencies_ms_.push_back(latency_ms);
  while (latencies_ms_.size() > kMaxLatencies) {
    latencies_ms_.pop_front();
  }
}

void Metrics::increment_backend_error() { ++backend_errors_; }
void Metrics::increment_timeout() { ++timeouts_; }
void Metrics::increment_rate_limited() { ++rate_limited_; }
void Metrics::increment_cache_hit() { ++cache_hits_; }
void Metrics::increment_cache_miss() { ++cache_misses_; }

MetricsSnapshot Metrics::snapshot() const {
  MetricsSnapshot snapshot;
  snapshot.total_requests = total_requests_.load();
  snapshot.active_requests = active_requests_.load();
  snapshot.backend_errors = backend_errors_.load();
  snapshot.timeouts = timeouts_.load();
  snapshot.rate_limited = rate_limited_.load();
  snapshot.cache_hits = cache_hits_.load();
  snapshot.cache_misses = cache_misses_.load();
  snapshot.bytes_in = bytes_in_.load();
  snapshot.bytes_out = bytes_out_.load();

  lock_guard lock(latency_mutex_);
  if (!latencies_ms_.empty()) {
    vector<long> values(latencies_ms_.begin(), latencies_ms_.end());
    sort(values.begin(), values.end());
    const auto index = static_cast<size_t>((values.size() - 1) * 0.95);
    snapshot.p95_latency_ms = static_cast<double>(values[index]);
  }
  return snapshot;
}

}  // namespace edgeshield
