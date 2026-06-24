#include "edgeshield/http/http_helpers.hpp"

namespace edgeshield {
using namespace std;

Response text_response(http::status status, unsigned version, string body) {
  Response response{status, version};
  response.set(http::field::server, "EdgeShield");
  response.set(http::field::content_type, "text/plain");
  response.body() = move(body);
  response.prepare_payload();
  return response;
}

Response json_response(unsigned version, const nlohmann::json& body) {
  Response response{http::status::ok, version};
  response.set(http::field::server, "EdgeShield");
  response.set(http::field::content_type, "application/json");
  response.set(http::field::access_control_allow_origin, "*");
  response.body() = body.dump(2);
  response.prepare_payload();
  return response;
}

string method_string(const Request& request) {
  return string(request.method_string());
}

string target_string(const Request& request) {
  return string(request.target());
}

void strip_hop_by_hop(Request& request) {
  request.erase(http::field::connection);
  request.erase(http::field::keep_alive);
  request.erase(http::field::proxy_authenticate);
  request.erase(http::field::proxy_authorization);
  request.erase(http::field::te);
  request.erase(http::field::trailer);
  request.erase(http::field::transfer_encoding);
  request.erase(http::field::upgrade);
}

optional<CachedResponse> response_to_cache(const Response& response, const RouteConfig& route) {
  if (response.result_int() < 200 || response.result_int() >= 300) {
    return nullopt;
  }
  CachedResponse cached;
  cached.status = response.result_int();
  cached.body = response.body();
  cached.expires_at = chrono::steady_clock::now() + chrono::seconds(route.cache.ttl_seconds);
  for (const auto& header : response.base()) {
    cached.headers.emplace(string(header.name_string()), string(header.value()));
  }
  return cached;
}

Response cached_to_response(const CachedResponse& cached, unsigned version) {
  Response response{static_cast<http::status>(cached.status), version};
  for (const auto& [key, value] : cached.headers) {
    response.set(key, value);
  }
  response.set("X-EdgeShield-Cache", "HIT");
  response.body() = cached.body;
  response.prepare_payload();
  return response;
}

}  // namespace edgeshield
