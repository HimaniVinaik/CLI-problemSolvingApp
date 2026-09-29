// String helpers used across the app.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace leet::str {

std::string trim(const std::string &s);
std::string rtrim(const std::string &s);
std::string lower(std::string s);
bool starts_with(const std::string &s, const std::string &p);
bool ends_with(const std::string &s, const std::string &p);
bool contains_ci(const std::string &hay, const std::string &needle);
std::vector<std::string> split(const std::string &s, char sep, bool keep_empty = true);
std::vector<std::string> split_lines(const std::string &s);
std::string join(const std::vector<std::string> &v, const std::string &sep);
std::string replace_all(std::string s, const std::string &from, const std::string &to);
std::string repeat(const std::string &s, int n);
std::string pad_left(const std::string &s, int width);
std::string pad_right(const std::string &s, int width);

// Display width of a UTF-8 string, ignoring ANSI escape sequences.
int display_width(const std::string &s);
// Truncate to a display width, appending an ellipsis if cut.
std::string truncate(const std::string &s, int width);
// Strip ANSI escapes.
std::string strip_ansi(const std::string &s);

uint64_t fnv1a(const std::string &data, uint64_t seed = 1469598103934665603ull);
std::string hex(uint64_t v);

// Human readable durations / sizes.
std::string format_ns(double ns);
std::string format_bytes(double bytes);
std::string format_count(long long n);

}  // namespace leet::str
