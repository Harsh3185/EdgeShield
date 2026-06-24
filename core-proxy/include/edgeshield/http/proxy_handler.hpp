#pragma once

#include "edgeshield/backend/connection_pool.hpp"
#include "edgeshield/state/gateway_state.hpp"

#include <boost/asio.hpp>
#include <memory>
#include <string>

#if EDGESHIELD_WITH_TLS
#include <boost/asio/ssl.hpp>
#endif

namespace edgeshield {

tcp::endpoint endpoint_from(const std::string& host, uint16_t port);
void run_proxy_listener(asio::io_context& ioc, tcp::endpoint endpoint, GatewayState& state,
                        BackendConnectionPool& backend_pool);
void run_metrics_listener(asio::io_context& ioc, tcp::endpoint endpoint, GatewayState& state);
void run_health_checker(asio::io_context& ioc, GatewayState& state, BackendConnectionPool& backend_pool);

#if EDGESHIELD_WITH_TLS
namespace ssl = asio::ssl;
void run_https_listener(asio::io_context& ioc, tcp::endpoint endpoint, ssl::context& ssl_context,
                        GatewayState& state, BackendConnectionPool& backend_pool);
#endif

}  // namespace edgeshield
