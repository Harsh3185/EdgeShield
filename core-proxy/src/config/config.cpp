#include "edgeshield/config/config.hpp"

#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>

namespace edgeshield {
using namespace std;
namespace {

unordered_set<string> string_set(const nlohmann::json& value) {
  unordered_set<string> out;
  for (const auto& item : value) {
    out.insert(item.get<string>());
  }
  return out;
}

}

AppConfig parse_config_text(const string& text) {
  const auto root = nlohmann::json::parse(text);
  AppConfig config;

  const auto& proxy = root.at("proxy");
  config.proxy.host = proxy.value("host", config.proxy.host);
  config.proxy.port = proxy.value("port", config.proxy.port);
  config.proxy.https_enabled = proxy.value("https_enabled", config.proxy.https_enabled);
  config.proxy.https_port = proxy.value("https_port", config.proxy.https_port);
  config.proxy.tls_cert_file = proxy.value("tls_cert_file", config.proxy.tls_cert_file);
  config.proxy.tls_key_file = proxy.value("tls_key_file", config.proxy.tls_key_file);
  config.proxy.metrics_port = proxy.value("metrics_port", config.proxy.metrics_port);
  config.proxy.worker_threads = proxy.value("worker_threads", config.proxy.worker_threads);

  if (config.proxy.https_enabled &&
      (config.proxy.tls_cert_file.empty() || config.proxy.tls_key_file.empty())) {
    throw runtime_error("HTTPS is enabled but tls_cert_file or tls_key_file is missing");
  }

  if (root.contains("timeouts_ms")) {
    const auto& timeouts = root.at("timeouts_ms");
    config.timeouts.connect = chrono::milliseconds(timeouts.value("connect", config.timeouts.connect.count()));
    config.timeouts.backend_response =
        chrono::milliseconds(timeouts.value("backend_response", config.timeouts.backend_response.count()));
    config.timeouts.health_check =
        chrono::milliseconds(timeouts.value("health_check", config.timeouts.health_check.count()));
  }

  if (root.contains("redis")) {
    const auto& redis = root.at("redis");
    config.redis.enabled = redis.value("enabled", config.redis.enabled);
    config.redis.host = redis.value("host", config.redis.host);
    config.redis.port = redis.value("port", config.redis.port);
    config.redis.key_prefix = redis.value("key_prefix", config.redis.key_prefix);
  }

  for (const auto& item : root.at("backends")) {
    BackendConfig backend;
    backend.id = item.at("id").get<string>();
    backend.host = item.at("host").get<string>();
    backend.port = item.at("port").get<uint16_t>();
    backend.health_path = item.value("health_path", backend.health_path);
    backend.enabled = item.value("enabled", true);
    config.backends.push_back(backend);
  }

  for (const auto& item : root.at("routes")) {
    RouteConfig route;
    route.prefix = item.value("prefix", route.prefix);
    route.backend_pool = item.at("backend_pool").get<vector<string>>();
    route.load_balancer = item.value("load_balancer", route.load_balancer);

    if (item.contains("cache")) {
      const auto& cache = item.at("cache");
      route.cache.enabled = cache.value("enabled", route.cache.enabled);
      route.cache.ttl_seconds = cache.value("ttl_seconds", route.cache.ttl_seconds);
      route.cache.max_entries = cache.value("max_entries", route.cache.max_entries);
      if (cache.contains("methods")) {
        route.cache.methods = string_set(cache.at("methods"));
      }
    }

    if (item.contains("rate_limit")) {
      const auto& rate = item.at("rate_limit");
      route.rate_limit.enabled = rate.value("enabled", route.rate_limit.enabled);
      route.rate_limit.requests = rate.value("requests", route.rate_limit.requests);
      route.rate_limit.window_seconds = rate.value("window_seconds", route.rate_limit.window_seconds);
    }

    if (item.contains("retry")) {
      const auto& retry = item.at("retry");
      route.retry.enabled = retry.value("enabled", route.retry.enabled);
      route.retry.attempts = retry.value("attempts", route.retry.attempts);
      if (retry.contains("methods")) {
        route.retry.methods = string_set(retry.at("methods"));
      }
    }

    if (item.contains("circuit_breaker")) {
      const auto& breaker = item.at("circuit_breaker");
      route.circuit_breaker.enabled = breaker.value("enabled", route.circuit_breaker.enabled);
      route.circuit_breaker.failure_threshold =
          breaker.value("failure_threshold", route.circuit_breaker.failure_threshold);
      route.circuit_breaker.cooldown_seconds =
          breaker.value("cooldown_seconds", route.circuit_breaker.cooldown_seconds);
    }

    config.routes.push_back(route);
  }

  if (config.backends.empty()) {
    throw runtime_error("EdgeShield config must define at least one backend");
  }
  if (config.routes.empty()) {
    throw runtime_error("EdgeShield config must define at least one route");
  }
  return config;
}

AppConfig load_config(const string& path) {
  ifstream file(path);
  if (!file) {
    throw runtime_error("Could not open config file: " + path);
  }
  ostringstream buffer;
  buffer << file.rdbuf();
  return parse_config_text(buffer.str());
}

bool method_allowed(const unordered_set<string>& methods, const string& method) {
  return methods.contains(method);
}

}
