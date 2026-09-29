#include "ui/render.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <set>

#include "ui/term.hpp"
#include "util/strings.hpp"

namespace leet::ui {

// ============================================================================
//  Markdown-lite
// ============================================================================
namespace {

enum SegStyle { kPlain = 0, kBold = 1, kItalic = 2, kCode = 4 };

struct Seg {
    std::string text;
    int style;
};

std::string sgr_for(int st) {
    std::string s;
    auto add = [&](const char *c) { if (!s.empty()) s += ';'; s += c; };
    if (st & kCode) add("38;5;222");
    if (st & kBold) add("1");
    if (st & kItalic) add("3");
    return s;
}

std::vector<Seg> inline_segments(const std::string &s) {
    std::vector<Seg> out;
    int st = kPlain;
    std::string cur;
    auto flush = [&] {
        if (!cur.empty()) out.push_back({cur, st});
        cur.clear();
    };
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '`') {
            size_t j = s.find('`', i + 1);
            if (j != std::string::npos) {
                flush();
                out.push_back({s.substr(i + 1, j - i - 1), st | kCode});
                i = j;
                continue;
            }
        }
        if (c == '*' && i + 1 < s.size() && s[i + 1] == '*') {
            flush();
            st ^= kBold;
            ++i;
            continue;
        }
        if (c == '*' && (st & kItalic) == 0 && i + 1 < s.size() && s[i + 1] != ' ' &&
            (i == 0 || s[i - 1] == ' ' || s[i - 1] == '(') && s.find('*', i + 1) != std::string::npos) {
            flush();
            st ^= kItalic;
            continue;
        }
        if (c == '*' && (st & kItalic)) {
            flush();
            st ^= kItalic;
            continue;
        }
        cur += c;
    }
    flush();
    return out;
}

// Greedy word wrap over styled segments. Returns rendered lines.
std::vector<std::string> wrap_segments(const std::vector<Seg> &segs, int width) {
    // Split into words, each word being a list of styled pieces.
    std::vector<std::vector<Seg>> words;
    std::vector<Seg> word;
    for (auto &sg : segs) {
        std::string piece;
        for (char c : sg.text) {
            if (c == ' ') {
                if (!piece.empty()) word.push_back({piece, sg.style});
                piece.clear();
                if (!word.empty()) words.push_back(word);
                word.clear();
            } else {
                piece += c;
            }
        }
        if (!piece.empty()) word.push_back({piece, sg.style});
    }
    if (!word.empty()) words.push_back(word);

    auto wlen = [](const std::vector<Seg> &w) {
        int n = 0;
        for (auto &p : w) n += str::display_width(p.text);
        return n;
    };
    auto render = [](const std::vector<Seg> &w) {
        std::string o;
        for (auto &p : w) o += style(sgr_for(p.style), p.text);
        return o;
    };

    std::vector<std::string> lines;
    std::string line;
    int used = 0;
    for (auto &w : words) {
        int l = wlen(w);
        if (used > 0 && used + 1 + l > width) {
            lines.push_back(line);
            line.clear();
            used = 0;
        }
        if (used > 0) { line += ' '; ++used; }
        line += render(w);
        used += l;
    }
    if (!line.empty() || lines.empty()) lines.push_back(line);
    return lines;
}

}  // namespace

