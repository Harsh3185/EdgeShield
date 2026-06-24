#pragma once

#include "edgeshield/backend/connection_pool.hpp"
#include "edgeshield/http/http_helpers.hpp"

namespace edgeshield {

class BackendClient {
 public:
  BackendClient(asio::io_context& ioc, BackendConnectionPool& pool, const TimeoutConfig& timeouts);
  Response send(const BackendConfig& backend, Request request);

 private:
  asio::io_context& ioc_;
  tcp::resolver resolver_;
  BackendConnectionPool& pool_;
  TimeoutConfig timeouts_;
};

}  // namespace edgeshield
