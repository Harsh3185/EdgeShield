#include "edgeshield/backend/backend_client.hpp"

namespace edgeshield {
using namespace std;

BackendClient::BackendClient(asio::io_context& ioc, BackendConnectionPool& pool, const TimeoutConfig& timeouts)
    : ioc_(ioc), resolver_(ioc), pool_(pool), timeouts_(timeouts) {}

Response BackendClient::send(const BackendConfig& backend, Request request) {
  strip_hop_by_hop(request);
  request.set(http::field::host, backend.host + ":" + to_string(backend.port));
  request.keep_alive(true);
  request.prepare_payload();

  auto connection = pool_.acquire(ioc_, resolver_, backend, timeouts_);
  beast::error_code ec;
  connection->stream.expires_after(timeouts_.backend_response);
  http::write(connection->stream, request, ec);
  if (ec) {
    pool_.close(move(connection));
    throw beast::system_error(ec);
  }

  beast::flat_buffer buffer;
  Response response;
  http::read(connection->stream, buffer, response, ec);
  if (ec) {
    pool_.close(move(connection));
    throw beast::system_error(ec);
  }
  response.set(http::field::server, "EdgeShield");
  response.set("X-EdgeShield-Cache", "MISS");
  response.prepare_payload();
  if (response.keep_alive()) {
    pool_.release(backend, move(connection));
  } else {
    pool_.close(move(connection));
  }
  return response;
}

}  // namespace edgeshield
