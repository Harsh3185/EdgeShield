#include "edgeshield/http/proxy_handler.hpp"

#include "edgeshield/backend/backend_client.hpp"
#include "edgeshield/http/http_helpers.hpp"

#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#if EDGESHIELD_WITH_TLS
#include <boost/beast/ssl.hpp>
#endif

#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <type_traits>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
#if EDGESHIELD_WITH_TLS
namespace ssl = asio::ssl;
#endif

namespace edgeshield {
using namespace std;
namespace {

using PlainStream = beast::tcp_stream;
#if EDGESHIELD_WITH_TLS
using TlsStream = beast::ssl_stream<beast::tcp_stream>;
#endif

unique_ptr<websocket::stream<beast::tcp_stream>> connect_backend_websocket(
    asio::io_context& ioc, const BackendConfig& backend, const TimeoutConfig& timeouts, const string& target,
    const string& request_id, const string& client_ip) {
  tcp::resolver resolver{ioc};
  auto backend_ws = make_unique<websocket::stream<beast::tcp_stream>>(ioc);
  beast::error_code ec;
  beast::get_lowest_layer(*backend_ws).expires_after(timeouts.connect);
  const auto endpoints = resolver.resolve(backend.host, to_string(backend.port), ec);
  if (ec) {
    throw beast::system_error(ec);
  }
  beast::get_lowest_layer(*backend_ws).connect(endpoints, ec);
  if (ec) {
    throw beast::system_error(ec);
  }

  backend_ws->set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));
  backend_ws->set_option(websocket::stream_base::decorator(
      [request_id, client_ip](websocket::request_type& request) {
        request.set("X-Forwarded-For", client_ip);
        request.set("X-Request-ID", request_id);
        request.set("X-EdgeShield-Proxy", "EdgeShield");
      }));

  beast::get_lowest_layer(*backend_ws).expires_after(timeouts.backend_response);
  backend_ws->handshake(backend.host + ":" + to_string(backend.port), target, ec);
  if (ec) {
    throw beast::system_error(ec);
  }
  beast::get_lowest_layer(*backend_ws).expires_never();
  return backend_ws;
}

template <typename Stream>
class WebSocketTunnel : public enable_shared_from_this<WebSocketTunnel<Stream>> {
 public:
  WebSocketTunnel(Stream stream, unique_ptr<websocket::stream<beast::tcp_stream>> backend_ws,
                  GatewayState& state, Request request, ProxyLog log,
                  chrono::steady_clock::time_point started)
      : client_ws_(move(stream)),
        backend_ws_(move(backend_ws)),
        state_(state),
        request_(move(request)),
        log_(move(log)),
        started_(started) {}

  void run() {
    client_ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
    auto self = this->shared_from_this();
    client_ws_.async_accept(request_, [self](beast::error_code ec) {
      if (ec) {
        self->finish(400, ec.message());
        return;
      }
      self->log_.status = 101;
      self->read_client();
      self->read_backend();
    });
  }

 private:
  void read_client() {
    auto self = this->shared_from_this();
    client_ws_.async_read(client_buffer_, [self](beast::error_code ec, size_t bytes) {
      if (ec) {
        self->finish_from_read(ec);
        return;
      }
      self->bytes_in_ += bytes;
      self->backend_ws_->binary(self->client_ws_.got_binary());
      self->write_backend();
    });
  }

  void write_backend() {
    auto self = this->shared_from_this();
    backend_ws_->async_write(client_buffer_.data(), [self](beast::error_code ec, size_t) {
      self->client_buffer_.consume(self->client_buffer_.size());
      if (ec) {
        self->finish(502, ec.message());
        return;
      }
      self->read_client();
    });
  }

  void read_backend() {
    auto self = this->shared_from_this();
    backend_ws_->async_read(backend_buffer_, [self](beast::error_code ec, size_t bytes) {
      if (ec) {
        self->finish_from_read(ec);
        return;
      }
      self->bytes_out_ += bytes;
      self->client_ws_.binary(self->backend_ws_->got_binary());
      self->write_client();
    });
  }

  void write_client() {
    auto self = this->shared_from_this();
    client_ws_.async_write(backend_buffer_.data(), [self](beast::error_code ec, size_t) {
      self->backend_buffer_.consume(self->backend_buffer_.size());
      if (ec) {
        self->finish(499, ec.message());
        return;
      }
      self->read_backend();
    });
  }

  void finish_from_read(const beast::error_code& ec) {
    if (ec == websocket::error::closed || ec == asio::error::operation_aborted) {
      finish(101, "");
      return;
    }
    finish(502, ec.message());
  }

