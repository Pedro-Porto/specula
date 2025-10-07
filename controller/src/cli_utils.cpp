// cli_utils.cpp
#include "cli_utils.h"

#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string>

/**
 * @brief Retrieves the terminal size (columns and rows).
 *
 * This function uses the TIOCGWINSZ ioctl call to determine the size of the terminal window.
 * If the terminal size cannot be determined, it defaults to 120 columns and 40 rows.
 *
 * @return A pair containing the number of columns and rows of the terminal.
 */
static std::pair<int, int> term_size() {
#ifdef TIOCGWINSZ
    struct winsize w{};
    // Use ioctl to get terminal size
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0 &&
        w.ws_row > 0)
        return {w.ws_col, w.ws_row};
#endif
    // Default terminal size if ioctl fails
    return {120, 40};
}

/**
 * @brief Wraps a string into multiple lines based on the specified width.
 *
 * This function splits a string into lines, ensuring that each line does not exceed the given width.
 * It attempts to break lines at spaces for better readability.
 *
 * @param s The input string to wrap.
 * @param width The maximum width of each line.
 * @return A vector of strings, where each string is a wrapped line.
 */
static std::vector<std::string> wrap_info(const std::string& s, size_t width) {
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= s.size()) {
        size_t nl = s.find('\n', start); // Find the next newline character
        std::string line = s.substr(
            start, nl == std::string::npos ? std::string::npos : nl - start);

        if (width == 0 || line.size() <= width) {
            // Line fits within the width
            out.push_back(std::move(line));
        } else {
            size_t i = 0;
            while (i < line.size()) {
                size_t len = std::min(width, line.size() - i);
                size_t cut = line.rfind(' ', i + len); // Try to break at a space
                if (cut != std::string::npos && cut >= i + len / 2) {
                    // Break at the space
                    out.push_back(line.substr(i, cut - i));
                    i = cut + 1;
                } else {
                    // No suitable space found, break at max width
                    out.push_back(line.substr(i, len));
                    i += len;
                }
            }
        }

        if (nl == std::string::npos) break; // No more newlines
        start = nl + 1; // Move to the next line
    }
    return out;
}

/**
 * @brief Prints a formatted table to the terminal.
 *
 * This function displays a table with headers and rows, formatted to fit within the terminal size.
 * It also includes an optional informational message above the table.
 *
 * @param headers A vector of column headers.
 * @param rows A vector of rows, where each row is a vector of strings representing cell values.
 * @param info An optional informational message to display above the table.
 * @param clear_full_screen If true, clears the entire terminal screen before printing.
 */
void print_table(const std::vector<std::string>& headers,
                 const std::vector<std::vector<std::string>>& rows,
                 const std::string& info, bool clear_full_screen) {
    // Hide cursor and move to the top of the screen
    std::cout << ansi::hide_cursor << ansi::home;
    if (clear_full_screen)
        std::cout << ansi::clr_all << ansi::home; // Clear the entire screen
    else
        std::cout << ansi::clr_eos; // Clear from cursor to end of screen

    auto [term_cols, term_rows] = term_size(); // Get terminal size
    size_t max_width = static_cast<size_t>(term_cols);

    size_t info_lines = 0;
    if (!info.empty()) {
        // Wrap and print the informational message
        auto wrapped = wrap_info(info, max_width ? max_width : info.size());
        for (const auto& ln : wrapped) {
            std::cout << ln << ansi::reset << "\n";
        }
        std::cout << "\n";
        info_lines = wrapped.size() + 1;
    }

    if (headers.empty()) {
        // No headers to display
        std::cout << "(no columns)\n" << ansi::show_cursor << std::flush;
        return;
    }

    const size_t cols = headers.size();
    std::vector<size_t> widths(cols, 0);
    auto visible_len = [](const std::string& s) { return s.size(); }; // Calculate visible length of a string
    auto trunc = [](const std::string& s, size_t w) {
        // Truncate a string to fit within the specified width
        if (w == 0 || s.size() <= w) return s;
        if (w <= 1) return std::string(w, '.');
        return s.substr(0, w - 1) + "…";
    };

    // Calculate column widths based on headers and rows
    for (size_t c = 0; c < cols; ++c)
        widths[c] = std::max(widths[c], visible_len(headers[c]));
    for (const auto& r : rows)
        for (size_t c = 0; c < cols && c < r.size(); ++c)
            widths[c] = std::max(widths[c], visible_len(r[c]));

    auto total_with_seps = [&](const std::vector<size_t>& w) {
        // Calculate total width including column separators
        size_t s = 0;
        for (size_t i = 0; i < w.size(); ++i) {
            s += w[i];
            if (i + 1 < w.size()) s += 3; // Add space for separators
        }
        return s;
    };

    if (max_width && total_with_seps(widths) > max_width) {
        // Adjust column widths to fit within the terminal width
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
        // Print a single row of the table
        for (size_t c = 0; c < cols; ++c) {
            std::string cell = (c < cells.size()) ? cells[c] : "";
            cell = trunc(cell, widths[c]);
            std::cout << std::left << std::setw((int)widths[c]) << cell;
            if (c + 1 < cols) std::cout << " | ";
        }
        std::cout << "\n";
    };
    auto print_sep = [&]() {
        // Print a separator line between rows
        for (size_t c = 0; c < cols; ++c) {
            std::cout << std::string(widths[c], '-');
            if (c + 1 < cols) std::cout << "-+-";
        }
        std::cout << "\n";
    };

    // Print the header row with inverted colors
    std::cout << ansi::inv << ansi::bold;
    print_row(headers);
    std::cout << ansi::reset;

    print_sep(); // Print the separator line

    size_t lines_printed = info_lines + 2;
    for (const auto& r : rows) {
        print_row(r); // Print each row of the table
        ++lines_printed;
    }

    // Pad the remaining lines to fill the terminal
    int pad = term_rows - static_cast<int>(lines_printed) - 1;
    for (int i = 0; i < pad; ++i) {
        std::cout << std::string(std::max(0, term_cols - 1), ' ') << "\n";
    }

    std::cout << ansi::show_cursor << std::flush; // Show the cursor again
}
