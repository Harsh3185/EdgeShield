#include "edgeshield/state/gateway_state.hpp"

#include "edgeshield/backend/backend_selector.hpp"
#include "edgeshield/backend/circuit_breaker.hpp"
#include "edgeshield/metrics/prometheus.hpp"
#include "edgeshield/routing/route_matcher.hpp"

#include <atomic>
#include <algorithm>
#include <chrono>
#include <random>
#include <sstream>

namespace edgeshield {
using namespace std;

GatewayState::GatewayState(AppConfig config) : config_(move(config)) {
  for (const auto& backend : config_.backends) {
    BackendRuntime runtime;
    runtime.config = backend;
    runtime.healthy = backend.enabled;
    runtime.manually_enabled = backend.enabled;
    backends_.emplace(backend.id, runtime);
  }
}

const AppConfig& GatewayState::config() const { return config_; }

AppConfig GatewayState::config_copy() const {
  shared_lock lock(config_mutex_);
  return config_;
}

TimeoutConfig GatewayState::timeouts() const {
  shared_lock lock(config_mutex_);
  return config_.timeouts;
}

vector<BackendConfig> GatewayState::backend_configs() const {
  shared_lock lock(config_mutex_);
  return config_.backends;
}

const RouteConfig* GatewayState::match_route(const string& target) const {
  shared_lock lock(config_mutex_);
  return edgeshield::match_route(config_.routes, target);
}

optional<BackendConfig> GatewayState::choose_backend(const RouteConfig& route) {
  unique_lock rr_lock(rr_mutex_);
  unique_lock backend_lock(backend_mutex_);
  return choose_round_robin_backend(route, backends_, rr_index_);
}

void GatewayState::record_backend_result(const string& backend_id, bool ok, const CircuitBreakerConfig& breaker) {
  unique_lock lock(backend_mutex_);
  auto found = backends_.find(backend_id);
  if (found == backends_.end()) {
    return;
  }

  edgeshield::record_backend_result(found->second, ok, breaker);
}

string GatewayState::cache_key(const string& method, const string& target) const {
  return edgeshield::cache_key(method, target);
}

optional<CachedResponse> GatewayState::get_cached(const string& key) {
  lock_guard lock(cache_mutex_);
  return edgeshield::get_cached(cache_, key);
}

void GatewayState::put_cached(const string& key, CachedResponse response, size_t max_entries) {
  lock_guard lock(cache_mutex_);
  edgeshield::put_cached(cache_, key, move(response), max_entries);
}

RateDecision GatewayState::check_rate_limit(const string& client_key, const RateLimitConfig& config) {
  const auto redis = [this] {
    shared_lock lock(config_mutex_);
    return config_.redis;
  }();

  if (redis.enabled) {
    const auto key = redis.key_prefix + ":" + client_key;
    if (const auto count = redis_incr_with_expire(redis, key, config.window_seconds)) {
      return RateDecision{*count <= config.requests, max(0, config.requests - *count)};
    }
  }

  lock_guard lock(rate_mutex_);
  return edgeshield::check_rate_limit(RedisConfig{}, rate_buckets_, client_key, config);
}

void GatewayState::start_request() { metrics_.start_request(); }

void GatewayState::finish_request(long latency_ms, uint64_t bytes_in, uint64_t bytes_out) {
  metrics_.finish_request(latency_ms, bytes_in, bytes_out);
}

void GatewayState::increment_backend_error() { metrics_.increment_backend_error(); }
void GatewayState::increment_timeout() { metrics_.increment_timeout(); }
void GatewayState::increment_rate_limited() { metrics_.increment_rate_limited(); }
void GatewayState::increment_cache_hit() { metrics_.increment_cache_hit(); }
void GatewayState::increment_cache_miss() { metrics_.increment_cache_miss(); }

MetricsSnapshot GatewayState::metrics_snapshot() const {
  return metrics_.snapshot();
}

void GatewayState::add_log(ProxyLog log) {
  logs_.add(move(log));
}

string GatewayState::prometheus_text() const {
  shared_lock lock(backend_mutex_);
  return edgeshield::prometheus_text(metrics_snapshot(), backends_);
}

string generate_request_id() {
  static atomic<uint64_t> counter{0};
  const auto now = chrono::duration_cast<chrono::milliseconds>(
                       chrono::system_clock::now().time_since_epoch())
                       .count();
  ostringstream out;
  out << "es-" << now << "-" << ++counter;
  return out.str();
}

string client_ip_from_endpoint(const string& endpoint) {
  const auto pos = endpoint.rfind(':');
  if (pos == string::npos) {
    return endpoint;
  }
  return endpoint.substr(0, pos);
}

}  // namespace edgeshield