std::string markdown(const std::string &text, int width, int indent) {
    std::string pad(indent, ' ');
    std::string out;
    auto lines = str::split_lines(text);
    std::string para;
    auto flush_para = [&] {
        if (str::trim(para).empty()) { para.clear(); return; }
        for (auto &l : wrap_segments(inline_segments(str::trim(para)), width - indent)) out += pad + l + "\n";
        out += "\n";
        para.clear();
    };
    for (size_t i = 0; i < lines.size(); ++i) {
        std::string raw = lines[i];
        std::string t = str::trim(raw);
        if (str::starts_with(t, "```")) {
            flush_para();
            std::string lang = str::trim(t.substr(3));
            std::string code;
            for (++i; i < lines.size() && !str::starts_with(str::trim(lines[i]), "```"); ++i) code += lines[i] + "\n";
            auto hl = highlight(code, lang.empty() ? "text" : lang);
            for (auto &l : hl) out += pad + "  " + l + "\n";
            out += "\n";
            continue;
        }
        if (t.empty()) { flush_para(); continue; }
        if (str::starts_with(t, "#")) {
            flush_para();
            size_t k = t.find_first_not_of('#');
            out += pad + accent(str::trim(t.substr(k))) + "\n";
            continue;
        }
        bool bullet = str::starts_with(t, "- ") || str::starts_with(t, "* ");
        bool numbered = !bullet && t.size() > 2 && std::isdigit((unsigned char)t[0]) &&
                        (t.find(". ") == 1 || t.find(". ") == 2);
        if (bullet || numbered) {
            flush_para();
            std::string marker, body;
            if (bullet) { marker = sym().bullet; body = t.substr(2); }
            else { size_t d = t.find(". "); marker = t.substr(0, d + 1); body = t.substr(d + 2); }
            // continuation lines (indented, non-bullet)
            while (i + 1 < lines.size()) {
                std::string nt = str::trim(lines[i + 1]);
                if (nt.empty() || str::starts_with(nt, "- ") || str::starts_with(nt, "* ") || str::starts_with(nt, "```"))
                    break;
                if (!(lines[i + 1].size() > 0 && lines[i + 1][0] == ' ')) break;
                body += " " + nt;
                ++i;
            }
            int mw = str::display_width(marker) + 1;
            auto wrapped = wrap_segments(inline_segments(body), width - indent - mw);
            for (size_t k = 0; k < wrapped.size(); ++k)
                out += pad + (k == 0 ? accent2(marker) + " " : std::string(mw, ' ')) + wrapped[k] + "\n";
            bool next_is_item = i + 1 < lines.size() &&
                                (str::starts_with(str::trim(lines[i + 1]), "- ") ||
                                 str::starts_with(str::trim(lines[i + 1]), "* ") ||
                                 (lines[i + 1].size() > 2 && std::isdigit((unsigned char)str::trim(lines[i + 1])[0])));
            if (!next_is_item) out += "\n";
            continue;
        }
        para += " " + t;
    }
    flush_para();
    while (str::ends_with(out, "\n\n")) out.pop_back();
    return out;
}

// ============================================================================
//  Syntax highlighting
// ============================================================================
namespace {

const std::set<std::string> kKeywords = {
    "alignas", "alignof", "auto", "break", "case", "catch", "class", "const", "constexpr", "const_cast",
    "continue", "decltype", "default", "delete", "do", "dynamic_cast", "else", "enum", "explicit",
    "extern", "false", "final", "for", "friend", "goto", "if", "inline", "mutable", "namespace", "new",
    "noexcept", "nullptr", "NULL", "operator", "override", "private", "protected", "public", "register",
    "reinterpret_cast", "restrict", "return", "sizeof", "static", "static_assert", "static_cast",
    "struct", "switch", "template", "this", "throw", "true", "try", "typedef", "typename", "union",
    "using", "virtual", "volatile", "while"};

const std::set<std::string> kTypes = {
    "void", "int", "long", "short", "char", "bool", "float", "double", "unsigned", "signed", "size_t",
    "string", "vector", "map", "unordered_map", "set", "unordered_set", "multiset", "multimap", "pair",
    "queue", "priority_queue", "stack", "deque", "list", "array", "tuple", "ListNode", "TreeNode",
    "Node", "uint32_t", "uint64_t", "int64_t", "int32_t", "uint8_t", "int8_t", "uint16_t", "int16_t",
    "greater", "less", "function", "optional", "bitset", "string_view", "std", "Solution", "FILE",
    "ptrdiff_t", "intptr_t", "uintptr_t"};

struct Tok {
    std::string text;
    std::string sgr;
};

std::vector<Tok> tokenize(const std::string &src) {
    std::vector<Tok> out;
    size_t i = 0, n = src.size();
    bool line_start = true;
    auto is_id = [](char c) { return std::isalnum((unsigned char)c) || c == '_'; };
    while (i < n) {
        char c = src[i];
        if (c == '\n') { out.push_back({"\n", ""}); ++i; line_start = true; continue; }
        if (c == ' ' || c == '\t') {
            size_t j = i;
            while (j < n && (src[j] == ' ' || src[j] == '\t')) ++j;
            out.push_back({src.substr(i, j - i), ""});
            i = j;
            continue;
        }
        if (line_start && c == '#') {
            size_t j = src.find('\n', i);
            if (j == std::string::npos) j = n;
            out.push_back({src.substr(i, j - i), "38;5;176"});
            i = j;
            continue;
        }
        line_start = false;
        if (c == '/' && i + 1 < n && src[i + 1] == '/') {
            size_t j = src.find('\n', i);
            if (j == std::string::npos) j = n;
            out.push_back({src.substr(i, j - i), "3;38;5;108"});
            i = j;
            continue;
        }
        if (c == '/' && i + 1 < n && src[i + 1] == '*') {
            size_t j = src.find("*/", i + 2);
            j = (j == std::string::npos) ? n : j + 2;
            out.push_back({src.substr(i, j - i), "3;38;5;108"});
            i = j;
            continue;
        }
        if (c == '"' || c == '\'') {
            size_t j = i + 1;
            while (j < n && src[j] != c && src[j] != '\n') j += (src[j] == '\\') ? 2 : 1;
            j = std::min(n, j + 1);
            out.push_back({src.substr(i, j - i), "38;5;186"});
            i = j;
            continue;
        }
        if (std::isdigit((unsigned char)c)) {
            size_t j = i;
            while (j < n && (std::isalnum((unsigned char)src[j]) || src[j] == '.' || src[j] == '\'')) ++j;
            out.push_back({src.substr(i, j - i), "38;5;141"});
            i = j;
            continue;
        }
        if (is_id(c)) {
            size_t j = i;
            while (j < n && is_id(src[j])) ++j;
            std::string w = src.substr(i, j - i);
            size_t k = j;
            while (k < n && src[k] == ' ') ++k;
            std::string sgr;
            if (kKeywords.count(w)) sgr = "38;5;204";
            else if (kTypes.count(w)) sgr = "38;5;81";
            else if (k < n && src[k] == '(') sgr = "38;5;149";
            out.push_back({w, sgr});
            i = j;
            continue;
        }
        out.push_back({std::string(1, c), ""});
        ++i;
    }
    return out;
}

}  // namespace

