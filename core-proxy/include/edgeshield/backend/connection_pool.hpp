#pragma once

#include "edgeshield/config/config.hpp"

#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace edgeshield {

namespace asio = boost::asio;
namespace beast = boost::beast;
using tcp = asio::ip::tcp;

class BackendConnectionPool {
 public:
  struct PooledConnection {
    explicit PooledConnection(asio::io_context& ioc);
    beast::tcp_stream stream;
    std::chrono::steady_clock::time_point idle_since{};
  };
  using Connection = std::unique_ptr<PooledConnection>;

  Connection acquire(asio::io_context& ioc, tcp::resolver& resolver, const BackendConfig& backend,
                     const TimeoutConfig& timeouts);
  void release(const BackendConfig& backend, Connection connection);
  void close(Connection connection);

 private:
  static std::string pool_key(const BackendConfig& backend);

  std::mutex mutex_;
  std::unordered_map<std::string, std::vector<Connection>> idle_;
  std::size_t max_idle_per_backend_ = 16;
  std::chrono::seconds max_idle_age_{3};
};

}  // namespace edgeshield
