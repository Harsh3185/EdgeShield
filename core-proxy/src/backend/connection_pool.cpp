#include "edgeshield/backend/connection_pool.hpp"

namespace edgeshield {
using namespace std;

BackendConnectionPool::PooledConnection::PooledConnection(asio::io_context& ioc) : stream(ioc) {}

BackendConnectionPool::Connection BackendConnectionPool::acquire(asio::io_context& ioc, tcp::resolver& resolver,
                                                                 const BackendConfig& backend,
                                                                 const TimeoutConfig& timeouts) {
  const auto key = pool_key(backend);
  const auto now = chrono::steady_clock::now();
  {
    lock_guard lock(mutex_);
    auto& bucket = idle_[key];
    while (!bucket.empty()) {
      auto connection = move(bucket.back());
      bucket.pop_back();
      if (connection && connection->stream.socket().is_open() && now - connection->idle_since <= max_idle_age_) {
        return connection;
      }
      close(move(connection));
    }
  }

  auto connection = make_unique<PooledConnection>(ioc);
  beast::error_code ec;
  connection->stream.expires_after(timeouts.connect);
  const auto endpoints = resolver.resolve(backend.host, to_string(backend.port), ec);
  if (ec) {
    throw beast::system_error(ec);
  }
  connection->stream.connect(endpoints, ec);
  if (ec) {
    throw beast::system_error(ec);
  }
  return connection;
}

void BackendConnectionPool::release(const BackendConfig& backend, Connection connection) {
  if (!connection || !connection->stream.socket().is_open()) {
    close(move(connection));
    return;
  }

  connection->idle_since = chrono::steady_clock::now();
  lock_guard lock(mutex_);
  auto& bucket = idle_[pool_key(backend)];
  if (bucket.size() >= max_idle_per_backend_) {
    close(move(connection));
    return;
  }
  bucket.push_back(move(connection));
}

void BackendConnectionPool::close(Connection connection) {
  if (!connection) {
    return;
  }
  beast::error_code ignored;
  if (connection->stream.socket().is_open()) {
    connection->stream.socket().shutdown(tcp::socket::shutdown_both, ignored);
    connection->stream.socket().close(ignored);
  }
}

string BackendConnectionPool::pool_key(const BackendConfig& backend) {
  return backend.id + "|" + backend.host + ":" + to_string(backend.port);
}

}  // namespace edgeshield
