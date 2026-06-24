#pragma once

#include "edgeshield/cache/response_cache.hpp"
#include "edgeshield/config/config.hpp"

#include <boost/beast/http.hpp>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace edgeshield {

namespace http = boost::beast::http;

using Request = http::request<http::string_body>;
using Response = http::response<http::string_body>;

Response text_response(http::status status, unsigned version, std::string body);
Response json_response(unsigned version, const nlohmann::json& body);
std::string method_string(const Request& request);
std::string target_string(const Request& request);
void strip_hop_by_hop(Request& request);
std::optional<CachedResponse> response_to_cache(const Response& response, const RouteConfig& route);
Response cached_to_response(const CachedResponse& cached, unsigned version);

}  // namespace edgeshield
