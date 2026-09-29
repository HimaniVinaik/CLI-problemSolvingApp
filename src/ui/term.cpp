#include "ui/term.hpp"

#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "util/strings.hpp"

namespace leet::ui {

namespace {
bool g_color = false;
bool g_unicode = true;
bool g_tty = false;
int g_width = 100;

const Symbols kUnicode = {
    "✔", "✘", "⚠", "•", "›", "·", "★", "○", "◐",
    "─", "│", "╭", "╮", "╰", "╯", "├", "┤",
    "█", "░",
    {" ", "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"},
};
const Symbols kAscii = {
    "OK", "X", "!", "*", ">", ".", "*", "o", "~",
    "-", "|", "+", "+", "+", "+", "+", "+",
    "#", ".",
    {" ", ".", ".", ":", ":", "|", "|", "#", "#"},
};
}  // namespace

void init_terminal() {
    g_tty = ::isatty(STDOUT_FILENO);
    const char *term = std::getenv("TERM");
    bool dumb = term && std::strcmp(term, "dumb") == 0;
    g_color = g_tty && !dumb && std::getenv("NO_COLOR") == nullptr;
    if (std::getenv("LEET_COLOR") || std::getenv("CLICOLOR_FORCE")) g_color = true;

    std::string loc;
    for (const char *v : {"LC_ALL", "LC_CTYPE", "LANG"}) {
        const char *x = std::getenv(v);
        if (x && *x) { loc = x; break; }
    }
    loc = str::lower(loc);
    g_unicode = loc.empty() || loc.find("utf-8") != std::string::npos || loc.find("utf8") != std::string::npos;
    if (std::getenv("LEET_ASCII")) g_unicode = false;

    int cols = 0;
    struct winsize ws;
    if (g_tty && ::ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) cols = ws.ws_col;
    if (!cols) {
        const char *c = std::getenv("COLUMNS");
        cols = c ? std::atoi(c) : 100;
    }
    g_width = std::clamp(cols - 2, 50, 110);
}

void set_color(bool on) { g_color = on; }
bool color() { return g_color; }
bool unicode() { return g_unicode; }
bool is_tty() { return g_tty; }
int width() { return g_width; }

std::string style(const std::string &sgr, const std::string &text) {
    if (!g_color || text.empty()) return text;
    return "\x1b[" + sgr + "m" + text + "\x1b[0m";
}

std::string bold(const std::string &s) { return style("1", s); }
std::string dim(const std::string &s) { return style("2", s); }
std::string italic(const std::string &s) { return style("3", s); }
std::string accent(const std::string &s) { return style("1;38;5;45", s); }
std::string accent2(const std::string &s) { return style("38;5;141", s); }
std::string ok(const std::string &s) { return style("1;38;5;78", s); }
std::string warn(const std::string &s) { return style("1;38;5;214", s); }
std::string err(const std::string &s) { return style("1;38;5;203", s); }
std::string info(const std::string &s) { return style("38;5;117", s); }
std::string code(const std::string &s) { return style("38;5;222", s); }

std::string difficulty_color(const std::string &d) {
    std::string l = str::lower(d);
    if (l == "easy") return "38;5;78";
    if (l == "medium") return "38;5;214";
    if (l == "hard") return "38;5;203";
    return "38;5;250";
}

std::string difficulty(const std::string &d) { return style(difficulty_color(d), d); }

const Symbols &sym() { return g_unicode ? kUnicode : kAscii; }

}  // namespace leet::ui
