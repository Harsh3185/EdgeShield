#pragma once

#include "edgeshield/backend/backend_runtime.hpp"
#include "edgeshield/cache/response_cache.hpp"
#include "edgeshield/config/config.hpp"
#include "edgeshield/metrics/metrics.hpp"
#include "edgeshield/metrics/proxy_log.hpp"
#include "edgeshield/ratelimit/rate_limiter.hpp"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace edgeshield {

class GatewayState {
 public:
  explicit GatewayState(AppConfig config);

  const AppConfig& config() const;
  AppConfig config_copy() const;
  TimeoutConfig timeouts() const;
  std::vector<BackendConfig> backend_configs() const;
  const RouteConfig* match_route(const std::string& target) const;
  std::optional<BackendConfig> choose_backend(const RouteConfig& route);
  void record_backend_result(const std::string& backend_id, bool ok, const CircuitBreakerConfig& breaker);

  std::string cache_key(const std::string& method, const std::string& target) const;
  std::optional<CachedResponse> get_cached(const std::string& key);
  void put_cached(const std::string& key, CachedResponse response, std::size_t max_entries);

  RateDecision check_rate_limit(const std::string& client_key, const RateLimitConfig& config);

  void start_request();
  void finish_request(long latency_ms, std::uint64_t bytes_in, std::uint64_t bytes_out);
  void increment_backend_error();
  void increment_timeout();
  void increment_rate_limited();
  void increment_cache_hit();
  void increment_cache_miss();
  MetricsSnapshot metrics_snapshot() const;
  void add_log(ProxyLog log);

  std::string prometheus_text() const;

 private:
  AppConfig config_;
  mutable std::shared_mutex config_mutex_;
  mutable std::shared_mutex backend_mutex_;
  std::unordered_map<std::string, BackendRuntime> backends_;
  mutable std::mutex rr_mutex_;
  std::unordered_map<std::string, std::size_t> rr_index_;

  mutable std::mutex cache_mutex_;
  std::unordered_map<std::string, CachedResponse> cache_;

  mutable std::mutex rate_mutex_;
  std::unordered_map<std::string, RateBucket> rate_buckets_;

  Metrics metrics_;
  ProxyLogStore logs_;
};

std::string generate_request_id();
std::string client_ip_from_endpoint(const std::string& endpoint);

}  // namespace edgeshield
