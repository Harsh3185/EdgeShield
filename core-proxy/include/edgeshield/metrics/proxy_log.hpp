#pragma once

#include <deque>
#include <mutex>
#include <string>
#include <vector>

namespace edgeshield {

struct ProxyLog {
  std::string request_id;
  std::string method;
  std::string target;
  std::string backend_id;
  unsigned status = 0;
  long latency_ms = 0;
  std::string error;
};

class ProxyLogStore {
 public:
  void reset();
  void add(ProxyLog log);
  std::vector<ProxyLog> snapshot() const;

 private:
  mutable std::mutex mutex_;
  std::deque<ProxyLog> recent_logs_;
};

}  // namespace edgeshield