  void finish(unsigned status, const string& error) {
    if (closed_) {
      return;
    }
    closed_ = true;
    if (log_.status == 0) {
      log_.status = status;
    }
    if (!error.empty()) {
      log_.error = error;
    }

    beast::error_code ignored;
    beast::get_lowest_layer(client_ws_).expires_after(chrono::seconds(2));
    beast::get_lowest_layer(client_ws_).socket().shutdown(tcp::socket::shutdown_both, ignored);
    beast::get_lowest_layer(client_ws_).socket().close(ignored);
    if (backend_ws_) {
      beast::get_lowest_layer(*backend_ws_).socket().shutdown(tcp::socket::shutdown_both, ignored);
      beast::get_lowest_layer(*backend_ws_).socket().close(ignored);
    }

    const auto elapsed =
        chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - started_);
    log_.latency_ms = elapsed.count();
    state_.finish_request(elapsed.count(), bytes_in_, bytes_out_);
    state_.add_log(log_);
    cout << log_.request_id << " " << log_.method << " " << log_.target << " -> " << log_.backend_id << " "
         << log_.status << " " << elapsed.count() << "ms " << log_.error << "\n";
  }

  websocket::stream<Stream> client_ws_;
  unique_ptr<websocket::stream<beast::tcp_stream>> backend_ws_;
  GatewayState& state_;
  Request request_;
  ProxyLog log_;
  chrono::steady_clock::time_point started_;
  beast::flat_buffer client_buffer_;
  beast::flat_buffer backend_buffer_;
  uint64_t bytes_in_ = 0;
  uint64_t bytes_out_ = 0;
  bool closed_ = false;
};

template <typename Stream>
class BasicProxySession : public enable_shared_from_this<BasicProxySession<Stream>> {
 public:
  template <typename T = Stream, typename = enable_if_t<is_constructible_v<T, tcp::socket&&>>>
  BasicProxySession(tcp::socket socket, GatewayState& state, BackendConnectionPool& backend_pool,
                    asio::io_context& ioc)
      : stream_(move(socket)), state_(state), backend_pool_(backend_pool), ioc_(ioc) {}

#if EDGESHIELD_WITH_TLS
  template <typename T = Stream,
            typename = enable_if_t<is_constructible_v<T, tcp::socket&&, ssl::context&>>, typename = void>
  BasicProxySession(tcp::socket socket, ssl::context& ssl_context, GatewayState& state,
                    BackendConnectionPool& backend_pool, asio::io_context& ioc)
      : stream_(move(socket), ssl_context), state_(state), backend_pool_(backend_pool), ioc_(ioc) {}
#endif

  void run() {
#if EDGESHIELD_WITH_TLS
    if constexpr (is_same_v<Stream, TlsStream>) {
      auto self = this->shared_from_this();
      beast::get_lowest_layer(stream_).expires_after(chrono::seconds(10));
      stream_.async_handshake(ssl::stream_base::server, [self](beast::error_code ec) {
        if (!ec) {
          self->read_request();
        }
      });
    } else {
      read_request();
    }
#else
    read_request();
#endif
  }

 private:
  void read_request() {
    auto self = this->shared_from_this();
    http::async_read(stream_, buffer_, request_, [self](beast::error_code ec, size_t) {
      if (ec) {
        return;
      }
      self->handle_request();
    });
  }

  void handle_request() {
    const auto started = chrono::steady_clock::now();
    const auto request_id = generate_request_id();
    const auto method = method_string(request_);
    const auto target = target_string(request_);
    const auto client_endpoint = remote_address();
    ProxyLog log{request_id, method, target, "", 0, 0, ""};
    state_.start_request();

    Response response;
    try {
      if (websocket::is_upgrade(request_)) {
        if (auto websocket_response = handle_websocket_request(started, request_id, client_endpoint, log)) {
          response = move(*websocket_response);
        } else {
          return;
        }
      } else {
        const auto* route = state_.match_route(target);
        if (!route) {
          response = text_response(http::status::not_found, request_.version(), "No EdgeShield route matched\n");
        } else if (route->rate_limit.enabled &&
                   !state_.check_rate_limit(client_endpoint + ":" + route->prefix, route->rate_limit).allowed) {
          state_.increment_rate_limited();
          response = text_response(http::status::too_many_requests, request_.version(), "Rate limit exceeded\n");
        } else {
          response = handle_proxy_request(*route, request_id, client_endpoint, log);
        }
      }
    } catch (const exception& ex) {
      log.error = ex.what();
      response = text_response(http::status::bad_gateway, request_.version(), "Backend request failed\n");
    }

    const auto elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - started);
    log.status = response.result_int();
    log.latency_ms = elapsed.count();
    state_.finish_request(elapsed.count(), request_.body().size(), response.body().size());
    state_.add_log(log);
    cout << request_id << " " << method << " " << target << " -> " << log.backend_id << " "
         << response.result_int() << " " << elapsed.count() << "ms " << log.error << "\n";

