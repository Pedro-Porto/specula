#include "../include/system_helpers.h"

#include <sys/statvfs.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

bool read_proc_stat(uint64_t& idle, uint64_t& total) {
    FILE* f = fopen("/proc/stat", "r");
    if (!f) return false;
    char line[512];
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return false;
    }
    fclose(f);

    std::istringstream iss(line);
    std::string cpu;
    uint64_t user = 0, nice_ = 0, system_ = 0, idle_ = 0, iowait = 0, irq = 0,
             soft = 0, steal = 0, guest = 0, gnice = 0;
    iss >> cpu >> user >> nice_ >> system_ >> idle_ >> iowait >> irq >> soft >>
        steal >> guest >> gnice;
    idle = idle_ + iowait;
    total = user + nice_ + system_ + idle_ + iowait + irq + soft + steal +
            guest + gnice;
    return true;
}
float get_cpu_percent() {
    uint64_t idle1 = 0, total1 = 0, idle2 = 0, total2 = 0;
    if (!read_proc_stat(idle1, total1)) return 0.0f;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (!read_proc_stat(idle2, total2)) return 0.0f;
    uint64_t didle = (idle2 - idle1);
    uint64_t dtotal = (total2 - total1);
    if (dtotal == 0) return 0.0f;
    float usage = 100.0f * (float)(dtotal - didle) / (float)dtotal;
    if (usage < 0) usage = 0;
    if (usage > 100) usage = 100;
    return usage;
}
void get_mem(uint64_t& used_kb, uint64_t& total_kb) {
    FILE* f = fopen("/proc/meminfo", "r");
    if (!f) {
        used_kb = total_kb = 0;
        return;
    }
    char k[64];
    uint64_t v = 0;
    char unit[16];
    uint64_t memTotal = 0, memAvailable = 0;
    while (fscanf(f, "%63s %lu %15s\n", k, &v, unit) == 3) {
        if (strcmp(k, "MemTotal:") == 0)
            memTotal = v;
        else if (strcmp(k, "MemAvailable:") == 0)
            memAvailable = v;
        if (memTotal && memAvailable) break;
    }
    fclose(f);
    total_kb = memTotal;
    used_kb = (memAvailable > memTotal) ? 0 : (memTotal - memAvailable);
}
void get_disk(uint64_t& used_kb, uint64_t& total_kb, const char* path = "/") {
    struct statvfs s{};
    if (statvfs(path, &s) != 0) {
        used_kb = total_kb = 0;
        return;
    }
    uint64_t total = (uint64_t)s.f_blocks * s.f_frsize;
    uint64_t freeb = (uint64_t)s.f_bfree * s.f_frsize;
    used_kb = (total - freeb) / 1024;
    total_kb = total / 1024;
}

int exec_command_stream(const std::string& cmd,
                        std::function<void(const std::string&)> on_chunk) {
    std::string shell = "/bin/sh -c \"" + cmd + "\"";
    FILE* p = popen(shell.c_str(), "r");
    if (!p) return 127;
    char buf[4096];
    while (true) {
        size_t n = fread(buf, 1, sizeof(buf), p);
        if (n > 0) {
            on_chunk(std::string(buf, buf + n));
        }
        if (n < sizeof(buf)) {
            if (feof(p)) break;
            if (ferror(p)) break;
        }
    }
    int status = pclose(p);
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return 128;
}