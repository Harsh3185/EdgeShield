#include "edgeshield/backend/connection_pool.hpp"
#include "edgeshield/config/config.hpp"
#include "edgeshield/http/proxy_handler.hpp"
#include "edgeshield/state/gateway_state.hpp"

#include <boost/asio.hpp>
#if EDGESHIELD_WITH_TLS
#include <boost/asio/ssl.hpp>
#endif

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace asio = boost::asio;
#if EDGESHIELD_WITH_TLS
namespace ssl = asio::ssl;
#endif
using namespace std;

int main(int argc, char* argv[]) {
  const string config_path = argc > 1 ? argv[1] : "config/edgeshield.json";

  try {
    auto config = edgeshield::load_config(config_path);
    edgeshield::GatewayState state(config);
    asio::io_context ioc(static_cast<int>(config.proxy.worker_threads));
    edgeshield::BackendConnectionPool backend_pool;

    edgeshield::run_proxy_listener(
        ioc, edgeshield::endpoint_from(config.proxy.host, config.proxy.port), state, backend_pool);
    edgeshield::run_metrics_listener(
        ioc, edgeshield::endpoint_from(config.proxy.host, config.proxy.metrics_port), state);
#if EDGESHIELD_WITH_TLS
    optional<ssl::context> ssl_context;
    if (config.proxy.https_enabled) {
      ssl_context.emplace(ssl::context::tlsv12_server);
      ssl_context->set_options(ssl::context::default_workarounds | ssl::context::no_sslv2 |
                               ssl::context::single_dh_use);
      ssl_context->use_certificate_chain_file(config.proxy.tls_cert_file);
      ssl_context->use_private_key_file(config.proxy.tls_key_file, ssl::context::pem);
      edgeshield::run_https_listener(
          ioc, edgeshield::endpoint_from(config.proxy.host, config.proxy.https_port), *ssl_context, state,
          backend_pool);
    }
#else
    if (config.proxy.https_enabled) {
      throw runtime_error("This EdgeShield build does not include OpenSSL/TLS support");
    }
#endif
    edgeshield::run_health_checker(ioc, state, backend_pool);

    cout << "EdgeShield proxy listening on " << config.proxy.host << ":" << config.proxy.port << "\n";
#if EDGESHIELD_WITH_TLS
    if (config.proxy.https_enabled) {
      cout << "EdgeShield HTTPS proxy listening on " << config.proxy.host << ":" << config.proxy.https_port
           << "\n";
    }
#endif
    cout << "EdgeShield metrics listening on " << config.proxy.host << ":" << config.proxy.metrics_port << "\n";

    vector<thread> workers;
    const auto count = max<size_t>(1, config.proxy.worker_threads);
    workers.reserve(count);
    for (size_t i = 0; i < count; ++i) {
      workers.emplace_back([&ioc] { ioc.run(); });
    }
    for (auto& worker : workers) {
      worker.join();
    }
  } catch (const exception& ex) {
    cerr << "EdgeShield failed: " << ex.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
