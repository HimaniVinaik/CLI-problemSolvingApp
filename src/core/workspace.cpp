#include "core/workspace.hpp"

#include <cstdlib>
#include <ctime>
#include <set>
#include <sstream>

#include "util/fs.hpp"
#include "util/strings.hpp"

namespace leet {

// ---------------------------------------------------------------------------
//  Workspace
// ---------------------------------------------------------------------------
std::string Workspace::solution_path(const Problem &p) const {
    return fs::join(fs::join(cfg_.workspace, p.lang), p.file_stem() + "." + p.ext());
}

bool Workspace::has_solution(const Problem &p) const { return fs::exists(solution_path(p)); }

std::string Workspace::starter(const Problem &p) const {
    std::ostringstream o;
    o << "/*\n";
    o << " * " << p.display_id() << ". " << p.title << "  [" << p.difficulty << "]  -  " << p.category << "\n";
    if (!p.url().empty()) o << " * " << p.url() << "\n";
    o << " *\n";
    o << " *   Read the statement :  leet show " << p.display_id() << "\n";
    o << " *   Run the examples   :  leet test " << p.display_id() << "\n";
    o << " *   Custom input       :  leet run " << p.display_id() << "\n";
    o << " *   Submit to the judge:  leet submit " << p.display_id() << "\n";
    o << " *   Stuck?             :  leet hint " << p.display_id() << "   /   leet solution " << p.display_id() << "\n";
    o << " *\n";
    if (!p.time.empty()) o << " * Goal: time " << p.time << ", extra space " << p.space << "\n";
    if (p.is_c())
        o << " * <stdio.h> <stdlib.h> <string.h> <stdbool.h> <stdint.h> <limits.h> <ctype.h>\n"
             " * are pre-included. Compiled as C11 with gcc -O2.\n";
    else
        o << " * Like on LeetCode, <bits/stdc++.h> and `using namespace std;` are already\n"
             " * available, as are ListNode / TreeNode. Compiled as C++17 with -O2.\n";
    o << " */\n\n";
    o << p.tmpl;
    return o.str();
}

std::string Workspace::ensure_solution(const Problem &p) const {
    std::string path = solution_path(p);
    if (!fs::exists(path)) fs::write_file(path, starter(p));
    return path;
}

std::string Workspace::reset_solution(const Problem &p) const {
    std::string path = solution_path(p);
    if (fs::exists(path)) {
        std::string backup = fs::join(fs::join(cfg_.state_dir, "backups"),
                                      p.file_stem() + "." + std::to_string(std::time(nullptr)) + "." + p.ext());
        if (auto c = fs::read_file(path)) fs::write_file(backup, *c);
    }
    fs::write_file(path, starter(p));
    return path;
}

bool Workspace::is_untouched(const Problem &p) const {
    auto c = fs::read_file(solution_path(p));
    return !c || *c == starter(p);
}

std::string Workspace::custom_input_path(const Problem &p) const {
    return fs::join(fs::join(cfg_.workspace, "inputs"), p.file_stem() + ".txt");
}

int Workspace::open_in_editor(const std::string &path) const {
    if (cfg_.editor.empty()) return -1;
    std::string quoted = "'" + str::replace_all(path, "'", "'\\''") + "'";
    return std::system((cfg_.editor + " " + quoted).c_str());
}

// ---------------------------------------------------------------------------
//  Progress  (workspace/.leet/progress.tsv)
// ---------------------------------------------------------------------------
namespace {
std::set<std::string> g_days;

std::string today() {
    std::time_t t = std::time(nullptr);
    std::tm tm = *std::localtime(&t);
    char buf[16];
    std::strftime(buf, sizeof buf, "%Y%m%d", &tm);
    return buf;
}
}  // namespace

Progress::Progress(const Config &cfg) : path_(fs::join(cfg.state_dir, "progress.tsv")) {
    auto c = fs::read_file(path_);
    if (!c) return;
    for (auto &line : str::split_lines(*c)) {
        if (str::starts_with(line, "#days ")) {
            for (auto &d : str::split(line.substr(6), ',', false)) g_days.insert(d);
            continue;
        }
        if (line.empty() || line[0] == '#') continue;
        auto f = str::split(line, '\t');
        if (f.size() < 7) continue;
        ProgressEntry e;
        e.status = f[1];
        try {
            e.attempts = std::stoi(f[2]);
            e.last = std::stoll(f[3]);
            e.best_ms = std::stod(f[4]);
        } catch (...) {
        }
        e.time_class = f[5];
        e.space_class = f[6];
        data_[f[0]] = e;
    }
}

const ProgressEntry *Progress::get(const std::string &key) const {
    auto it = data_.find(key);
    return it == data_.end() ? nullptr : &it->second;
}

std::string Progress::status(const std::string &key) const {
    auto e = get(key);
    return e ? e->status : "";
}

void Progress::record(const std::string &key, bool accepted, double ms, const std::string &tcls,
                      const std::string &scls) {
    auto &e = data_[key];
    e.attempts++;
    e.last = (long long)std::time(nullptr);
    if (accepted) {
        e.status = "solved";
        if (e.best_ms == 0 || ms < e.best_ms) e.best_ms = ms;
        if (!tcls.empty()) e.time_class = tcls;
        if (!scls.empty()) e.space_class = scls;
        g_days.insert(today());
    } else if (e.status != "solved") {
        e.status = "attempted";
    }
    save();
}

void Progress::clear(const std::string &key) {
    data_.erase(key);
    save();
}

bool Progress::save() const {
    std::ostringstream o;
    o << "# leet progress: key status attempts last best_ms time_class space_class\n";
    std::vector<std::string> days(g_days.begin(), g_days.end());
    o << "#days " << str::join(days, ",") << "\n";
    for (auto &[k, e] : data_) {
        o << k << '\t' << e.status << '\t' << e.attempts << '\t' << e.last << '\t' << e.best_ms << '\t'
          << (e.time_class.empty() ? "-" : e.time_class) << '\t' << (e.space_class.empty() ? "-" : e.space_class)
          << '\n';
    }
    return fs::write_file(path_, o.str());
}

int Progress::streak_days() const {
    // count consecutive days ending today (or yesterday) with an accepted run
    std::time_t t = std::time(nullptr);
    int streak = 0;
    for (int back = 0; back < 3650; ++back) {
        std::time_t d = t - (std::time_t)back * 86400;
        std::tm tm = *std::localtime(&d);
        char buf[16];
        std::strftime(buf, sizeof buf, "%Y%m%d", &tm);
        if (g_days.count(buf)) ++streak;
        else if (back == 0) continue;  // today not done yet: streak may still be alive
        else break;
    }
    return streak;
}

}  // namespace leet
