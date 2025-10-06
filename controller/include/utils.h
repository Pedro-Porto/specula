#include <memory>
#include <string>
#include <vector>

#include "../../core/include/connection.h"
#include "thread_safe_vector.h"

void broadcast(const std::string& cmd, const std::string& payload,
               ThreadSafeVector<Connection>& conns);