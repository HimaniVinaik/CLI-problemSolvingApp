#include "core/history.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <sstream>

#include "util/fs.hpp"
#include "util/strings.hpp"

namespace leet {

History::History(const Config &cfg)
    : index_path_(fs::join(cfg.state_dir, "submissions.tsv")), dir_(fs::join(cfg.state_dir, "submissions")) {
    load();
}

void History::load() {
    all_.clear();
    auto c = fs::read_file(index_path_);
    if (!c) return;
    for (auto &line : str::split_lines(*c)) {
        if (line.empty() || line[0] == '#') continue;
        auto f = str::split(line, '\t');
        if (f.size() < 10) continue;
        Submission s;
        try {
            s.id = std::stoll(f[0]);
            s.time = std::stoll(f[2]);
            s.passed = std::stoi(f[4]);
            s.total = std::stoi(f[5]);
            s.max_ms = std::stod(f[6]);
        } catch (...) {
            continue;
        }
        s.key = f[1];
        s.verdict = f[3];
        s.time_class = f[7];
        s.space_class = f[8];
        s.snapshot = (!f[9].empty() && f[9][0] == '/') ? f[9] : fs::join(dir_, f[9]);
        all_.push_back(s);
    }
}

Submission History::add(const Problem &p, const std::string &source_path, const std::string &verdict, int passed,
                        int total, double max_ms, const std::string &tcls, const std::string &scls) {
    Submission s;
    s.id = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    if (!all_.empty() && s.id <= all_.back().id) s.id = all_.back().id + 1;
    s.key = p.key;
    s.time = (long long)std::time(nullptr);
    s.verdict = verdict;
    s.passed = passed;
    s.total = total;
    s.max_ms = max_ms;
    s.time_class = tcls.empty() ? "-" : tcls;
    s.space_class = scls.empty() ? "-" : scls;
    std::string rel = p.file_stem() + "/" + std::to_string(s.id) + "." + p.ext();   // relative: workspace can move
    s.snapshot = fs::join(dir_, rel);
    fs::write_file(s.snapshot, fs::read_file(source_path).value_or(""));

    std::ostringstream o;
    if (!fs::exists(index_path_)) o << "# id\tkey\ttime\tverdict\tpassed\ttotal\tmax_ms\ttime_class\tspace_class\tsnapshot\n";
    else o << fs::read_file(index_path_).value_or("");
    o << s.id << '\t' << s.key << '\t' << s.time << '\t' << s.verdict << '\t' << s.passed << '\t' << s.total << '\t'
      << s.max_ms << '\t' << s.time_class << '\t' << s.space_class << '\t' << rel << '\n';
    fs::write_file(index_path_, o.str());
    all_.push_back(s);
    return s;
}

std::vector<Submission> History::for_problem(const std::string &key) const {
    std::vector<Submission> out;
    for (auto &s : all_)
        if (s.key == key) out.push_back(s);
    return out;
}

std::vector<Submission> History::recent(size_t limit) const {
    std::vector<Submission> out(all_.rbegin(), all_.rend());
    if (out.size() > limit) out.resize(limit);
    return out;
}

std::string format_when(long long t) {
    std::time_t tt = (std::time_t)t, now = std::time(nullptr);
    std::tm a = *std::localtime(&tt), b = *std::localtime(&now);
    char hm[16], full[32];
    std::strftime(hm, sizeof hm, "%H:%M", &a);
    std::strftime(full, sizeof full, "%Y-%m-%d %H:%M", &a);
    if (a.tm_year == b.tm_year && a.tm_yday == b.tm_yday) return std::string("today ") + hm;
    std::time_t y = now - 86400;
    std::tm c = *std::localtime(&y);
    if (a.tm_year == c.tm_year && a.tm_yday == c.tm_yday) return std::string("yesterday ") + hm;
    return full;
}

}  // namespace leet