    write_response(move(response));
  }

  optional<Response> handle_websocket_request(chrono::steady_clock::time_point started,
                                              const string& request_id, const string& client_ip,
                                              ProxyLog& log) {
    const auto target = target_string(request_);
    const auto* route = state_.match_route(target);
    if (!route) {
      return text_response(http::status::not_found, request_.version(), "No EdgeShield route matched\n");
    }

    if (route->rate_limit.enabled &&
        !state_.check_rate_limit(client_ip + ":" + route->prefix, route->rate_limit).allowed) {
      state_.increment_rate_limited();
      return text_response(http::status::too_many_requests, request_.version(), "Rate limit exceeded\n");
    }

    auto backend = state_.choose_backend(*route);
    if (!backend) {
      state_.increment_backend_error();
      return text_response(http::status::bad_gateway, request_.version(), "No healthy backend available\n");
    }

    log.backend_id = backend->id;
    try {
      auto backend_ws =
          connect_backend_websocket(ioc_, *backend, state_.timeouts(), target, request_id, client_ip);
      state_.record_backend_result(backend->id, true, route->circuit_breaker);
      auto tunnel = make_shared<WebSocketTunnel<Stream>>(move(stream_), move(backend_ws), state_,
                                                         move(request_), log, started);
      tunnel->run();
      return nullopt;
    } catch (const beast::system_error& ex) {
      log.error = ex.code().message();
      state_.increment_backend_error();
      state_.record_backend_result(backend->id, false, route->circuit_breaker);
      return text_response(http::status::bad_gateway, request_.version(), "WebSocket backend failed\n");
    } catch (const exception& ex) {
      log.error = ex.what();
      state_.increment_backend_error();
      state_.record_backend_result(backend->id, false, route->circuit_breaker);
      return text_response(http::status::bad_gateway, request_.version(), "WebSocket backend failed\n");
    }
  }

  Response handle_proxy_request(const RouteConfig& route, const string& request_id, const string& client_ip,
                                ProxyLog& log) {
    const auto method = method_string(request_);
    const auto target = target_string(request_);
    const auto key = state_.cache_key(method, target);
    if (route.cache.enabled && method_allowed(route.cache.methods, method)) {
      if (auto cached = state_.get_cached(key)) {
        state_.increment_cache_hit();
        return cached_to_response(*cached, request_.version());
      }
      state_.increment_cache_miss();
    }

    const auto attempts =
        route.retry.enabled && method_allowed(route.retry.methods, method) ? max(1, route.retry.attempts) : 1;
    string last_error;
    bool saw_timeout = false;
    for (int attempt = 0; attempt < attempts; ++attempt) {
      auto backend = state_.choose_backend(route);
      if (!backend) {
        state_.increment_backend_error();
        return text_response(http::status::bad_gateway, request_.version(), "No healthy backend available\n");
      }
      log.backend_id = backend->id;
      try {
        auto outbound = request_;
        outbound.set("X-Forwarded-For", client_ip);
        outbound.set("X-Request-ID", request_id);
        outbound.set("X-EdgeShield-Proxy", "EdgeShield");
        BackendClient client(ioc_, backend_pool_, state_.timeouts());
        auto response = client.send(*backend, move(outbound));
        state_.record_backend_result(backend->id, true, route.circuit_breaker);
        if (route.cache.enabled && method_allowed(route.cache.methods, method)) {
          if (auto cached = response_to_cache(response, route)) {
            state_.put_cached(key, *cached, route.cache.max_entries);
          }
        }
        return response;
      } catch (const beast::system_error& ex) {
        last_error = ex.code().message();
        if (ex.code() == asio::error::timed_out || ex.code() == beast::error::timeout) {
          saw_timeout = true;
          state_.increment_timeout();
        } else {
          state_.increment_backend_error();
        }
        state_.record_backend_result(backend->id, false, route.circuit_breaker);
      } catch (const exception& ex) {
        last_error = ex.what();
        state_.increment_backend_error();
        state_.record_backend_result(backend->id, false, route.circuit_breaker);
      }
    }

    log.error = last_error;
    return text_response(saw_timeout ? http::status::gateway_timeout : http::status::bad_gateway, request_.version(),
                         "Backend request failed: " + last_error + "\n");
  }

  void write_response(Response response) {
    auto shared_response = make_shared<Response>(move(response));
    auto self = this->shared_from_this();
    http::async_write(stream_, *shared_response, [self, shared_response](beast::error_code, size_t) {
#if EDGESHIELD_WITH_TLS
      if constexpr (is_same_v<Stream, TlsStream>) {
        beast::get_lowest_layer(self->stream_).expires_after(chrono::seconds(10));
        self->stream_.async_shutdown([self, shared_response](beast::error_code) {});
      } else {
        beast::error_code ignored;
        self->stream_.socket().shutdown(tcp::socket::shutdown_send, ignored);
      }
#else
      beast::error_code ignored;
      self->stream_.socket().shutdown(tcp::socket::shutdown_send, ignored);
#endif
    });
  }

  string remote_address() const {
    beast::error_code ec;
#if EDGESHIELD_WITH_TLS
    if constexpr (is_same_v<Stream, TlsStream>) {
      const auto endpoint = beast::get_lowest_layer(stream_).socket().remote_endpoint(ec);
      return ec ? "" : endpoint.address().to_string();
    } else {
      const auto endpoint = stream_.socket().remote_endpoint(ec);
      return ec ? "" : endpoint.address().to_string();
    }
#else
    const auto endpoint = stream_.socket().remote_endpoint(ec);
    return ec ? "" : endpoint.address().to_string();
#endif
  }

  Stream stream_;
  GatewayState& state_;
  BackendConnectionPool& backend_pool_;
  asio::io_context& ioc_;
  beast::flat_buffer buffer_;
  Request request_;
};

