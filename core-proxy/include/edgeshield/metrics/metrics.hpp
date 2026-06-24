#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <mutex>

namespace edgeshield {

struct MetricsSnapshot {
  std::uint64_t total_requests = 0;
  std::uint64_t active_requests = 0;
  std::uint64_t backend_errors = 0;
  std::uint64_t timeouts = 0;
  std::uint64_t rate_limited = 0;
  std::uint64_t cache_hits = 0;
  std::uint64_t cache_misses = 0;
  std::uint64_t bytes_in = 0;
  std::uint64_t bytes_out = 0;
  double p95_latency_ms = 0.0;
};

class Metrics {
 public:
  void reset();
  void start_request();
  void finish_request(long latency_ms, std::uint64_t bytes_in, std::uint64_t bytes_out);
  void increment_backend_error();
  void increment_timeout();
  void increment_rate_limited();
  void increment_cache_hit();
  void increment_cache_miss();
  MetricsSnapshot snapshot() const;

 private:
  std::atomic<std::uint64_t> total_requests_{0};
  std::atomic<std::uint64_t> active_requests_{0};
  std::atomic<std::uint64_t> backend_errors_{0};
  std::atomic<std::uint64_t> timeouts_{0};
  std::atomic<std::uint64_t> rate_limited_{0};
  std::atomic<std::uint64_t> cache_hits_{0};
  std::atomic<std::uint64_t> cache_misses_{0};
  std::atomic<std::uint64_t> bytes_in_{0};
  std::atomic<std::uint64_t> bytes_out_{0};

  mutable std::mutex latency_mutex_;
  std::deque<long> latencies_ms_;
};

}  // namespace edgeshield
