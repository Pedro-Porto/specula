#include "../include/utils.h"

std::unordered_map<std::string, std::string> parse_kv(const std::string& s) {
    std::unordered_map<std::string, std::string> kv;
    std::stringstream ss(s);
    std::string token;
    while (ss >> token) {
        auto eq = token.find('=');
        if (eq == std::string::npos) continue;
        std::string key = token.substr(0, eq);
        std::string val = token.substr(eq + 1);
        kv[key] = val;
    }
    return kv;
}

std::string trim(std::string x) {
    auto issp = [](unsigned char c) { return std::isspace(c); };
    while (!x.empty() && issp(x.front())) x.erase(x.begin());
    while (!x.empty() && issp(x.back())) x.pop_back();
    return x;
}

std::string humanBytes(uint64_t b) {
    static const char* u[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB"};
    int i = 0;
    double v = (double)b;
    while (v >= 1024.0 && i < 5) {
        v /= 1024.0;
        ++i;
    }
    std::ostringstream os;
    os << std::fixed << std::setprecision(v >= 10 ? 0 : 1) << v << u[i];
    return os.str();
}

double pct(uint64_t used, uint64_t total) {
    if (total == 0) return 0.0;
    return (double)used * 100.0 / (double)total;
}