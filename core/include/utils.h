#pragma once
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>

std::string trim(std::string x);
std::unordered_map<std::string, std::string> parse_kv(const std::string& s);
std::string humanBytes(uint64_t b);
double pct(uint64_t used, uint64_t total);