#include "util/strings.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace leet::str {

std::string trim(const std::string &s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::string rtrim(const std::string &s) {
    size_t b = s.find_last_not_of(" \t\r\n");
    return b == std::string::npos ? "" : s.substr(0, b + 1);
}

std::string lower(std::string s) {
    for (auto &c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

bool starts_with(const std::string &s, const std::string &p) { return s.compare(0, p.size(), p) == 0; }

bool ends_with(const std::string &s, const std::string &p) {
    return s.size() >= p.size() && s.compare(s.size() - p.size(), p.size(), p) == 0;
}

bool contains_ci(const std::string &hay, const std::string &needle) {
    return lower(hay).find(lower(needle)) != std::string::npos;
}

std::vector<std::string> split(const std::string &s, char sep, bool keep_empty) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == sep) {
            if (keep_empty || !cur.empty()) out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (keep_empty || !cur.empty()) out.push_back(cur);
    return out;
}

std::vector<std::string> split_lines(const std::string &s) {
    std::vector<std::string> out = split(s, '\n');
    for (auto &l : out)
        if (!l.empty() && l.back() == '\r') l.pop_back();
    if (!out.empty() && out.back().empty()) out.pop_back();
    return out;
}

std::string join(const std::vector<std::string> &v, const std::string &sep) {
    std::string o;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) o += sep;
        o += v[i];
    }
    return o;
}

std::string replace_all(std::string s, const std::string &from, const std::string &to) {
    if (from.empty()) return s;
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
    return s;
}

std::string repeat(const std::string &s, int n) {
    std::string o;
    for (int i = 0; i < n; ++i) o += s;
    return o;
}

std::string pad_left(const std::string &s, int width) {
    int w = display_width(s);
    return w >= width ? s : std::string(width - w, ' ') + s;
}

std::string pad_right(const std::string &s, int width) {
    int w = display_width(s);
    return w >= width ? s : s + std::string(width - w, ' ');
}

// Rough East-Asian-width free UTF-8 width: every code point counts as 1,
// except combining marks (0) — good enough for our box drawing / symbols.
int display_width(const std::string &s) {
    int w = 0;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = (unsigned char)s[i];
        if (c == 0x1b) {  // ANSI CSI sequence
            ++i;
            if (i < s.size() && s[i] == '[') {
                ++i;
                while (i < s.size() && !(s[i] >= 0x40 && s[i] <= 0x7e)) ++i;
                ++i;
            }
            continue;
        }
        int len = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xe ? 3 : (c >> 3) == 0x1e ? 4 : 1;
        uint32_t cp = c;
        if (len > 1 && i + len <= s.size()) {
            cp = c & (0xff >> (len + 1));
            for (int k = 1; k < len; ++k) cp = (cp << 6) | ((unsigned char)s[i + k] & 0x3f);
        }
        bool combining = (cp >= 0x300 && cp <= 0x36f) || cp == 0xfe0f;
        if (!combining) w += 1;
        i += len;
    }
    return w;
}

std::string truncate(const std::string &s, int width) {
    if (display_width(s) <= width) return s;
    if (width <= 1) return width == 1 ? "…" : "";
    std::string out;
    int w = 0;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = (unsigned char)s[i];
        if (c == 0x1b) {
            size_t j = i + 1;
            if (j < s.size() && s[j] == '[') {
                ++j;
                while (j < s.size() && !(s[j] >= 0x40 && s[j] <= 0x7e)) ++j;
                ++j;
            }
            out += s.substr(i, j - i);
            i = j;
            continue;
        }
        int len = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xe ? 3 : (c >> 3) == 0x1e ? 4 : 1;
        if (w + 1 > width - 1) break;
        out += s.substr(i, len);
        ++w;
        i += len;
    }
    return out + "…\x1b[0m";
}

std::string strip_ansi(const std::string &s) {
    std::string o;
    for (size_t i = 0; i < s.size();) {
        if (s[i] == 0x1b && i + 1 < s.size() && s[i + 1] == '[') {
            size_t j = i + 2;
            while (j < s.size() && !(s[j] >= 0x40 && s[j] <= 0x7e)) ++j;
            i = j + 1;
        } else {
            o += s[i++];
        }
    }
    return o;
}

uint64_t fnv1a(const std::string &data, uint64_t h) {
    for (unsigned char c : data) {
        h ^= c;
        h *= 1099511628211ull;
    }
    return h;
}

std::string hex(uint64_t v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%016llx", (unsigned long long)v);
    return buf;
}

std::string format_ns(double ns) {
    char buf[64];
    if (ns < 1e3) std::snprintf(buf, sizeof buf, "%.0f ns", ns);
    else if (ns < 1e6) std::snprintf(buf, sizeof buf, "%.1f µs", ns / 1e3);
    else if (ns < 1e9) std::snprintf(buf, sizeof buf, "%.1f ms", ns / 1e6);
    else std::snprintf(buf, sizeof buf, "%.2f s", ns / 1e9);
    return buf;
}

std::string format_bytes(double b) {
    char buf[64];
    if (b < 1024) std::snprintf(buf, sizeof buf, "%.0f B", b);
    else if (b < 1024 * 1024) std::snprintf(buf, sizeof buf, "%.1f KB", b / 1024);
    else if (b < 1024.0 * 1024 * 1024) std::snprintf(buf, sizeof buf, "%.1f MB", b / (1024 * 1024));
    else std::snprintf(buf, sizeof buf, "%.2f GB", b / (1024.0 * 1024 * 1024));
    return buf;
}

std::string format_count(long long n) {
    if (n >= 1000000 && n % 1000000 == 0) return std::to_string(n / 1000000) + "M";
    if (n >= 1000000) {
        char buf[32];
        std::snprintf(buf, sizeof buf, "%.1fM", n / 1e6);
        return buf;
    }
    if (n >= 10000) {
        char buf[32];
        std::snprintf(buf, sizeof buf, "%.0fK", n / 1e3);
        return buf;
    }
    return std::to_string(n);
}

}  // namespace leet::str
