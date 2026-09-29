#include "app/lineedit.hpp"

#include <termios.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <iostream>

#include "util/fs.hpp"
#include "util/strings.hpp"

namespace leet {

namespace {
struct termios g_orig;

void write_out(const std::string &s) {
    ssize_t r = ::write(STDOUT_FILENO, s.data(), s.size());
    (void)r;
}

std::string common_prefix(const std::vector<std::string> &v) {
    if (v.empty()) return "";
    std::string p = v[0];
    for (auto &s : v) {
        size_t i = 0;
        while (i < p.size() && i < s.size() && p[i] == s[i]) ++i;
        p.resize(i);
    }
    return p;
}
}  // namespace

LineEditor::LineEditor(std::string file) : history_file_(std::move(file)) {
    if (auto c = fs::read_file(history_file_))
        for (auto &l : str::split_lines(*c))
            if (!l.empty()) history_.push_back(l);
}

LineEditor::~LineEditor() { disable_raw(); }

bool LineEditor::enable_raw() {
    if (!::isatty(STDIN_FILENO) || !::isatty(STDOUT_FILENO)) return false;
    if (::tcgetattr(STDIN_FILENO, &g_orig) < 0) return false;
    struct termios raw = g_orig;
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (::tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0) return false;
    raw_ = true;
    return true;
}

void LineEditor::disable_raw() {
    if (raw_) {
        ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig);
        raw_ = false;
    }
}

void LineEditor::save_history() const {
    size_t start = history_.size() > 500 ? history_.size() - 500 : 0;
    std::string o;
    for (size_t i = start; i < history_.size(); ++i) o += history_[i] + "\n";
    fs::write_file(history_file_, o);
}

bool LineEditor::read_fallback(const std::string &prompt, std::string &out) {
    std::cout << prompt << std::flush;
    return (bool)std::getline(std::cin, out);
}

