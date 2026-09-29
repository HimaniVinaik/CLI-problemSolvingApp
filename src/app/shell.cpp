// Interactive shell:  `leet` with no arguments.
#include <iostream>

#include "app/app.hpp"
#include "app/lineedit.hpp"
#include "ui/render.hpp"
#include "ui/term.hpp"
#include "util/fs.hpp"
#include "util/strings.hpp"

namespace leet {

using std::cout;

namespace {
std::vector<std::string> tokenize(const std::string &line) {
    std::vector<std::string> out;
    std::string cur;
    bool in_q = false, have = false;
    char q = 0;
    for (char c : line) {
        if (in_q) {
            if (c == q) in_q = false;
            else cur += c;
        } else if (c == '"' || c == '\'') {
            in_q = true;
            q = c;
            have = true;
        } else if (c == ' ' || c == '\t') {
            if (have || !cur.empty()) out.push_back(cur);
            cur.clear();
            have = false;
        } else {
            cur += c;
        }
    }
    if (have || !cur.empty()) out.push_back(cur);
    return out;
}
}  // namespace

int App::shell() {
    in_shell_ = true;
    const char *logo[] = {
        R"(  _           _   )",
        R"( | | ___  ___| |_ )",
        R"( | |/ _ \/ _ \ __|)",
        R"( | |  __/  __/ |_ )",
        R"( |_|\___|\___|\__|)",
    };
    int solved = 0;
    for (auto &[k, e] : progress_->entries()) solved += e.status == "solved";
    size_t total = repo_.all().size();
    const char *side[] = {
        "",
        "LeetCode-style practice for C++ and C",
        "",
        "",
        "",
    };
    std::string prog = ui::dim("solved ") + ui::accent(std::to_string(solved)) + ui::dim(" / " + std::to_string(total)) + "  " +
                       ui::progress_bar(total ? (double)solved / total : 0, 16);
    cout << "\n";
    for (int i = 0; i < 5; ++i) {
        cout << "  " << ui::accent(logo[i]) << "   ";
        if (i == 1) cout << ui::bold(side[i]);
        if (i == 2) cout << prog;
        if (i == 3) cout << ui::dim("type ") << ui::code("help") << ui::dim(", ") << ui::code("list") << ui::dim(", or a problem id like ")
                         << ui::code("1") << ui::dim(" / ") << ui::code("c3");
        if (i == 4) cout << ui::dim("Tab completes, Up/Down browse history, Ctrl-D exits");
        cout << "\n";
    }
    cout << "\n";

    LineEditor le(fs::join(cfg_.state_dir, "history"));
    auto cmds = command_names();
    le.set_completer([&](const std::string &buf) {
        std::vector<std::string> out;
        auto parts = tokenize(buf);
        bool new_word = !buf.empty() && buf.back() == ' ';
        if (parts.empty() || (parts.size() == 1 && !new_word)) {
            std::string pre = parts.empty() ? "" : parts[0];
            for (auto &c : cmds)
                if (str::starts_with(c, pre)) out.push_back(c);
            return out;
        }
        if (parts.size() == 1 && new_word) parts.push_back("");
        if (parts.size() == 2) {
            std::string pre = str::lower(parts[1]);
            for (auto &p : repo_.all()) {
                if (str::starts_with(p.slug, pre)) out.push_back(parts[0] + " " + p.slug);
                else if (str::starts_with(p.key, pre) && !pre.empty()) out.push_back(parts[0] + " " + p.key);
            }
        }
        return out;
    });

    std::string line;
    while (le.read(prompt(), line)) {
        auto args = tokenize(str::trim(line));
        if (args.empty()) continue;
        std::string c = str::lower(args[0]);
        if (c == "quit" || c == "exit" || c == "q") break;
        if (c == "clear" || c == "cls") {
            cout << "\x1b[2J\x1b[H" << std::flush;
            continue;
        }
        if (c == "home" || c == "close") {
            current_ = nullptr;
            continue;
        }
        if (c == "!") continue;
        dispatch(args);
        std::cout.flush();
    }
    cout << ui::dim("  bye! keep grinding.") << "\n";
    return 0;
}

}  // namespace leet
