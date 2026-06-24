#pragma once

#include "edgeshield/config/config.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <unordered_map>

namespace edgeshield {

struct CachedResponse {
  unsigned status = 200;
  std::string body;
  std::unordered_map<std::string, std::string> headers;
  std::chrono::steady_clock::time_point expires_at;
};

std::string cache_key(const std::string& method, const std::string& target);
std::optional<CachedResponse> get_cached(std::unordered_map<std::string, CachedResponse>& cache,
                                         const std::string& key);
void put_cached(std::unordered_map<std::string, CachedResponse>& cache, const std::string& key,
                CachedResponse response, std::size_t max_entries);

}  // namespace edgeshield
