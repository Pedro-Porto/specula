#pragma once
#include <string>
#include <vector>
#include <cstddef>

void print_table(
    const std::vector<std::string>& headers,
    const std::vector<std::vector<std::string>>& rows,
    const std::string& info = "",
    bool clear_full_screen = false
);
