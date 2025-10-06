// cli_utils.cpp
#include "cli_utils.h"

#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string>

namespace ansi {
static constexpr const char* reset = "\x1b[0m";
static constexpr const char* bold = "\x1b[1m";
static constexpr const char* inv = "\x1b[7m";
static constexpr const char* faint = "\x1b[2m";
static constexpr const char* home = "\x1b[H";
static constexpr const char* clr_eos = "\x1b[0J";
static constexpr const char* clr_all = "\x1b[2J";
static constexpr const char* hide_cursor = "\x1b[?25l";
static constexpr const char* show_cursor = "\x1b[?25h";
}

static std::pair<int, int> term_size() {
#ifdef TIOCGWINSZ
    struct winsize w{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0 &&
        w.ws_row > 0)
        return {w.ws_col, w.ws_row};
#endif
    return {120, 40};
}

static std::vector<std::string> wrap_info(const std::string& s, size_t width) {
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= s.size()) {
        size_t nl = s.find('\n', start);
        std::string line = s.substr(
            start, nl == std::string::npos ? std::string::npos : nl - start);

        if (width == 0 || line.size() <= width) {
            out.push_back(std::move(line));
        } else {
            size_t i = 0;
            while (i < line.size()) {
                size_t len = std::min(width, line.size() - i);
                size_t cut = line.rfind(' ', i + len);
                if (cut != std::string::npos && cut >= i + len / 2) {
                    out.push_back(line.substr(i, cut - i));
                    i = cut + 1;
                } else {
                    out.push_back(line.substr(i, len));
                    i += len;
                }
            }
        }

        if (nl == std::string::npos) break;
        start = nl + 1;
    }
    return out;
}

void print_table(const std::vector<std::string>& headers,
                 const std::vector<std::vector<std::string>>& rows,
                 const std::string& info, bool clear_full_screen) {
    std::cout << ansi::hide_cursor << ansi::home;
    if (clear_full_screen)
        std::cout << ansi::clr_all << ansi::home;
    else
        std::cout << ansi::clr_eos;

    auto [term_cols, term_rows] = term_size();
    size_t max_width = static_cast<size_t>(term_cols);

    size_t info_lines = 0;
    if (!info.empty()) {
        auto wrapped = wrap_info(info, max_width ? max_width : info.size());
        for (const auto& ln : wrapped) {
            std::cout << ln << ansi::reset << "\n";
        }
        std::cout << "\n";
        info_lines = wrapped.size() + 1;
    }

    if (headers.empty()) {
        std::cout << "(no columns)\n" << ansi::show_cursor << std::flush;
        return;
    }

    const size_t cols = headers.size();
    std::vector<size_t> widths(cols, 0);
    auto visible_len = [](const std::string& s) { return s.size(); };
    auto trunc = [](const std::string& s, size_t w) {
        if (w == 0 || s.size() <= w) return s;
        if (w <= 1) return std::string(w, '.');
        return s.substr(0, w - 1) + "…";
    };

    for (size_t c = 0; c < cols; ++c)
        widths[c] = std::max(widths[c], visible_len(headers[c]));
    for (const auto& r : rows)
        for (size_t c = 0; c < cols && c < r.size(); ++c)
            widths[c] = std::max(widths[c], visible_len(r[c]));

    auto total_with_seps = [&](const std::vector<size_t>& w) {
        size_t s = 0;
        for (size_t i = 0; i < w.size(); ++i) {
            s += w[i];
            if (i + 1 < w.size()) s += 3;
        }
        return s;
    };

    if (max_width && total_with_seps(widths) > max_width) {
        size_t over = total_with_seps(widths) - max_width;
        size_t i = 0;
        while (over > 0) {
            if (widths[i] > 3) {
                --widths[i];
                --over;
            }
            i = (i + 1) % cols;
        }
    }

    auto print_row = [&](const std::vector<std::string>& cells) {
        for (size_t c = 0; c < cols; ++c) {
            std::string cell = (c < cells.size()) ? cells[c] : "";
            cell = trunc(cell, widths[c]);
            std::cout << std::left << std::setw((int)widths[c]) << cell;
            if (c + 1 < cols) std::cout << " | ";
        }
        std::cout << "\n";
    };
    auto print_sep = [&]() {
        for (size_t c = 0; c < cols; ++c) {
            std::cout << std::string(widths[c], '-');
            if (c + 1 < cols) std::cout << "-+-";
        }
        std::cout << "\n";
    };

    std::cout << ansi::inv << ansi::bold;
    print_row(headers);
    std::cout << ansi::reset;

    print_sep();

    size_t lines_printed = info_lines + 2;
    for (const auto& r : rows) {
        print_row(r);
        ++lines_printed;
    }

    int pad = term_rows - static_cast<int>(lines_printed) - 1;
    for (int i = 0; i < pad; ++i) {
        std::cout << std::string(std::max(0, term_cols - 1), ' ') << "\n";
    }

    std::cout << ansi::show_cursor << std::flush;
}
