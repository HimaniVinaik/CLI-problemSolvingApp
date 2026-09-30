// Submission history and manual progress marking:
//   leet history [id] [n]     leet restore <id> [n]     leet done <id>     leet undone <id>
#include <algorithm>
#include <iostream>

#include "app/app.hpp"
#include "ui/render.hpp"
#include "ui/term.hpp"
#include "util/fs.hpp"
#include "util/strings.hpp"

namespace leet {

using std::cout;

namespace {
bool is_number(const std::string &s) { return !s.empty() && std::all_of(s.begin(), s.end(), ::isdigit); }

std::string verdict_badge(const Submission &s) {
    std::string sym = s.accepted() ? ui::sym().check : ui::sym().cross;
    return s.accepted() ? ui::ok(sym + " " + s.verdict) : ui::err(sym + " " + s.verdict);
}

std::string cx(const std::string &c) { return c == "-" || c.empty() ? ui::dim("—") : complexity_label(c); }
}  // namespace

int App::cmd_history(const Args &a) {
    // `leet history` without a problem (and no current problem): recent submissions everywhere
    if (a.pos.empty() && (!current_ || a.has("all"))) {
        auto subs = history_->recent(a.has("all") ? 1000000 : 20);
        cout << "\n" << ui::rule("Recent submissions") << "\n\n";
        if (subs.empty()) {
            cout << "  " << ui::dim("No submissions yet. Solve something and run ") << ui::code("leet submit <id>") << "\n\n";
            return 0;
        }
        ui::Table t;
        t.column("when");
        t.column("problem", ui::Table::Left, 40);
        t.column("verdict");
        t.column("tests", ui::Table::Right);
        t.column("time");
        for (auto &s : subs) {
            const Problem *p = repo_.find(s.key);
            std::string name = p ? ui::accent2(p->display_id()) + " " + p->title : s.key;
            t.row({ui::dim(format_when(s.time)), name, verdict_badge(s),
                   std::to_string(s.passed) + "/" + std::to_string(s.total), cx(s.time_class)});
        }
        cout << t.render(4);
        cout << "\n  " << ui::dim(std::to_string(history_->size()) + " submissions in total.  Details: ")
             << ui::code("leet history <id>") << "\n\n";
        return 0;
    }

    // `leet history <id> [n]`
    Args b = a;
    int pick = 0;
    if (b.pos.size() >= 2 && is_number(b.pos.back())) {
        pick = std::stoi(b.pos.back());
        b.pos.pop_back();
    }
    const Problem *p = resolve(b);
    if (!p) return 1;
    auto subs = history_->for_problem(p->key);
    if (subs.empty()) {
        cout << "  " << ui::dim("No submissions for " + p->display_id() + ". " + p->title + " yet.") << "\n";
        return 0;
    }

    if (pick > 0) {   // show the code of one submission
        if (pick > (int)subs.size()) {
            std::cerr << ui::err("There is no submission #" + std::to_string(pick)) << ui::dim("  (" + std::to_string(subs.size()) + " saved)") << "\n";
            return 1;
        }
        const Submission &s = subs[pick - 1];
        auto code = fs::read_file(s.snapshot);
        cout << "\n" << ui::rule(p->display_id() + ". " + p->title + ui::dim("  submission #" + std::to_string(pick))) << "\n\n";
        cout << "  " << verdict_badge(s) << ui::dim("   " + std::to_string(s.passed) + "/" + std::to_string(s.total) + " tests   " +
                                                     format_when(s.time))
             << ui::dim("   time ") << cx(s.time_class) << ui::dim("   space ") << cx(s.space_class) << "\n\n";
        if (!code) {
            cout << "  " << ui::err("The saved code is missing: ") << s.snapshot << "\n";
            return 1;
        }
        cout << ui::code_block(*code, p->ext());
        cout << "\n  " << ui::dim("Put it back into your file with ") << ui::code("leet restore " + p->display_id() + " " + std::to_string(pick))
             << "\n\n";
        return 0;
    }

    cout << "\n" << ui::rule(p->display_id() + ". " + p->title + ui::dim("  submissions")) << "\n\n";
    ui::Table t;
    t.column("#", ui::Table::Right);
    t.column("when");
    t.column("verdict");
    t.column("tests", ui::Table::Right);
    t.column("slowest", ui::Table::Right);
    t.column("time");
    t.column("space");
    for (size_t i = 0; i < subs.size(); ++i) {
        auto &s = subs[i];
        t.row({ui::accent2(std::to_string(i + 1)), ui::dim(format_when(s.time)), verdict_badge(s),
               std::to_string(s.passed) + "/" + std::to_string(s.total), s.max_ms > 0 ? str::format_ns(s.max_ms * 1e6) : ui::dim("—"),
               cx(s.time_class), cx(s.space_class)});
    }
    cout << t.render(4);
    int acc = (int)std::count_if(subs.begin(), subs.end(), [](const Submission &s) { return s.accepted(); });
    cout << "\n  " << ui::dim(std::to_string(subs.size()) + " submissions, " + std::to_string(acc) + " accepted.") << "\n";
    view_next_steps({{"leet history " + p->display_id() + " <n>", "view the code of submission n"},
                     {"leet restore " + p->display_id() + " <n>", "put submission n back into your file"}});
    cout << "\n";
    return 0;
}

int App::cmd_restore(const Args &a) {
    Args b = a;
    int pick = 0;
    if (b.pos.size() >= 2 && is_number(b.pos.back())) {
        pick = std::stoi(b.pos.back());
        b.pos.pop_back();
    }
    const Problem *p = resolve(b);
    if (!p) return 1;
    auto subs = history_->for_problem(p->key);
    if (subs.empty()) {
        cout << "  " << ui::dim("No saved submissions for " + p->display_id() + " yet.") << "\n";
        return 1;
    }
    if (pick == 0) {   // default: the latest accepted submission, else the latest one
        pick = (int)subs.size();
        for (int i = (int)subs.size(); i >= 1; --i)
            if (subs[i - 1].accepted()) { pick = i; break; }
    }
    if (pick < 1 || pick > (int)subs.size()) {
        std::cerr << ui::err("There is no submission #" + std::to_string(pick)) << "\n";
        return 1;
    }
    const Submission &s = subs[pick - 1];
    auto code = fs::read_file(s.snapshot);
    if (!code) {
        std::cerr << ui::err("The saved code is missing: ") << s.snapshot << "\n";
        return 1;
    }
    if (ws_->has_solution(*p) && !confirm("Replace your current file with submission #" + std::to_string(pick) + " (" +
                                              s.verdict + ", " + format_when(s.time) + ")?", true, a))
        return 1;
    std::string path = ws_->replace_solution(*p, *code);
    cout << "  " << ui::ok(std::string(ui::sym().check) + " Restored submission #" + std::to_string(pick)) << ui::dim(" into ")
         << fs::pretty(path) << ui::dim("  (previous file backed up in .leet/backups)") << "\n";
    return 0;
}

int App::cmd_mark(const Args &a, bool done) {
    const Problem *p = resolve(a);
    if (!p) return 1;
    std::string before = progress_->status(p->key);
    progress_->mark(p->key, done);
    if (done) {
        if (before == "solved")
            cout << "  " << ui::dim(p->display_id() + ". " + p->title + " was already solved.") << "\n";
        else
            cout << "  " << ui::ok(std::string(ui::sym().check) + " Marked " + p->display_id() + ". " + p->title + " as done")
                 << ui::dim("  (undo with `leet undone " + p->display_id() + "`)") << "\n";
    } else {
        cout << "  " << ui::warn(std::string(ui::sym().circle) + " Marked " + p->display_id() + ". " + p->title + " as not done")
             << ui::dim(before == "solved" ? "  (your submission history is kept)" : "") << "\n";
    }
    int solved = 0;
    for (auto &[k, e] : progress_->entries()) solved += e.status == "solved";
    cout << "  " << ui::dim(std::to_string(solved) + " / " + std::to_string(repo_.all().size()) + " solved") << "\n";
    return 0;
}

}  // namespace leet
