#include "edgeshield/metrics/proxy_log.hpp"

namespace edgeshield {
using namespace std;
namespace {

constexpr size_t kMaxLogs = 250;

}

void ProxyLogStore::reset() {
  lock_guard lock(mutex_);
  recent_logs_.clear();
}

void ProxyLogStore::add(ProxyLog log) {
  lock_guard lock(mutex_);
  recent_logs_.push_front(move(log));
  while (recent_logs_.size() > kMaxLogs) {
    recent_logs_.pop_back();
  }
}

vector<ProxyLog> ProxyLogStore::snapshot() const {
  lock_guard lock(mutex_);
  return vector<ProxyLog>(recent_logs_.begin(), recent_logs_.end());
}

}  // namespace edgeshield