using ProxySession = BasicProxySession<PlainStream>;
#if EDGESHIELD_WITH_TLS
using HttpsProxySession = BasicProxySession<TlsStream>;
#endif

class MetricsSession : public enable_shared_from_this<MetricsSession> {
 public:
  MetricsSession(tcp::socket socket, GatewayState& state) : socket_(move(socket)), state_(state) {}
  void run() { read_request(); }

 private:
  void read_request() {
    auto self = shared_from_this();
    http::async_read(socket_, buffer_, request_, [self](beast::error_code ec, size_t) {
      if (!ec) {
        self->handle_request();
      }
    });
  }

  void handle_request() {
    const auto method = method_string(request_);
    const auto target = target_string(request_);
    Response response;
    if (method == "OPTIONS") {
      response = text_response(http::status::no_content, request_.version(), "");
    } else if (target == "/metrics") {
      response = text_response(http::status::ok, request_.version(), state_.prometheus_text());
      response.set(http::field::content_type, "text/plain; version=0.0.4");
    } else {
      response = text_response(http::status::not_found, request_.version(), "Unknown metrics endpoint\n");
    }
    response.set(http::field::access_control_allow_origin, "*");
    response.set(http::field::access_control_allow_methods, "GET,OPTIONS");
    response.set(http::field::access_control_allow_headers, "content-type");
    write_response(move(response));
  }

  void write_response(Response response) {
    auto shared_response = make_shared<Response>(move(response));
    auto self = shared_from_this();
    http::async_write(socket_, *shared_response, [self, shared_response](beast::error_code, size_t) {
      beast::error_code ignored;
      self->socket_.shutdown(tcp::socket::shutdown_send, ignored);
    });
  }

  tcp::socket socket_;
  GatewayState& state_;
  beast::flat_buffer buffer_;
  Request request_;
};

class HealthChecker : public enable_shared_from_this<HealthChecker> {
 public:
  HealthChecker(asio::io_context& ioc, GatewayState& state, BackendConnectionPool& backend_pool)
      : ioc_(ioc), state_(state), backend_pool_(backend_pool), timer_(ioc), resolver_(ioc) {}

  void run() { schedule(); }

 private:
  void schedule() {
    timer_.expires_after(chrono::seconds(3));
    auto self = shared_from_this();
    timer_.async_wait([self](beast::error_code ec) {
      if (!ec) {
        self->check_all();
        self->schedule();
      }
    });
  }

  void check_all() {
    CircuitBreakerConfig breaker;
    const auto config = state_.config_copy();
    if (!config.routes.empty()) {
      breaker = config.routes.front().circuit_breaker;
    }

    for (const auto& backend : config.backends) {
      bool ok = false;
      try {
        BackendClient client(ioc_, backend_pool_, state_.timeouts());
        Request request{http::verb::get, backend.health_path, 11};
        request.set(http::field::host, backend.host + ":" + to_string(backend.port));
        request.set(http::field::user_agent, "EdgeShield health-check");
        request.prepare_payload();
        auto response = client.send(backend, move(request));
        ok = response.result_int() >= 200 && response.result_int() < 300;
      } catch (...) {
        ok = false;
      }
      state_.record_backend_result(backend.id, ok, breaker);
    }
  }

