// Rendering of problems, judge results and complexity reports.
#include <algorithm>
#include <cmath>
#include <iostream>

#include "app/app.hpp"
#include "ui/render.hpp"
#include "ui/term.hpp"
#include "util/fs.hpp"
#include "util/strings.hpp"

namespace leet {

using namespace judge;
using std::cout;

std::string App::status_icon(const Problem &p) const {
    std::string s = progress_->status(p.key);
    if (s == "solved") return ui::ok(ui::sym().check);
    if (s == "attempted") return ui::warn(ui::sym().half);
    if (ws_->has_solution(p) && !ws_->is_untouched(p)) return ui::info(ui::sym().circle);
    return ui::dim(ui::sym().dot);
}

void App::view_header(const Problem &p) const {
    int inner = ui::width() - 4;
    std::string left = ui::accent(p.display_id() + ". " + p.title);
    std::string right = ui::bold(ui::difficulty(p.difficulty));
    std::string line1 = left + std::string(std::max(1, inner - str::display_width(left) - str::display_width(right)), ' ') + right;
    std::string meta = ui::accent2(p.category);
    if (!p.topics.empty()) meta += ui::dim("  " + std::string(ui::sym().dot) + "  " + str::join(p.topics, ", "));
    std::string lang = p.is_c() ? "solve in C" : "solve in C++";
    std::string line2 = meta + std::string(std::max(1, inner - str::display_width(meta) - (int)lang.size()), ' ') + ui::dim(lang);
    cout << "\n" << ui::box({line1, line2}, "", "38;5;240") ;
}

std::string App::format_input(const Problem &p, const std::vector<std::string> &args, int indent) const {
    std::string out;
    int avail = ui::width() - indent - 4;
    for (size_t i = 0; i < args.size(); ++i) {
        std::string name = i < p.params.size() ? p.params[i] : "arg" + std::to_string(i + 1);
        std::string v = args[i];
        std::string line = ui::dim(name + " = ") + ui::code(str::truncate(v, std::max(10, avail - (int)name.size() - 3)));
        out += (i ? std::string(indent, ' ') : "") + line + (i + 1 < args.size() ? "\n" : "");
    }
    return out;
}

void App::view_examples(const Problem &p) const {
    int k = 0;
    for (auto &t : p.tests) {
        if (!t.example) continue;
        ++k;
        cout << "  " << ui::bold("Example " + std::to_string(k)) << "\n";
        cout << "    " << ui::dim("Input   ") << format_input(p, t.args, 12) << "\n";
        cout << "    " << ui::dim("Output  ") << ui::code(str::truncate(t.expected, ui::width() - 14)) << "\n";
        if (!t.explanation.empty()) {
            std::string md = ui::markdown(t.explanation, ui::width() - 8, 0);
            auto lines = str::split_lines(md);
            for (size_t i = 0; i < lines.size(); ++i)
                cout << "    " << (i == 0 ? ui::dim("Why     ") : "        ") << ui::italic(lines[i]) << "\n";
        }
        cout << "\n";
    }
}

void App::view_statement(const Problem &p) const {
    view_header(p);
    cout << "\n" << ui::markdown(p.description, ui::width(), 2) << "\n\n";
    view_examples(p);
    if (!p.constraints.empty()) {
        cout << "  " << ui::bold("Constraints") << "\n";
        cout << ui::markdown(p.constraints, ui::width(), 4) << "\n\n";
    }
    if (!p.time.empty()) {
        cout << "  " << ui::bold("Goal") << "  " << ui::dim("time ") << ui::accent2(p.time) << ui::dim("   extra space ")
             << ui::accent2(p.space) << "\n";
    }
    std::string st = progress_->status(p.key);
    auto pe = progress_->get(p.key);
    std::string status = st == "solved" ? ui::ok(std::string(ui::sym().check) + " solved") + (pe && pe->manual ? ui::dim(" (marked by hand)") : "")
                         : st == "attempted" ? ui::warn(std::string(ui::sym().half) + " attempted")
                                             : ui::dim("not started");
    cout << "  " << ui::bold("Status") << ui::dim(" ") << status;
    if (!p.hints.empty()) cout << ui::dim("   " + std::to_string(p.hints.size()) + " hint(s) available");
    cout << "\n";
    if (ws_->has_solution(p)) cout << "  " << ui::bold("File") << "  " << ui::dim(fs::pretty(ws_->solution_path(p))) << "\n";
}

void App::view_next_steps(const std::vector<std::pair<std::string, std::string>> &steps) const {
    cout << "\n";
    int w = 20;
    for (auto &s : steps) w = std::max(w, (int)s.first.size() + 3);
    for (auto &[cmd, what] : steps)
        cout << "  " << ui::accent2(ui::sym().arrow) << " " << ui::code(str::pad_right(cmd, w)) << ui::dim(what) << "\n";
}

void App::view_case(const Problem &p, const CaseResult &c, bool show_expected) const {
    auto label = [](const std::string &s) { return ui::dim(str::pad_right(s, 10)); };
    std::string pad(6, ' ');
    if (!c.input.empty()) {
        std::string in = format_input(p, c.input, 16);
        // long generated inputs: show a preview
        cout << pad << label("Input") << in << "\n";
    }
    if (c.n > 0 && c.kind != "example" && c.kind != "test")
        cout << pad << label("Size") << "n = " << c.n << ui::dim("  (generated input)") << "\n";
    if (show_expected && !c.expected.empty())
        cout << pad << label("Expected") << ui::ok(str::truncate(c.expected, ui::width() - 18)) << "\n";
    if (c.verdict == Verdict::Accepted || c.verdict == Verdict::WrongAnswer) {
        std::string got = c.got.empty() ? ui::dim("(empty)") : str::truncate(c.got, ui::width() - 18);
        cout << pad << label("Output") << (c.verdict == Verdict::Accepted ? ui::ok(got) : ui::err(got)) << "\n";
    }
    if (!c.detail.empty()) cout << pad << label("Error") << ui::err(c.detail) << "\n";
    if (!str::trim(c.user_stdout).empty()) {
        auto lines = str::split_lines(c.user_stdout);
        cout << pad << label("Stdout") << ui::dim("(your prints)") << "\n";
        for (size_t i = 0; i < lines.size() && i < 15; ++i) cout << pad << "  " << lines[i] << "\n";
        if (lines.size() > 15) cout << pad << "  " << ui::dim("… " + std::to_string(lines.size() - 15) + " more lines") << "\n";
    }
    std::string se = str::trim(c.user_stderr);
    if (!se.empty() && c.verdict != Verdict::Accepted) {
        auto lines = str::split_lines(se);
        cout << pad << label("Stderr") << "\n";
        for (size_t i = 0; i < lines.size() && i < 25; ++i) cout << pad << "  " << ui::dim(lines[i]) << "\n";
    }
}

void App::view_compile_error(const Problem &p, const BuildResult &b) const {
    cout << "\n" << ui::box({ui::err(std::string(ui::sym().cross) + "  Compile Error")}, "", "38;5;203");
    auto lines = str::split_lines(b.log);
    size_t shown = 0;
    for (auto &l : lines) {
        if (shown++ >= 40) { cout << "  " << ui::dim("… (" + std::to_string(lines.size() - 40) + " more lines)") << "\n"; break; }
        cout << "  " << l << "\n";
    }
    if (b.log.find("[driver for") != std::string::npos) {
        cout << "\n  " << ui::warn(std::string(ui::sym().warn) + " The error is inside the judge driver.") << "\n";
        cout << "    " << ui::dim("This usually means the function/class name or signature was changed.") << "\n";
        cout << "    " << ui::dim("Keep the signature from the template:") << "\n";
        for (auto &l : ui::highlight(p.tmpl, p.ext())) cout << "      " << l << "\n";
    }
}

void App::view_report(const Problem &p, const Report &r) const {
    if (r.verdict == Verdict::CompileError) { view_compile_error(p, r.build); return; }
    if (!r.build.log.empty() && r.verdict != Verdict::CompileError) {
        auto lines = str::split_lines(r.build.log);
        cout << "\n  " << ui::warn("Compiler warnings") << "\n";
        for (size_t i = 0; i < lines.size() && i < 12; ++i) cout << "    " << lines[i] << "\n";
    }
    if (r.verdict == Verdict::JudgeError) {
        cout << "\n" << ui::box({ui::err("Judge error"), r.message.empty() && r.failure ? r.failure->detail : r.message}, "", "38;5;203");
        return;
    }
    if (r.verdict == Verdict::Accepted) {
        std::vector<std::string> lines;
        lines.push_back(ui::ok(std::string(ui::sym().check) + "  Accepted") + ui::dim("   " + std::to_string(r.passed()) + " / " +
                                                                                     std::to_string(r.total()) + " tests passed"));
        std::string breakdown = ui::dim("examples ") + std::to_string(r.examples.size()) + ui::dim("  ·  hidden ") +
                                std::to_string(r.tests_total) + ui::dim("  ·  randomized ") + std::to_string(r.random_total) +
                                ui::dim("  ·  large ") + (r.large_ran ? "1" : "0");
        lines.push_back(breakdown);
        lines.push_back(ui::dim("slowest call ") + str::format_ns(r.max_ms * 1e6) + ui::dim("   total ") +
                        str::format_ns(r.total_ms * 1e6));
        cout << "\n" << ui::box(lines, "", "38;5;78");
        return;
    }
    const CaseResult &f = *r.failure;
    std::string where = f.kind == "example" ? "Example " + std::to_string(f.index)
                        : f.kind == "test" ? "Hidden test " + std::to_string(f.index)
                        : f.kind == "random" ? "Randomized test " + std::to_string(f.index)
                        : f.kind == "large" ? "Large test" : "Custom input";
    std::string head = ui::err(std::string(ui::sym().cross) + "  " + verdict_name(r.verdict)) + ui::dim("   on " + where) +
                       ui::dim("   (" + std::to_string(r.passed()) + " passed before it)");
    cout << "\n" << ui::box({head}, "", "38;5;203");
    if (f.kind == "large" && f.verdict == Verdict::TimeLimit) {
        cout << "      " << ui::warn("Your solution is too slow for the largest input size (n = " + str::format_count(f.n) + ").") << "\n";
        cout << "      " << ui::dim("Aim for " + p.time + ". Try `leet analyze " + p.display_id() + "` to see how it scales.") << "\n";
        return;
    }
    view_case(p, f);
    if (f.verdict == Verdict::RuntimeError)
        cout << "\n      " << ui::dim("Tip: `leet test " + p.display_id() + " --sanitize` pinpoints the faulting line (AddressSanitizer).") << "\n";
}

// ---------------------------------------------------------------------------
void App::view_bench(const Problem &p, const BenchReport &b) const {
    cout << "\n" << ui::rule("Complexity analysis") << "\n\n";
    if (!b.ran) {
        cout << "  " << ui::dim("Skipped: " + (b.skipped.empty() ? std::string("no data") : b.skipped)) << "\n";
        if (!p.time.empty())
            cout << "  " << ui::dim("Expected: time ") << p.time << ui::dim(", extra space ") << p.space << "\n";
        return;
    }
    // table
    bool counted = !b.user.empty() && b.user.front().ops > 0;
    ui::Table t;
    t.column("n", ui::Table::Right);
    t.column("time / call", ui::Table::Right);
    t.column("", ui::Table::Left);
    t.column("reference", ui::Table::Right);
    if (counted) t.column("operations", ui::Table::Right);
    t.column("memory", ui::Table::Right);
    t.column("(stack)", ui::Table::Right);
    // bar length follows the growth of the operation count (or time)
    auto metric = [&](const BenchPoint &x) { return counted ? (double)x.ops : std::max(x.ns, 1.0); };
    double lo = 1e300, hi = 0;
    for (auto &pt : b.user) { lo = std::min(lo, metric(pt)); hi = std::max(hi, metric(pt)); }
    for (auto &pt : b.user) {
        double frac = hi > lo ? (std::log(metric(pt)) - std::log(lo)) / (std::log(hi) - std::log(lo)) : 0;
        int cells = 1 + (int)std::lround(frac * 14);
        std::string bar;
        for (int i = 0; i < cells; ++i) bar += ui::sym().bar_full;
        std::string ref = "";
        for (auto &r : b.ref)
            if (r.n == pt.n) ref = str::format_ns(r.ns);
        std::vector<std::string> row = {str::format_count(pt.n), str::format_ns(pt.ns), ui::style("38;5;45", bar), ui::dim(ref)};
        if (counted) row.push_back(str::format_count(pt.ops));
        row.push_back(str::format_bytes((double)pt.memory()));
        row.push_back(ui::dim(str::format_bytes((double)pt.stack)));
        t.row(row);
    }
    cout << t.render(4);
    if (!b.stopped.empty()) cout << "    " << ui::dim(b.stopped) << "\n";
    if (!p.bench.unit.empty()) cout << "    " << ui::dim("n = " + p.bench.unit) << "\n";
    cout << "\n";

    auto verdict_line = [&](const std::string &what, const Fit &est, const std::string &exp_cls, const std::string &exp_display,
                            bool precise) {
        std::string e = est.ok() ? complexity_label(est.cls) : "?";
        cout << "  " << ui::bold(str::pad_right(what, 6)) << " " << ui::accent(str::pad_right(e, 12)) << ui::dim("goal ")
             << str::pad_right(exp_display.empty() ? complexity_label(exp_cls) : exp_display, 14);
        if (exp_cls.empty() || exp_cls == "-") { cout << "\n"; return; }
        switch (compare_class(exp_cls, est.cls, precise)) {
            case Match::Optimal: cout << ui::ok(std::string(ui::sym().check) + " optimal"); break;
            case Match::Close: cout << ui::ok(std::string(ui::sym().check) + " matches") << ui::dim(" (within measurement noise)"); break;
            case Match::Worse: cout << ui::warn(std::string(ui::sym().warn) + " grows faster than the optimal " + complexity_label(exp_cls)); break;
            default: cout << ui::dim("unknown"); break;
        }
        cout << "\n";
    };
    verdict_line("Time", b.time, p.bench.time_class, p.time, b.basis == "operation count");
    verdict_line("Space", b.space, p.bench.space_class, p.space, true);

    // speed relative to the reference at the largest common n
    for (auto it = b.user.rbegin(); it != b.user.rend(); ++it) {
        auto r = std::find_if(b.ref.begin(), b.ref.end(), [&](const BenchPoint &x) { return x.n == it->n; });
        if (r == b.ref.end() || r->ns <= 0) continue;
        double ratio = it->ns / r->ns;
        char buf[64];
        std::snprintf(buf, sizeof buf, "%.2fx", ratio);
        std::string msg = ratio <= 1.0 ? ui::ok(std::string(buf)) + ui::dim(" the reference time (faster or equal)")
                        : ratio < 3 ? std::string(buf) + ui::dim(" the reference time")
                                    : ui::warn(std::string(buf)) + ui::dim(" the reference time (room to optimize)");
        cout << "  " << ui::bold("Speed ") << " " << msg << ui::dim("  at n = " + str::format_count(it->n)) << "\n";
        break;
    }
    cout << "\n  " << ui::dim("Time class is estimated from the " + b.basis + " on growing random inputs;") << "\n";
    if (counted)
        cout << "  " << ui::dim("operations = basic blocks your code executed (deterministic, unaffected by CPU caches).") << "\n";
    cout << "  " << ui::dim("Memory = peak heap (including the returned value) + stack depth during the call.") << "\n";
}

}  // namespace leet
