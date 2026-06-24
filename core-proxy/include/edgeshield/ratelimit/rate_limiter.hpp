#pragma once

#include "edgeshield/config/config.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <unordered_map>

namespace edgeshield {

struct RateDecision {
  bool allowed = true;
  int remaining = 0;
};

struct RateBucket {
  int count = 0;
  std::chrono::steady_clock::time_point window_start;
};

std::optional<int> redis_incr_with_expire(const RedisConfig& redis, const std::string& key,
                                          int window_seconds);
RateDecision check_rate_limit(const RedisConfig& redis,
                              std::unordered_map<std::string, RateBucket>& rate_buckets,
                              const std::string& client_key, const RateLimitConfig& config);

}  // namespace edgeshield
