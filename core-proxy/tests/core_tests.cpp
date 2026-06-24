#include "edgeshield/config.hpp"
#include "edgeshield/gateway.hpp"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <thread>

using namespace edgeshield;
using namespace std;

namespace {

const char* kConfig = R"json(
{
  "proxy": {"host": "0.0.0.0", "port": 8080, "metrics_port": 9090, "worker_threads": 2},
  "timeouts_ms": {"connect": 100, "backend_response": 200, "health_check": 50},
  "redis": {"enabled": false, "host": "127.0.0.1", "port": 6379, "key_prefix": "test:rate"},
  "backends": [
    {"id": "a", "host": "127.0.0.1", "port": 3001, "health_path": "/health", "enabled": true},
    {"id": "b", "host": "127.0.0.1", "port": 3002, "health_path": "/health", "enabled": true}
  ],
  "routes": [
    {
      "prefix": "/api",
      "backend_pool": ["a", "b"],
      "load_balancer": "round_robin",
      "cache": {"enabled": true, "ttl_seconds": 1, "methods": ["GET"], "max_entries": 2},
      "rate_limit": {"enabled": true, "requests": 2, "window_seconds": 1},
      "retry": {"enabled": true, "attempts": 2, "methods": ["GET", "HEAD"]},
      "circuit_breaker": {"enabled": true, "failure_threshold": 2, "cooldown_seconds": 1}
    }
  ]
}
)json";

}

TEST_CASE("JSON config parses proxy routes and policies") {
  auto config = parse_config_text(kConfig);
  REQUIRE(config.proxy.port == 8080);
  REQUIRE(config.proxy.metrics_port == 9090);
  REQUIRE(config.proxy.worker_threads == 2);
  REQUIRE(config.backends.size() == 2);
  REQUIRE(config.redis.key_prefix == "test:rate");
  REQUIRE(config.routes.size() == 1);
  REQUIRE(config.routes[0].prefix == "/api");
  REQUIRE(config.routes[0].cache.enabled);
  REQUIRE(config.routes[0].rate_limit.requests == 2);
  REQUIRE(method_allowed(config.routes[0].retry.methods, "GET"));
  REQUIRE_FALSE(method_allowed(config.routes[0].retry.methods, "POST"));
}

TEST_CASE("route matching picks configured prefix") {
  GatewayState state(parse_config_text(kConfig));
  auto* route = state.match_route("/api/users");
  REQUIRE(route != nullptr);
  REQUIRE(route->prefix == "/api");
  REQUIRE(state.match_route("/other") == nullptr);
}

TEST_CASE("round robin only selects healthy enabled backends") {
  auto config = parse_config_text(kConfig);
  config.backends[0].enabled = false;
  GatewayState state(config);
  auto* route = state.match_route("/api/users");
  REQUIRE(route != nullptr);

  for (int i = 0; i < 4; ++i) {
    auto chosen = state.choose_backend(*route);
    REQUIRE(chosen);
    REQUIRE(chosen->id == "b");
  }
}

TEST_CASE("cache stores hit then expires by ttl") {
  GatewayState state(parse_config_text(kConfig));
  CachedResponse response;
  response.status = 200;
  response.body = "cached";
  response.expires_at = chrono::steady_clock::now() + chrono::milliseconds(20);

  state.put_cached("GET /api/users", response, 2);
  REQUIRE(state.get_cached("GET /api/users"));
  this_thread::sleep_for(chrono::milliseconds(30));
  REQUIRE_FALSE(state.get_cached("GET /api/users"));
}

TEST_CASE("rate limiter blocks after configured request count") {
  GatewayState state(parse_config_text(kConfig));
  const auto* route = state.match_route("/api/users");
  REQUIRE(route != nullptr);

  REQUIRE(state.check_rate_limit("client", route->rate_limit).allowed);
  REQUIRE(state.check_rate_limit("client", route->rate_limit).allowed);
  REQUIRE_FALSE(state.check_rate_limit("client", route->rate_limit).allowed);
}

TEST_CASE("circuit breaker opens after failures and skips backend") {
  GatewayState state(parse_config_text(kConfig));
  const auto* route = state.match_route("/api/users");
  REQUIRE(route != nullptr);

  state.record_backend_result("a", false, route->circuit_breaker);
  state.record_backend_result("a", false, route->circuit_breaker);
  for (int i = 0; i < 4; ++i) {
    auto chosen = state.choose_backend(*route);
    REQUIRE(chosen);
    REQUIRE(chosen->id == "b");
  }
}