std::vector<std::string> highlight(const std::string &code, const std::string &lang) {
    std::vector<std::string> lines(1);
    if (lang == "text" || !color()) {
        auto l = str::split_lines(code);
        return l.empty() ? std::vector<std::string>{""} : l;
    }
    for (auto &t : tokenize(code)) {
        // a token may span lines (block comments)
        size_t start = 0;
        while (true) {
            size_t nl = t.text.find('\n', start);
            std::string part = t.text.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
            if (!part.empty()) lines.back() += t.sgr.empty() ? part : style(t.sgr, part);
            if (nl == std::string::npos) break;
            lines.emplace_back();
            start = nl + 1;
        }
    }
    if (lines.size() > 1 && lines.back().empty()) lines.pop_back();
    return lines;
}

std::string code_block(const std::string &code, const std::string &lang, const std::string &title) {
    auto lines = highlight(code, lang);
    int digits = (int)std::to_string(lines.size()).size();
    std::vector<std::string> body;
    int inner = width() - 4;
    for (size_t i = 0; i < lines.size(); ++i) {
        std::string num = str::pad_left(std::to_string(i + 1), digits);
        std::string l = dim(num) + " " + dim(sym().v) + " " + lines[i];
        body.push_back(str::truncate(l, inner));
    }
    return box(body, title);
}

// ============================================================================
//  Boxes, rules, bars, tables
// ============================================================================
std::string box(const std::vector<std::string> &lines, const std::string &title, const std::string &sgr, int w) {
    const auto &s = sym();
    if (w <= 0) w = width();
    int inner = w - 4;
    for (auto &l : lines) inner = std::max(inner, std::min(str::display_width(l), width() - 4));
    inner = std::min(inner, width() - 4);
    auto col = [&](const std::string &x) { return style(sgr, x); };
    std::string out;
    std::string top = s.tl + std::string(s.h);
    int title_w = 0;
    if (!title.empty()) {
        top += " ";
        title_w = str::display_width(title) + 2;
    }
    std::string fill;
    for (int i = 0; i < inner + 2 - 1 - title_w; ++i) fill += s.h;
    out += "  " + col(top) + (title.empty() ? "" : bold(title) + " ") + col(fill + s.tr) + "\n";
    for (auto &l : lines) {
        std::string t = str::truncate(l, inner);
        out += "  " + col(s.v) + " " + str::pad_right(t, inner) + " " + col(s.v) + "\n";
    }
    std::string bottom = s.bl;
    for (int i = 0; i < inner + 2; ++i) bottom += s.h;
    out += "  " + col(bottom + s.br) + "\n";
    return out;
}

