#include "edgeshield/ratelimit/rate_limiter.hpp"

#include <boost/asio.hpp>

#include <algorithm>

namespace edgeshield {
using namespace std;

optional<int> redis_incr_with_expire(const RedisConfig& redis, const string& key, int window_seconds) {
  boost::asio::ip::tcp::iostream stream;
  stream.expires_after(chrono::milliseconds(500));
  stream.connect(redis.host, to_string(redis.port));
  if (!stream) {
    return nullopt;
  }

  stream << "*2\r\n$4\r\nINCR\r\n$" << key.size() << "\r\n" << key << "\r\n" << flush;
  string line;
  if (!getline(stream, line) || line.empty() || line[0] != ':') {
    return nullopt;
  }

  const auto count = stoi(line.substr(1));
  if (count == 1) {
    stream << "*3\r\n$6\r\nEXPIRE\r\n$" << key.size() << "\r\n" << key << "\r\n$"
           << to_string(window_seconds).size() << "\r\n" << window_seconds << "\r\n" << flush;
    getline(stream, line);
  }
  return count;
}

RateDecision check_rate_limit(const RedisConfig& redis, unordered_map<string, RateBucket>& rate_buckets,
                              const string& client_key, const RateLimitConfig& config) {
  if (redis.enabled) {
    const auto key = redis.key_prefix + ":" + client_key;
    if (const auto count = redis_incr_with_expire(redis, key, config.window_seconds)) {
      return RateDecision{*count <= config.requests, max(0, config.requests - *count)};
    }
  }

  const auto now = chrono::steady_clock::now();
  auto& bucket = rate_buckets[client_key];
  if (bucket.window_start.time_since_epoch().count() == 0 ||
      now - bucket.window_start >= chrono::seconds(config.window_seconds)) {
    bucket.window_start = now;
    bucket.count = 0;
  }
  ++bucket.count;
  const auto remaining = max(0, config.requests - bucket.count);
  return RateDecision{bucket.count <= config.requests, remaining};
}

}  // namespace edgeshield