  asio::io_context& ioc_;
  GatewayState& state_;
  BackendConnectionPool& backend_pool_;
  asio::steady_timer timer_;
  tcp::resolver resolver_;
};

template <typename Session>
class Listener : public enable_shared_from_this<Listener<Session>> {
 public:
  Listener(asio::io_context& ioc, tcp::endpoint endpoint, GatewayState& state,
           BackendConnectionPool* backend_pool = nullptr)
      : ioc_(ioc), acceptor_(ioc), state_(state), backend_pool_(backend_pool) {
    beast::error_code ec;
    acceptor_.open(endpoint.protocol(), ec);
    if (ec) throw beast::system_error(ec);
    acceptor_.set_option(asio::socket_base::reuse_address(true), ec);
    if (ec) throw beast::system_error(ec);
    acceptor_.bind(endpoint, ec);
    if (ec) throw beast::system_error(ec);
    acceptor_.listen(asio::socket_base::max_listen_connections, ec);
    if (ec) throw beast::system_error(ec);
  }

  void run() { accept(); }

 private:
  void accept() {
    auto self = this->shared_from_this();
    acceptor_.async_accept(asio::make_strand(ioc_), [self](beast::error_code ec, tcp::socket socket) {
      if (!ec) {
        if constexpr (is_same_v<Session, ProxySession>) {
          make_shared<ProxySession>(move(socket), self->state_, *self->backend_pool_, self->ioc_)->run();
        } else {
          make_shared<MetricsSession>(move(socket), self->state_)->run();
        }
      }
      self->accept();
    });
  }

  asio::io_context& ioc_;
  tcp::acceptor acceptor_;
  GatewayState& state_;
  BackendConnectionPool* backend_pool_;
};

#if EDGESHIELD_WITH_TLS
class HttpsListener : public enable_shared_from_this<HttpsListener> {
 public:
  HttpsListener(asio::io_context& ioc, tcp::endpoint endpoint, ssl::context& ssl_context, GatewayState& state,
                BackendConnectionPool& backend_pool)
      : ioc_(ioc), acceptor_(ioc), ssl_context_(ssl_context), state_(state), backend_pool_(backend_pool) {
    beast::error_code ec;
    acceptor_.open(endpoint.protocol(), ec);
    if (ec) throw beast::system_error(ec);
    acceptor_.set_option(asio::socket_base::reuse_address(true), ec);
    if (ec) throw beast::system_error(ec);
    acceptor_.bind(endpoint, ec);
    if (ec) throw beast::system_error(ec);
    acceptor_.listen(asio::socket_base::max_listen_connections, ec);
    if (ec) throw beast::system_error(ec);
  }

  void run() { accept(); }

 private:
  void accept() {
    auto self = shared_from_this();
    acceptor_.async_accept(asio::make_strand(ioc_), [self](beast::error_code ec, tcp::socket socket) {
      if (!ec) {
        make_shared<HttpsProxySession>(move(socket), self->ssl_context_, self->state_, self->backend_pool_,
                                       self->ioc_)
            ->run();
      }
      self->accept();
    });
  }

  asio::io_context& ioc_;
  tcp::acceptor acceptor_;
  ssl::context& ssl_context_;
  GatewayState& state_;
  BackendConnectionPool& backend_pool_;
};
#endif

}  // namespace

tcp::endpoint endpoint_from(const string& host, uint16_t port) {
  return {asio::ip::make_address(host), port};
}

void run_proxy_listener(asio::io_context& ioc, tcp::endpoint endpoint, GatewayState& state,
                        BackendConnectionPool& backend_pool) {
  make_shared<Listener<ProxySession>>(ioc, endpoint, state, &backend_pool)->run();
}

void run_metrics_listener(asio::io_context& ioc, tcp::endpoint endpoint, GatewayState& state) {
  make_shared<Listener<MetricsSession>>(ioc, endpoint, state)->run();
}

void run_health_checker(asio::io_context& ioc, GatewayState& state, BackendConnectionPool& backend_pool) {
  make_shared<HealthChecker>(ioc, state, backend_pool)->run();
}

#if EDGESHIELD_WITH_TLS
void run_https_listener(asio::io_context& ioc, tcp::endpoint endpoint, ssl::context& ssl_context,
                        GatewayState& state, BackendConnectionPool& backend_pool) {
  make_shared<HttpsListener>(ioc, endpoint, ssl_context, state, backend_pool)->run();
}
#endif

}  // namespace edgeshield
