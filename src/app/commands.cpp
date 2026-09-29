// User-facing commands.
#include <unistd.h>

#include <algorithm>
#include <iostream>
#include <map>
#include <random>

#include "app/app.hpp"
#include "ui/render.hpp"
#include "ui/term.hpp"
#include "util/fs.hpp"
#include "util/strings.hpp"

namespace leet {

using namespace judge;
using std::cout;

namespace {
Filter make_filter(const Args &a) {
    Filter f;
    f.difficulty = a.get("difficulty");
    f.category = a.get("category");
    f.lang = a.get("lang");
    f.status = a.get("status");
    f.search = a.get("search");
    if (a.command != "random" && a.command != "pick" && !a.pos.empty() && f.search.empty()) f.search = str::join(a.pos, " ");
    // convenience: `leet list easy`, `leet list c`
    std::string s = str::lower(f.search);
    if (s == "easy" || s == "medium" || s == "hard") { f.difficulty = s; f.search.clear(); }
    if (s == "c" || s == "cpp") { f.lang = s; f.search.clear(); }
    if (s == "solved" || s == "todo" || s == "attempted") { f.status = s; f.search.clear(); }
    return f;
}
}  // namespace

// ---------------------------------------------------------------------------
int App::cmd_list(const Args &a) {
    Filter f = make_filter(a);
    auto items = repo_.filter(f);
    if (!f.status.empty()) {
        std::string want = str::lower(f.status);
        items.erase(std::remove_if(items.begin(), items.end(), [&](const Problem *p) {
                        std::string s = progress_->status(p->key);
                        if (want == "todo" || want == "unsolved") return s == "solved";
                        return s != want;
                    }), items.end());
    }
    if (items.empty()) {
        cout << "  " << ui::dim("No problems match those filters.") << "\n";
        return 1;
    }
    bool grouped = !a.has("flat");
    int title_w = std::max(24, ui::width() - 40);
    std::string cat;
    ui::Table t;
    auto flush = [&] {
        if (t.size()) cout << t.render(4) << "\n";
        t = ui::Table();
    };
    auto new_table = [&] {
        t.column(" ", ui::Table::Left);
        t.column("id", ui::Table::Right);
        t.column("title", ui::Table::Left, title_w);
        t.column("difficulty", ui::Table::Left);
        if (!grouped) t.column("topic", ui::Table::Left, 22);
    };
    cout << "\n";
    if (!grouped) new_table();
    int solved = 0;
    for (auto *p : items) {
        if (progress_->status(p->key) == "solved") ++solved;
        if (grouped && p->category != cat) {
            flush();
            cat = p->category;
            int n = 0, s = 0;
            for (auto *q : items)
                if (q->category == cat) { ++n; s += progress_->status(q->key) == "solved"; }
            cout << ui::rule(cat + ui::dim("  " + std::to_string(s) + "/" + std::to_string(n))) << "\n";
            new_table();
        }
        std::vector<std::string> row = {status_icon(*p), ui::accent2(p->display_id()), p->title, ui::difficulty(p->difficulty)};
        if (!grouped) row.push_back(ui::dim(p->category));
        t.row(row);
    }
    flush();
    cout << "  " << ui::dim(std::to_string(items.size()) + " problems  " + ui::sym().dot + "  " + std::to_string(solved) +
                            " solved  " + ui::sym().dot + "  ")
         << ui::ok(ui::sym().check) << ui::dim(" solved  ") << ui::warn(ui::sym().half) << ui::dim(" attempted  ")
         << ui::info(ui::sym().circle) << ui::dim(" in progress") << "\n";
    cout << "  " << ui::dim("Open one with ") << ui::code("leet show <id>") << "\n\n";
    return 0;
}

int App::cmd_categories(const Args &) {
    cout << "\n" << ui::rule("Topics") << "\n\n";
    for (auto &c : repo_.categories()) {
        int n = 0, s = 0;
        for (auto &p : repo_.all())
            if (p.category == c) { ++n; s += progress_->status(p.key) == "solved"; }
        cout << "    " << str::pad_right(c, 26) << ui::progress_bar(n ? (double)s / n : 0, 20) << "  "
             << ui::dim(str::pad_left(std::to_string(s), 3) + " / " + std::to_string(n)) << "\n";
    }
    cout << "\n  " << ui::dim("List a topic with ") << ui::code("leet list -c \"<topic>\"") << "\n\n";
    return 0;
}

// ---------------------------------------------------------------------------
int App::cmd_show(const Args &a) {
    const Problem *p = resolve(a);
    if (!p) return 1;
    view_statement(*p);
    view_next_steps({{"leet edit " + p->display_id(), "write your solution"},
                     {"leet test " + p->display_id(), "run the examples"},
                     {"leet submit " + p->display_id(), "full judge + complexity"}});
    cout << "\n";
    return 0;
}

int App::cmd_edit(const Args &a) {
    const Problem *p = resolve(a);
    if (!p) return 1;
    bool fresh = !ws_->has_solution(*p);
    std::string path = ws_->ensure_solution(*p);
    if (fresh) cout << "  " << ui::ok(std::string(ui::sym().check) + " Created ") << fs::pretty(path) << "\n";
    if (a.has("no-open") || cfg_.editor.empty() || !::isatty(STDIN_FILENO)) {
        cout << "  " << ui::dim("Solution file: ") << path << "\n";
        if (cfg_.editor.empty()) cout << "  " << ui::dim("Set $EDITOR to open it automatically.") << "\n";
        return 0;
    }
    int rc = ws_->open_in_editor(path);
    if (rc != 0) {
        cout << "  " << ui::warn("Editor exited with an error") << ui::dim(" (" + cfg_.editor + ")") << "\n";
        return 1;
    }
    view_next_steps({{"leet test " + p->display_id(), "run the examples"},
                     {"leet submit " + p->display_id(), "full judge + complexity"}});
    return 0;
}

int App::cmd_path(const Args &a) {
    const Problem *p = resolve(a);
    if (!p) return 1;
    cout << ws_->ensure_solution(*p) << "\n";
    return 0;
}

int App::cmd_reset(const Args &a) {
    const Problem *p = resolve(a);
    if (!p) return 1;
    if (ws_->has_solution(*p) && !confirm("Replace your solution for " + p->title + " with the template?", false, a))
        return 1;
    std::string path = ws_->reset_solution(*p);
    cout << "  " << ui::ok(std::string(ui::sym().check) + " Reset ") << fs::pretty(path)
         << ui::dim("  (old version backed up in .leet/backups)") << "\n";
    return 0;
}

// Checks shared by test / run / submit.
static bool ready_to_judge(const Problem &p, const Workspace &ws, std::string &path) {
    if (!ws.has_solution(p)) {
        path = ws.ensure_solution(p);
        cout << "\n  " << ui::warn("No solution yet.") << " Created the starter file:\n    " << fs::pretty(path) << "\n";
        cout << "  " << ui::dim("Write your code there, or run ") << ui::code("leet edit " + p.display_id()) << "\n\n";
        return false;
    }
    path = ws.solution_path(p);
    return true;
}

int App::cmd_test(const Args &a) {
    const Problem *p = resolve(a);
    if (!p) return 1;
    std::string path;
    if (!ready_to_judge(*p, *ws_, path)) return 1;
    Options o;
    o.examples_only = true;
    o.sanitize = a.has("sanitize");
    ui::Spinner sp("Compiling …");
    o.status = [&](const std::string &s) { sp.set_label(s); };
    Report r = judge_->submit(*p, path, o);
    sp.stop();
    cout << "\n" << ui::rule(p->display_id() + ". " + p->title + ui::dim("  examples" + std::string(o.sanitize ? "  (sanitized build)" : ""))) << "\n";
    if (r.verdict == Verdict::CompileError || r.verdict == Verdict::JudgeError) {
        view_report(*p, r);
        return 1;
    }
    if (!r.build.log.empty()) {
        cout << "\n  " << ui::warn("Compiler warnings") << "\n";
        for (auto &l : str::split_lines(r.build.log)) cout << "    " << l << "\n";
    }
    int pass = 0;
    cout << "\n";
    for (auto &c : r.examples) {
        bool ok = c.verdict == Verdict::Accepted;
        pass += ok;
        cout << "  " << (ok ? ui::ok(ui::sym().check) : ui::err(ui::sym().cross)) << " " << ui::bold("Example " + std::to_string(c.index))
             << "  " << (ok ? ui::dim(str::format_ns(c.ms * 1e6)) : ui::err(verdict_name(c.verdict))) << "\n";
        if (!ok || a.has("verbose")) {
            view_case(*p, c);
            cout << "\n";
        } else if (!str::trim(c.user_stdout).empty()) {
            auto lines = str::split_lines(c.user_stdout);
            for (size_t i = 0; i < lines.size() && i < 6; ++i) cout << "      " << ui::dim("stdout ") << lines[i] << "\n";
        }
    }
    size_t total = r.examples.size();
    cout << "\n  ";
    if (pass == (int)total) {
        cout << ui::ok("All " + std::to_string(total) + " examples passed.") << ui::dim(" Hidden and randomized tests run on submit.") << "\n";
        view_next_steps({{"leet submit " + p->display_id(), "full judge + complexity"}});
    } else {
        cout << ui::err(std::to_string(pass) + " / " + std::to_string(total) + " examples passed.") << "\n";
        view_next_steps({{"leet run " + p->display_id(), "try your own input"}, {"leet hint " + p->display_id(), "get a nudge"}});
        if (!progress_->get(p->key) || progress_->status(p->key) != "solved") progress_->record(p->key, false, 0, "", "");
    }
    cout << "\n";
    return pass == (int)total ? 0 : 1;
}

int App::cmd_submit(const Args &a) {
    const Problem *p = resolve(a);
    if (!p) return 1;
    std::string path;
    if (!ready_to_judge(*p, *ws_, path)) return 1;
    if (ws_->is_untouched(*p)) {
        cout << "  " << ui::warn("Your file is still the untouched template.") << ui::dim(" Write a solution first: ")
             << ui::code("leet edit " + p->display_id()) << "\n";
        return 1;
    }
    Options o;
    o.sanitize = a.has("sanitize");
    o.bench = !a.has("no-bench");
    o.stress = !a.has("no-random");
    ui::Spinner sp("Compiling …");
    o.status = [&](const std::string &s) { sp.set_label(s); };
    Report r = judge_->submit(*p, path, o);
    sp.stop();
    cout << "\n" << ui::rule(p->display_id() + ". " + p->title + ui::dim("  submission")) << "\n";
    view_report(*p, r);
    if (r.verdict == Verdict::Accepted) {
        if (o.bench) view_bench(*p, r.bench);
        bool first = progress_->status(p->key) != "solved";
        progress_->record(p->key, true, r.max_ms, r.bench.ran ? r.bench.time.cls : "", r.bench.ran ? r.bench.space.cls : "");
        int solved = 0;
        for (auto &[k, e] : progress_->entries()) solved += e.status == "solved";
        cout << "\n  " << (first ? ui::ok(std::string(ui::sym().star) + " New problem solved!") + "  " : std::string())
             << ui::dim(std::to_string(solved) + " / " + std::to_string(repo_.all().size()) + " solved") << "\n";
        const Problem *nx = repo_.next(*p, 1);
        std::vector<std::pair<std::string, std::string>> steps = {{"leet solution " + p->display_id(), "compare with the commented reference"}};
        if (nx) steps.push_back({"leet next", "go to " + nx->display_id() + ". " + nx->title});
        view_next_steps(steps);
    } else if (r.verdict != Verdict::JudgeError) {
        progress_->record(p->key, false, 0, "", "");
        view_next_steps({{"leet run " + p->display_id() + " -i <file>", "reproduce with your own input"},
                         {"leet hint " + p->display_id(), "get a nudge"}});
    }
    cout << "\n";
    return r.verdict == Verdict::Accepted ? 0 : 1;
}

int App::cmd_analyze(const Args &a) {
    const Problem *p = resolve(a);
    if (!p) return 1;
    std::string path;
    if (!ready_to_judge(*p, *ws_, path)) return 1;
    Options o;
    ui::Spinner sp("Compiling …");
    o.status = [&](const std::string &s) { sp.set_label(s); };
    Report r = judge_->analyze(*p, path, o);
    sp.stop();
    if (r.verdict == Verdict::CompileError) { view_compile_error(*p, r.build); return 1; }
    cout << "\n" << ui::rule(p->display_id() + ". " + p->title) << "\n";
    view_bench(*p, r.bench);
    cout << "\n  " << ui::dim("Note: analyze does not check correctness; use `leet submit` for that.") << "\n\n";
    return 0;
}

// ---------------------------------------------------------------------------
int App::cmd_run(const Args &a) {
    const Problem *p = resolve(a);
    if (!p) return 1;
    std::string path;
    if (!ready_to_judge(*p, *ws_, path)) return 1;

    std::vector<std::string> input;
    std::string src = a.get("input");
    const TestCase *ex = nullptr;
    for (auto &t : p->tests)
        if (t.example) { ex = &t; break; }
    if (!src.empty()) {
        auto c = fs::read_file(src);
        if (!c) { std::cerr << ui::err("Cannot read ") << src << "\n"; return 1; }
        for (auto &l : str::split_lines(*c))
            if (!str::trim(l).empty()) input.push_back(str::trim(l));
    } else if (!::isatty(STDIN_FILENO)) {
        std::string l;
        while (std::getline(std::cin, l))
            if (!str::trim(l).empty()) input.push_back(str::trim(l));
    } else {
        size_t n = ex ? ex->args.size() : p->params.size();
        cout << "\n  " << ui::bold("Custom input") << ui::dim("  one value per argument, LeetCode syntax; Enter keeps the example value") << "\n";
        for (size_t i = 0; i < n; ++i) {
            std::string name = i < p->params.size() ? p->params[i] : "arg" + std::to_string(i + 1);
            std::string def = ex && i < ex->args.size() ? ex->args[i] : "";
            cout << "  " << ui::accent2(name) << ui::dim(def.empty() ? "" : " [" + str::truncate(def, 40) + "]") << ": " << std::flush;
            std::string line;
            if (!std::getline(std::cin, line)) return 1;
            line = str::trim(line);
            input.push_back(line.empty() ? def : line);
        }
    }
    if (input.empty()) { std::cerr << ui::err("No input given.") << "\n"; return 1; }

    ui::Spinner sp("Compiling and running …");
    Report r = judge_->run_custom(*p, path, input, a.has("sanitize"));
    sp.stop();
    cout << "\n" << ui::rule(p->display_id() + ". " + p->title + ui::dim("  custom run")) << "\n";
    if (r.verdict == Verdict::CompileError) { view_compile_error(*p, r.build); return 1; }
    const CaseResult &c = r.examples[0];
    cout << "\n";
    if (c.verdict == Verdict::JudgeError) {
        cout << "  " << ui::err("Invalid input: ") << c.detail << "\n";
        cout << "  " << ui::dim("Expected " + std::to_string(p->params.size()) + " line(s): " + str::join(p->params, ", ")) << "\n\n";
        return 1;
    }
    std::string head = c.verdict == Verdict::Accepted ? ui::ok(std::string(ui::sym().check) + " Matches the reference")
                       : c.verdict == Verdict::WrongAnswer ? ui::err(std::string(ui::sym().cross) + " Differs from the reference")
                                                           : ui::err(std::string(ui::sym().cross) + " " + verdict_name(c.verdict));
    if (c.expected.empty() && c.verdict == Verdict::Accepted) head = ui::dim("(reference output unavailable)");
    cout << "  " << head << ui::dim("   " + str::format_ns(c.ms * 1e6)) << "\n\n";
    view_case(*p, c);
    cout << "\n";
    return c.verdict == Verdict::Accepted ? 0 : 1;
}

// ---------------------------------------------------------------------------
int App::cmd_solution(const Args &a) {
    const Problem *p = resolve(a);
    if (!p) return 1;
    if (progress_->status(p->key) != "solved" &&
        !confirm(ui::warn("You haven't solved this one yet.") + " Show the solution anyway?", false, a)) {
        cout << "  " << ui::dim("Try ") << ui::code("leet hint " + p->display_id()) << ui::dim(" first.") << "\n";
        return 1;
    }
    view_header(*p);
    if (!p->approach.empty()) {
        cout << "\n" << ui::rule("Approach") << "\n\n" << ui::markdown(p->approach, ui::width(), 2) << "\n";
    }
    cout << "\n" << ui::rule("Reference solution" + ui::dim("  (" + std::string(p->is_c() ? "C" : "C++") + ")")) << "\n\n";
    cout << ui::code_block(p->solution, p->ext(), "");
    cout << "\n  " << ui::bold("Complexity") << "  " << ui::dim("time ") << ui::accent2(p->time) << ui::dim("   extra space ")
         << ui::accent2(p->space) << "\n\n";
    return 0;
}

int App::cmd_hint(const Args &a) {
    Args b = a;
    int k = 1;
    // `leet hint 88 2` -> second hint
    if (b.pos.size() >= 2 && std::all_of(b.pos.back().begin(), b.pos.back().end(), ::isdigit)) {
        k = std::stoi(b.pos.back());
        b.pos.pop_back();
    } else if (b.pos.size() == 1 && current_ && std::all_of(b.pos[0].begin(), b.pos[0].end(), ::isdigit) &&
               std::stoi(b.pos[0]) <= 5 && in_shell_) {
        k = std::stoi(b.pos[0]);
        b.pos.clear();
    }
    const Problem *p = resolve(b);
    if (!p) return 1;
    if (p->hints.empty()) {
        cout << "  " << ui::dim("No hints for this problem.") << "\n";
        return 0;
    }
    bool all = a.has("all");
    k = std::clamp(k, 1, (int)p->hints.size());
    cout << "\n";
    for (int i = all ? 1 : k; i <= (all ? (int)p->hints.size() : k); ++i) {
        std::string title = "Hint " + std::to_string(i) + " of " + std::to_string(p->hints.size());
        auto md = str::split_lines(ui::markdown(p->hints[i - 1], ui::width() - 6, 0));
        cout << ui::box(md, title, "38;5;141");
    }
    if (!all && k < (int)p->hints.size())
        cout << "  " << ui::dim("Next hint: ") << ui::code("leet hint " + p->display_id() + " " + std::to_string(k + 1)) << "\n";
    cout << "\n";
    return 0;
}

// ---------------------------------------------------------------------------
int App::cmd_stats(const Args &) {
    const auto &all = repo_.all();
    int solved = 0, attempted = 0;
    std::map<std::string, std::pair<int, int>> diff, lang;
    for (auto &p : all) {
        std::string s = progress_->status(p.key);
        bool ok = s == "solved";
        solved += ok;
        attempted += s == "attempted";
        diff[p.difficulty].second++;
        diff[p.difficulty].first += ok;
        lang[p.lang].second++;
        lang[p.lang].first += ok;
    }
    cout << "\n" << ui::rule("Your progress") << "\n\n";
    double frac = all.empty() ? 0 : (double)solved / all.size();
    char pct[16];
    std::snprintf(pct, sizeof pct, "%3.0f%%", frac * 100);
    cout << "    " << ui::bold("Solved ") << ui::accent(std::to_string(solved)) << ui::dim(" / " + std::to_string(all.size()))
         << "   " << ui::progress_bar(frac, 30) << " " << pct;
    int streak = progress_->streak_days();
    if (streak > 0) cout << "   " << ui::warn(std::to_string(streak) + "-day streak");
    cout << "\n";
    if (attempted) cout << "    " << ui::dim(std::to_string(attempted) + " attempted but not yet accepted") << "\n";
    cout << "\n";
    for (const char *d : {"Easy", "Medium", "Hard"}) {
        auto [s, n] = diff[d];
        cout << "    " << str::pad_right(ui::difficulty(d), 8) << " " << ui::progress_bar(n ? (double)s / n : 0, 24, ui::difficulty_color(d))
             << "  " << ui::dim(str::pad_left(std::to_string(s), 3) + " / " + std::to_string(n)) << "\n";
    }
    cout << "\n";
    cout << "    " << str::pad_right("C++ (Top 150)", 14) << ui::progress_bar(lang["cpp"].second ? (double)lang["cpp"].first / lang["cpp"].second : 0, 18)
         << "  " << ui::dim(std::to_string(lang["cpp"].first) + " / " + std::to_string(lang["cpp"].second)) << "\n";
    cout << "    " << str::pad_right("C classics", 14) << ui::progress_bar(lang["c"].second ? (double)lang["c"].first / lang["c"].second : 0, 18)
         << "  " << ui::dim(std::to_string(lang["c"].first) + " / " + std::to_string(lang["c"].second)) << "\n";

    cout << "\n" << ui::rule("By topic") << "\n\n";
    for (auto &c : repo_.categories()) {
        int n = 0, s = 0;
        for (auto &p : all)
            if (p.category == c) { ++n; s += progress_->status(p.key) == "solved"; }
        cout << "    " << str::pad_right(c, 26) << ui::progress_bar(n ? (double)s / n : 0, 20) << "  "
             << ui::dim(str::pad_left(std::to_string(s), 3) + " / " + std::to_string(n)) << "\n";
    }
    // recent
    std::vector<std::pair<long long, const Problem *>> recent;
    for (auto &p : all)
        if (auto e = progress_->get(p.key); e && e->status == "solved") recent.push_back({e->last, &p});
    std::sort(recent.rbegin(), recent.rend());
    if (!recent.empty()) {
        cout << "\n" << ui::rule("Recently solved") << "\n\n";
        for (size_t i = 0; i < recent.size() && i < 5; ++i) {
            auto *p = recent[i].second;
            auto e = progress_->get(p->key);
            std::string cx = e->time_class.empty() || e->time_class == "-" ? "" : "  time " + complexity_label(e->time_class);
            cout << "    " << ui::ok(ui::sym().check) << " " << ui::accent2(str::pad_left(p->display_id(), 4)) << "  "
                 << str::pad_right(p->title, 44) << ui::dim(cx) << "\n";
        }
    }
    // suggestion
    for (auto &p : all)
        if (progress_->status(p.key) != "solved") {
            cout << "\n  " << ui::dim("Up next: ") << ui::accent2(p.display_id() + ". " + p.title) << ui::dim("  ") << ui::code("leet show " + p.display_id()) << "\n";
            break;
        }
    cout << "\n";
    return 0;
}

int App::cmd_random(const Args &a) {
    Filter f = make_filter(a);
    std::vector<const Problem *> pool;
    for (auto *p : repo_.filter(f))
        if (progress_->status(p->key) != "solved") pool.push_back(p);
    if (pool.empty()) {
        cout << "  " << ui::ok("Nothing left to solve with those filters. Impressive!") << "\n";
        return 0;
    }
    std::mt19937 rng(std::random_device{}());
    const Problem *p = pool[std::uniform_int_distribution<size_t>(0, pool.size() - 1)(rng)];
    current_ = p;
    Args s;
    s.command = "show";
    s.pos = {p->key};
    return cmd_show(s);
}

int App::cmd_next(const Args &a, int dir) {
    const Problem *base = current_;
    if (!a.pos.empty()) base = repo_.find(a.pos[0]);
    const Problem *nx = nullptr;
    if (!base) {
        // first unsolved
        for (auto &p : repo_.all())
            if (progress_->status(p.key) != "solved") { nx = &p; break; }
    } else {
        nx = repo_.next(*base, dir);
    }
    if (!nx) {
        cout << "  " << ui::dim(dir > 0 ? "That was the last problem." : "That was the first problem.") << "\n";
        return 1;
    }
    current_ = nx;
    Args s;
    s.command = "show";
    s.pos = {nx->key};
    return cmd_show(s);
}

}  // namespace leet
