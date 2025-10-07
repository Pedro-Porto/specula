/**
 * @file cli_utils.h
 * @brief Utility functions and constants for CLI operations.
 */

#pragma once

#include <string>
#include <vector>

/**
 * @namespace ansi
 * @brief Contains ANSI escape codes for terminal formatting.
 */
namespace ansi {
    /** Reset formatting */
    static constexpr const char* reset = "\x1b[0m";
    /** Bold text */
    static constexpr const char* bold = "\x1b[1m";
    /** Inverted colors */
    static constexpr const char* inv = "\x1b[7m";
    /** Faint text */
    static constexpr const char* faint = "\x1b[2m";
    /** Move cursor to home position */
    static constexpr const char* home = "\x1b[H";
    /** Clear from cursor to end of screen */
    static constexpr const char* clr_eos = "\x1b[0J";
    /** Clear entire screen */
    static constexpr const char* clr_all = "\x1b[2J";
    /** Hide cursor */
    static constexpr const char* hide_cursor = "\x1b[?25l";
    /** Show cursor */
    static constexpr const char* show_cursor = "\x1b[?25h";
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
void print_table(
    const std::vector<std::string>& headers,
    const std::vector<std::vector<std::string>>& rows,
    const std::string& info = "",
    bool clear_full_screen = false
);
