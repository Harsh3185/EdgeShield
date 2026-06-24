#include "edgeshield/cache/response_cache.hpp"

namespace edgeshield {
using namespace std;

string cache_key(const string& method, const string& target) {
  return method + " " + target;
}

optional<CachedResponse> get_cached(unordered_map<string, CachedResponse>& cache, const string& key) {
  auto found = cache.find(key);
  if (found == cache.end()) {
    return nullopt;
  }
  if (found->second.expires_at <= chrono::steady_clock::now()) {
    cache.erase(found);
    return nullopt;
  }
  return found->second;
}

void put_cached(unordered_map<string, CachedResponse>& cache, const string& key, CachedResponse response,
                size_t max_entries) {
  if (cache.size() >= max_entries && !cache.contains(key)) {
    cache.erase(cache.begin());
  }
  cache[key] = move(response);
}

}  // namespace edgeshield