bool LineEditor::read(const std::string &prompt, std::string &out) {
    if (!enable_raw()) return read_fallback(prompt, out);
    std::string buf;
    size_t pos = 0;
    size_t hidx = history_.size();
    std::string saved;

    auto redraw = [&] {
        std::string s = "\r\x1b[2K" + prompt + buf;
        int back = str::display_width(buf.substr(pos));
        if (back > 0) s += "\x1b[" + std::to_string(back) + "D";
        write_out(s);
    };
    auto prev_char = [&](size_t p) {  // UTF-8 aware
        if (p == 0) return p;
        --p;
        while (p > 0 && ((unsigned char)buf[p] & 0xC0) == 0x80) --p;
        return p;
    };
    auto next_char = [&](size_t p) {
        if (p >= buf.size()) return p;
        ++p;
        while (p < buf.size() && ((unsigned char)buf[p] & 0xC0) == 0x80) ++p;
        return p;
    };

    redraw();
    bool result = true;
    while (true) {
        char c;
        ssize_t n = ::read(STDIN_FILENO, &c, 1);
        if (n <= 0) { result = false; break; }
        if (c == '\r' || c == '\n') break;
        if (c == 4) {  // Ctrl-D
            if (buf.empty()) { result = false; break; }
            if (pos < buf.size()) { buf.erase(pos, next_char(pos) - pos); redraw(); }
            continue;
        }
        if (c == 3) {  // Ctrl-C: discard line
            write_out("^C\r\n");
            buf.clear();
            pos = 0;
            redraw();
            continue;
        }
        if (c == 127 || c == 8) {
            if (pos > 0) {
                size_t p = prev_char(pos);
                buf.erase(p, pos - p);
                pos = p;
                redraw();
            }
            continue;
        }
        if (c == 1) { pos = 0; redraw(); continue; }             // Ctrl-A
        if (c == 5) { pos = buf.size(); redraw(); continue; }    // Ctrl-E
        if (c == 21) { buf.erase(0, pos); pos = 0; redraw(); continue; }  // Ctrl-U
        if (c == 11) { buf.erase(pos); redraw(); continue; }     // Ctrl-K
        if (c == 12) { write_out("\x1b[2J\x1b[H"); redraw(); continue; }  // Ctrl-L
        if (c == 23) {  // Ctrl-W: delete previous word
            size_t p = pos;
            while (p > 0 && buf[p - 1] == ' ') --p;
            while (p > 0 && buf[p - 1] != ' ') --p;
            buf.erase(p, pos - p);
            pos = p;
            redraw();
            continue;
        }
        if (c == '\t') {
            if (!completer_) continue;
            auto cands = completer_(buf.substr(0, pos));
            if (cands.empty()) continue;
            std::string pre = common_prefix(cands);
            std::string head = buf.substr(0, pos);
            if (cands.size() == 1) pre += " ";
            if (pre.size() > head.size()) {
                buf = pre + buf.substr(pos);
                pos = pre.size();
                redraw();
            } else if (cands.size() > 1) {
                std::string list = "\r\n";
                size_t shown = 0;
                for (auto &s : cands) {
                    if (shown++ >= 40) { list += "…"; break; }
                    auto parts = str::split(s, ' ', false);
                    list += (parts.empty() ? s : parts.back()) + "  ";
                }
                write_out(list + "\r\n");
                redraw();
            }
            continue;
        }
        if (c == 27) {  // escape sequence
            char seq[3];
            if (::read(STDIN_FILENO, &seq[0], 1) != 1) continue;
            if (::read(STDIN_FILENO, &seq[1], 1) != 1) continue;
            if (seq[0] == '[') {
                if (seq[1] >= '0' && seq[1] <= '9') {
                    if (::read(STDIN_FILENO, &seq[2], 1) != 1) continue;
                    if (seq[2] == '~') {
                        if (seq[1] == '3' && pos < buf.size()) { buf.erase(pos, next_char(pos) - pos); }
                        if (seq[1] == '1' || seq[1] == '7') pos = 0;
                        if (seq[1] == '4' || seq[1] == '8') pos = buf.size();
                        redraw();
                    } else if (seq[2] == ';') {  // e.g. ESC[1;5C (ctrl+arrow): consume two more
                        char x[2];
                        if (::read(STDIN_FILENO, x, 2) == 2) {
                            if (x[1] == 'C') { while (pos < buf.size() && buf[pos] == ' ') ++pos; while (pos < buf.size() && buf[pos] != ' ') ++pos; }
                            if (x[1] == 'D') { while (pos > 0 && buf[pos - 1] == ' ') --pos; while (pos > 0 && buf[pos - 1] != ' ') --pos; }
                            redraw();
                        }
                    }
                    continue;
                }
                switch (seq[1]) {
                    case 'A':  // up
                        if (hidx > 0) {
                            if (hidx == history_.size()) saved = buf;
                            buf = history_[--hidx];
                            pos = buf.size();
                            redraw();
                        }
                        break;
                    case 'B':  // down
                        if (hidx < history_.size()) {
                            ++hidx;
                            buf = hidx == history_.size() ? saved : history_[hidx];
                            pos = buf.size();
                            redraw();
                        }
                        break;
                    case 'C': pos = next_char(pos); redraw(); break;
                    case 'D': pos = prev_char(pos); redraw(); break;
                    case 'H': pos = 0; redraw(); break;
                    case 'F': pos = buf.size(); redraw(); break;
                }
            } else if (seq[0] == 'O') {
                if (seq[1] == 'H') pos = 0;
                if (seq[1] == 'F') pos = buf.size();
                redraw();
            }
            continue;
        }
        if ((unsigned char)c >= 32) {
            buf.insert(pos, 1, c);
            ++pos;
            redraw();
        }
    }
    write_out("\r\n");
    disable_raw();
    if (!result) return false;
    out = buf;
    std::string t = str::trim(buf);
    if (!t.empty() && (history_.empty() || history_.back() != t)) {
        history_.push_back(t);
        save_history();
    }
    return true;
}

}  // namespace leet
