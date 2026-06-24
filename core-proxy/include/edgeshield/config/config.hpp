#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

namespace edgeshield {

struct ProxyConfig {
  std::string host = "0.0.0.0";
  uint16_t port = 8080;
  bool https_enabled = false;
  uint16_t https_port = 8443;
  std::string tls_cert_file;
  std::string tls_key_file;
  uint16_t metrics_port = 9090;
  std::size_t worker_threads = 4;
};

struct TimeoutConfig {
  std::chrono::milliseconds connect{1000};
  std::chrono::milliseconds backend_response{3000};
  std::chrono::milliseconds health_check{1000};
};

struct BackendConfig {
  std::string id;
  std::string host;
  uint16_t port = 0;
  std::string health_path = "/health";
  bool enabled = true;
};

struct CacheConfig {
  bool enabled = false;
  int ttl_seconds = 30;
  std::unordered_set<std::string> methods{"GET"};
  std::size_t max_entries = 512;
};

struct RateLimitConfig {
  bool enabled = false;
  int requests = 30;
  int window_seconds = 60;
};

struct RedisConfig {
  bool enabled = false;
  std::string host = "127.0.0.1";
  uint16_t port = 6379;
  std::string key_prefix = "edgeshield:rate";
};

struct RetryConfig {
  bool enabled = false;
  int attempts = 1;
  std::unordered_set<std::string> methods{"GET", "HEAD"};
};

struct CircuitBreakerConfig {
  bool enabled = false;
  int failure_threshold = 3;
  int cooldown_seconds = 10;
};

struct RouteConfig {
  std::string prefix = "/";
  std::vector<std::string> backend_pool;
  std::string load_balancer = "round_robin";
  CacheConfig cache;
  RateLimitConfig rate_limit;
  RetryConfig retry;
  CircuitBreakerConfig circuit_breaker;
};

struct AppConfig {
  ProxyConfig proxy;
  TimeoutConfig timeouts;
  RedisConfig redis;
  std::vector<BackendConfig> backends;
  std::vector<RouteConfig> routes;
};

AppConfig load_config(const std::string& path);
AppConfig parse_config_text(const std::string& text);
bool method_allowed(const std::unordered_set<std::string>& methods, const std::string& method);

}  // namespace edgeshield
