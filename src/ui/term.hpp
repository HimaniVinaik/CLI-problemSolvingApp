// Terminal capabilities and semantic styling.
#pragma once

#include <string>

namespace leet::ui {

void init_terminal();              // detect colors / unicode / width
void set_color(bool on);
bool color();
bool unicode();
bool is_tty();
int width();                       // usable content width (clamped)

// Raw SGR wrapper: style("1;36", "text")
std::string style(const std::string &sgr, const std::string &text);

// Semantic helpers
std::string bold(const std::string &s);
std::string dim(const std::string &s);
std::string italic(const std::string &s);
std::string accent(const std::string &s);     // brand color
std::string accent2(const std::string &s);    // secondary brand color
std::string ok(const std::string &s);
std::string warn(const std::string &s);
std::string err(const std::string &s);
std::string info(const std::string &s);
std::string code(const std::string &s);       // inline code
std::string difficulty(const std::string &d); // colored Easy/Medium/Hard
std::string difficulty_color(const std::string &d);  // SGR code

struct Symbols {
    const char *check, *cross, *warn, *bullet, *arrow, *dot, *star, *circle, *half;
    const char *h, *v, *tl, *tr, *bl, *br, *lt, *rt;  // box drawing
    const char *bar_full, *bar_empty;
    const char *bars[9];  // fractional block characters for charts
};
const Symbols &sym();

}  // namespace leet::ui
