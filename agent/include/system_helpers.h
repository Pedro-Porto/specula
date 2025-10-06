#pragma once
#include <cstdint>
#include <functional>
#include <string>

float get_cpu_percent();
void get_mem(uint64_t& used_kb, uint64_t& total_kb);
void get_disk(uint64_t& used_kb, uint64_t& total_kb,
                     const char* path);
int exec_command_stream(const std::string& cmd,
                               std::function<void(const std::string&)> on_chunk);