std::string rule(const std::string &title, const std::string &sgr) {
    const auto &s = sym();
    std::string left = std::string(s.h) + s.h + " ";
    int used = 3 + str::display_width(title) + 1;
    std::string right;
    for (int i = used; i < width(); ++i) right += s.h;
    return "  " + style(sgr, left) + bold(title) + " " + style("38;5;240", right);
}

std::string progress_bar(double frac, int w, const std::string &sgr) {
    frac = std::clamp(frac, 0.0, 1.0);
    const auto &s = sym();
    int full = (int)(frac * w + 1e-9);
    std::string a, b;
    for (int i = 0; i < full; ++i) a += s.bar_full;
    for (int i = full; i < w; ++i) b += s.bar_empty;
    return style(sgr, a) + style("38;5;238", b);
}

void Table::column(const std::string &header, Align a, int max_width) { cols_.push_back({header, a, max_width}); }

void Table::row(std::vector<std::string> cells) {
    cells.resize(cols_.size());
    rows_.push_back(std::move(cells));
}

std::string Table::render(int indent) const {
    std::vector<int> w(cols_.size());
    for (size_t c = 0; c < cols_.size(); ++c) {
        w[c] = str::display_width(cols_[c].header);
        for (auto &r : rows_) w[c] = std::max(w[c], str::display_width(r[c]));
        if (cols_[c].max_width > 0) w[c] = std::min(w[c], cols_[c].max_width);
    }
    auto cell = [&](const std::string &txt, size_t c) {
        std::string t = str::truncate(txt, w[c]);
        int pad = w[c] - str::display_width(t);
        switch (cols_[c].align) {
            case Right: return std::string(pad, ' ') + t;
            case Center: return std::string(pad / 2, ' ') + t + std::string(pad - pad / 2, ' ');
            default: return t + std::string(pad, ' ');
        }
    };
    std::string pad(indent, ' ');
    std::string out = pad;
    for (size_t c = 0; c < cols_.size(); ++c) out += (c ? "  " : "") + style("1;38;5;245", cell(cols_[c].header, c));
    out += "\n" + pad;
    int total = 0;
    for (size_t c = 0; c < cols_.size(); ++c) total += w[c] + (c ? 2 : 0);
    out += style("38;5;238", str::repeat(sym().h, total)) + "\n";
    for (auto &r : rows_) {
        out += pad;
        for (size_t c = 0; c < cols_.size(); ++c) out += (c ? "  " : "") + cell(r[c], c);
        out += "\n";
    }
    return out;
}

// ============================================================================
//  Spinner
// ============================================================================
Spinner::Spinner(std::string label) : label_(std::move(label)) {
    if (!is_tty()) return;
    running_ = true;
    th_ = std::thread([this] {
        static const char *uni[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
        static const char *asc[] = {"|", "/", "-", "\\"};
        int i = 0;
        while (running_) {
            std::string l;
            {
                std::lock_guard<std::mutex> g(mu_);
                l = label_;
            }
            const char *f = unicode() ? uni[i % 10] : asc[i % 4];
            std::fprintf(stdout, "\r\x1b[2K  %s %s", style("38;5;45", f).c_str(), l.c_str());
            std::fflush(stdout);
            ++i;
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
        }
    });
}

Spinner::~Spinner() { stop(); }

void Spinner::set_label(const std::string &label) {
    std::lock_guard<std::mutex> g(mu_);
    label_ = label;
}

void Spinner::stop(const std::string &final_line) {
    if (running_) {
        running_ = false;
        if (th_.joinable()) th_.join();
        std::fprintf(stdout, "\r\x1b[2K");
        std::fflush(stdout);
    }
    if (!final_line.empty()) std::cout << final_line << std::endl;
}

}  // namespace leet::ui
