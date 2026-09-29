#include "judge/judge.hpp"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <sstream>

#include "judge/process.hpp"
#include "util/fs.hpp"
#include "util/strings.hpp"

namespace leet::judge {

std::string verdict_name(Verdict v) {
    switch (v) {
        case Verdict::Accepted: return "Accepted";
        case Verdict::WrongAnswer: return "Wrong Answer";
        case Verdict::RuntimeError: return "Runtime Error";
        case Verdict::TimeLimit: return "Time Limit Exceeded";
        case Verdict::CompileError: return "Compile Error";
        case Verdict::JudgeError: return "Judge Error";
    }
    return "?";
}

int Report::passed() const {
    int e = 0;
    for (auto &x : examples) e += x.verdict == Verdict::Accepted;
    return e + tests_passed + random_passed + (large_ran && verdict == Verdict::Accepted ? 1 : 0);
}
int Report::total() const { return (int)examples.size() + tests_total + random_total + (large_ran ? 1 : 0); }

// Compare outputs: exact after trimming, with a numeric tolerance of 1e-5 for
// floating point tokens (so 2.50000 == 2.5).
bool outputs_match(const std::string &e0, const std::string &g0) {
    std::string e = str::trim(e0), g = str::trim(g0);
    if (e == g) return true;
    size_t i = 0, j = 0;
    auto numstart = [](const std::string &s, size_t k) {
        return k < s.size() && (std::isdigit((unsigned char)s[k]) || ((s[k] == '-' || s[k] == '.') && k + 1 < s.size() &&
                                                                     std::isdigit((unsigned char)s[k + 1])));
    };
    while (i < e.size() && j < g.size()) {
        if (numstart(e, i) && numstart(g, j)) {
            char *pe, *pg;
            double a = std::strtod(e.c_str() + i, &pe), b = std::strtod(g.c_str() + j, &pg);
            if (std::fabs(a - b) > 1e-5 * std::max(1.0, std::fabs(a))) return false;
            i = pe - e.c_str();
            j = pg - g.c_str();
            continue;
        }
        if (e[i] == ' ') { ++i; continue; }
        if (g[j] == ' ') { ++j; continue; }
        if (e[i] != g[j]) return false;
        ++i, ++j;
    }
    while (i < e.size() && e[i] == ' ') ++i;
    while (j < g.size() && g[j] == ' ') ++j;
    return i == e.size() && j == g.size();
}

// ---------------------------------------------------------------------------
CaseResult Judge::run_case(const Problem &p, const std::string &bin, const std::vector<std::string> &input,
                           double timeout, bool sanitize) {
    CaseResult cr;
    cr.input = input;
    std::string dir = fs::join(builder_.work_dir(p), "run");
    fs::mkdirs(dir);
    std::string in = fs::join(dir, "in.txt"), out = fs::join(dir, "out.txt"), st = fs::join(dir, "stats.txt");
    std::string so = fs::join(dir, "stdout.txt"), se = fs::join(dir, "stderr.txt");
    fs::write_file(in, str::join(input, "\n") + "\n");
    fs::remove(out);
    fs::remove(st);

    Limits lim;
    lim.timeout_s = timeout;
    if (!sanitize) lim.memory_bytes = 4ll << 30;
    ProcResult r = run({bin, "run", in, out, st}, "", so, se, lim);
    cr.wall_ms = r.wall_ms;
    auto clip = [](std::string s) {
        if (s.size() > 4000) s = s.substr(0, 4000) + "\n… (truncated)";
        return s;
    };
    cr.user_stdout = clip(fs::read_file(so).value_or(""));
    cr.user_stderr = clip(fs::read_file(se).value_or(""));

    if (!r.started) {
        cr.verdict = Verdict::JudgeError;
        cr.detail = r.error;
        return cr;
    }
    if (r.timed_out) {
        cr.verdict = Verdict::TimeLimit;
        cr.detail = "no answer after " + str::format_ns(timeout * 1e9);
        cr.ms = r.wall_ms;
        return cr;
    }
    if (r.signal) {
        cr.verdict = Verdict::RuntimeError;
        cr.detail = signal_name(r.signal) + " - " + signal_explanation(r.signal);
        return cr;
    }
    if (r.exit_code == 3) {
        cr.verdict = Verdict::JudgeError;
        cr.detail = str::trim(cr.user_stderr);
        return cr;
    }
    if (r.exit_code != 0) {
        cr.verdict = Verdict::RuntimeError;
        std::string e = str::trim(cr.user_stderr);
        auto pos = e.find("[judge] uncaught exception: ");
        cr.detail = pos != std::string::npos ? "uncaught exception: " + e.substr(pos + 28)
                                             : "program exited with code " + std::to_string(r.exit_code);
        if (sanitize && e.find("Sanitizer") != std::string::npos) cr.detail = "sanitizer reported an error (see stderr)";
        return cr;
    }
    cr.got = str::trim(fs::read_file(out).value_or(""));
    std::istringstream ss(fs::read_file(st).value_or("0 0 0 0"));
    double ns = 0;
    ss >> ns;
    cr.ms = ns / 1e6;
    cr.verdict = Verdict::Accepted;  // provisional: caller compares output
    return cr;
}

bool Judge::generate(const std::string &bin, long long n, unsigned long long seed, std::vector<std::string> &lines,
                     std::string &err) {
    std::string dir = fs::dirname(bin);
    std::string out = fs::join(dir, "gen.txt"), se = fs::join(dir, "gen.err");
    Limits lim;
    lim.timeout_s = 30;
    auto r = run({bin, "gen", std::to_string(n), std::to_string(seed)}, "", out, se, lim);
    if (!r.ok()) {
        err = "generator failed (n=" + std::to_string(n) + "): " + fs::read_file(se).value_or("");
        return false;
    }
    lines = str::split_lines(fs::read_file(out).value_or(""));
    return true;
}

// ---------------------------------------------------------------------------
Report Judge::submit(const Problem &p, const std::string &source, const Options &opt) {
    Report rep;
    auto status = [&](const std::string &s) { if (opt.status) opt.status(s); };
    status("Compiling " + fs::basename(source) + " …");
    BuildOptions bo;
    bo.sanitize = opt.sanitize;
    bo.reference = opt.source_is_reference;
    rep.build = builder_.build(p, source, bo);
    if (!rep.build.ok) {
        rep.verdict = Verdict::CompileError;
        return rep;
    }
    const std::string &bin = rep.build.binary;
    double timeout = p.timeout * (opt.sanitize ? 4 : 1);

    auto record = [&](CaseResult &cr) {
        rep.max_ms = std::max(rep.max_ms, cr.ms);
        rep.total_ms += cr.ms;
    };
    auto check = [&](CaseResult &cr, const std::string &expected) {
        cr.expected = expected;
        if (cr.verdict == Verdict::Accepted && !outputs_match(expected, cr.got)) cr.verdict = Verdict::WrongAnswer;
        record(cr);
        return cr.verdict == Verdict::Accepted;
    };

    // 1) examples  2) hidden tests
    int ex = 0, hid = 0;
    for (auto &t : p.tests) {
        if (!t.example && opt.examples_only) break;
        status(t.example ? "Running example " + std::to_string(ex + 1) + " …"
                         : "Running test " + std::to_string(hid + 1) + " / " + std::to_string(p.tests.size() - p.example_count()) + " …");
        CaseResult cr = run_case(p, bin, t.args, timeout, opt.sanitize);
        cr.kind = t.example ? "example" : "test";
        cr.index = t.example ? ++ex : ++hid;
        cr.explanation = t.explanation;
        bool okc = check(cr, t.expected);
        if (t.example) rep.examples.push_back(cr);
        else { rep.tests_total++; if (okc) rep.tests_passed++; }
        if (!okc) {
            if (!rep.failure) rep.failure = cr;
            if (!t.example || !opt.examples_only) break;
        }
    }
    if (rep.failure) {
        rep.verdict = rep.failure->verdict;
        return rep;
    }
    if (opt.examples_only) {
        rep.verdict = Verdict::Accepted;
        return rep;
    }

    // 3) randomized tests against the reference solution
    std::string ref_bin;
    bool need_ref = (opt.stress || opt.large || opt.bench);
    if (need_ref && !p.driver.empty() && p.driver.find("h.gen(") != std::string::npos) {
        if (opt.source_is_reference) {
            ref_bin = bin;
        } else {
            status("Preparing reference judge …");
            auto rb = builder_.build_reference(p);
            if (!rb.ok) {
                rep.verdict = Verdict::JudgeError;
                rep.message = "reference solution failed to compile:\n" + rb.log;
                return rep;
            }
            ref_bin = rb.binary;
        }
    }
    if (opt.stress && !ref_bin.empty() && p.stress_count > 0) {
        int count = p.stress_count;
        for (int i = 0; i < count; ++i) {
            long long n = 1 + (long long)i * (p.stress_n - 1) / std::max(1, count - 1);
            unsigned long long seed = 7919ull * (i + 1) + (unsigned long long)p.number;
            status("Randomized test " + std::to_string(i + 1) + " / " + std::to_string(count) + " …");
            std::vector<std::string> lines;
            std::string err;
            if (!generate(ref_bin, n, seed, lines, err)) {
                rep.verdict = Verdict::JudgeError;
                rep.message = err;
                return rep;
            }
            CaseResult ref = run_case(p, ref_bin, lines, 30, false);
            if (ref.verdict != Verdict::Accepted) {
                rep.verdict = Verdict::JudgeError;
                rep.message = "reference failed on generated input: " + ref.detail + "\n" + ref.user_stderr;
                return rep;
            }
            CaseResult cr = opt.source_is_reference ? ref : run_case(p, bin, lines, timeout, opt.sanitize);
            cr.kind = "random";
            cr.index = i + 1;
            cr.n = n;
            rep.random_total++;
            if (!check(cr, ref.got)) {
                rep.failure = cr;
                rep.verdict = cr.verdict;
                return rep;
            }
            rep.random_passed++;
        }
    }

    // 4) one large input: correctness at scale + time limit
    if (opt.large && !ref_bin.empty() && p.bench.enabled && !opt.sanitize) {
        long long n = p.bench.max_n;
        status("Large test (n = " + str::format_count(n) + ") …");
        std::vector<std::string> lines;
        std::string err;
        if (generate(ref_bin, n, 424242, lines, err)) {
            CaseResult ref = run_case(p, ref_bin, lines, 60, false);
            if (ref.verdict == Verdict::Accepted) {
                CaseResult cr = opt.source_is_reference ? ref : run_case(p, bin, lines, timeout + 1.0, false);
                cr.kind = "large";
                cr.index = 1;
                cr.n = n;
                rep.large_ran = true;
                if (!check(cr, ref.got)) {
                    rep.failure = cr;
                    rep.verdict = cr.verdict;
                    return rep;
                }
            }
        }
    }
    rep.verdict = Verdict::Accepted;

    // 5) complexity
    if (opt.bench && !opt.sanitize) rep.bench = bench(p, source, bin, ref_bin, opt.source_is_reference, opt.status);
    return rep;
}

// ---------------------------------------------------------------------------
std::vector<BenchPoint> Judge::sweep(const Problem &p, const std::string &bin, const std::string &count_bin,
                                     std::string &stopped, const std::function<void(long long)> &tick) {
    std::vector<BenchPoint> pts;
    const auto &b = p.bench;
    auto start = std::chrono::steady_clock::now();
    std::string dir = fs::dirname(bin);
    std::string st = fs::join(dir, "bench.txt"), so = fs::join(dir, "bench.out");
    bool counting = !count_bin.empty();
    for (long long n = b.min_n; n <= b.max_n; n = b.additive ? n + b.step : n * b.step) {
        if (tick) tick(n);
        fs::remove(st);
        Limits lim;
        lim.timeout_s = 20;
        lim.memory_bytes = 6ll << 30;
        auto r = run({bin, "bench", std::to_string(n), "99991", st}, "", so, so, lim);
        if (!r.ok()) {
            stopped = r.timed_out ? "stopped at n = " + str::format_count(n) + ": too slow"
                                  : "stopped at n = " + str::format_count(n) + ": crashed";
            break;
        }
        std::istringstream ss(fs::read_file(st).value_or(""));
        BenchPoint pt;
        pt.n = n;
        long long reps = 0;
        if (!(ss >> pt.ns >> pt.heap >> pt.stack >> reps)) break;
        // Same input again, this time counting executed operations.
        if (counting) {
            fs::remove(st);
            lim.timeout_s = 40;
            auto rc = run({count_bin, "bench", std::to_string(n), "99991", st}, "", so, so, lim);
            std::istringstream cs(fs::read_file(st).value_or(""));
            double d;
            long long h, s, rr;
            if (!rc.ok() || !(cs >> d >> h >> s >> rr >> pt.ops) || pt.ops <= 0) counting = false;
        }
        if (!counting)
            for (auto &q : pts) q.ops = 0;  // incomplete counts are useless
        pts.push_back(pt);
        double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        if (pt.ns > 0.35e9) {
            if (n < b.max_n) stopped = "stopped at n = " + str::format_count(n) + ": calls became slower than 0.35 s";
            break;
        }
        if (elapsed > 12) {
            stopped = "stopped at n = " + str::format_count(n) + ": time budget used up";
            break;
        }
    }
    return pts;
}

BenchReport Judge::bench(const Problem &p, const std::string &source, const std::string &bin,
                         const std::string &ref_bin, bool is_ref,
                         const std::function<void(const std::string &)> &status) {
    BenchReport br;
    if (!p.bench.enabled) {
        br.skipped = "input size is fixed for this problem, so growth cannot be measured";
        return br;
    }
    if (ref_bin.empty()) {
        br.skipped = "no input generator available";
        return br;
    }
    bool small = p.bench.additive;

    // Operation-counting build of the same source (GCC only).
    std::string count_bin;
    if (builder_.can_count()) {
        if (status) status("Preparing operation counter …");
        BuildOptions bo;
        bo.count = true;
        bo.reference = is_ref;
        auto cb = is_ref ? builder_.build_reference(p, true) : builder_.build(p, source, bo);
        if (cb.ok) count_bin = cb.binary;
    }
    br.user = sweep(p, bin, count_bin, br.stopped, [&](long long n) {
        if (status) status("Measuring complexity: n = " + str::format_count(n) + " …");
    });
    br.wall = fit_time(br.user, small);
    br.work = fit_work(br.user, small);
    br.time = choose_time_class(br.work, br.wall, br.basis);
    br.space = fit_space(br.user, small);

    if (!is_ref) {
        std::string cache = fs::join(fs::dirname(ref_bin), "refbench-" + fs::basename(ref_bin) + ".txt");
        if (auto c = fs::read_file(cache)) {
            for (auto &line : str::split_lines(*c)) {
                std::istringstream ss(line);
                BenchPoint pt;
                if (ss >> pt.n >> pt.ns >> pt.heap >> pt.stack >> pt.ops) br.ref.push_back(pt);
            }
        } else {
            std::string dummy, ref_count;
            if (builder_.can_count()) {
                auto rc = builder_.build_reference(p, true);
                if (rc.ok) ref_count = rc.binary;
            }
            br.ref = sweep(p, ref_bin, ref_count, dummy, [&](long long n) {
                if (status) status("Measuring reference solution: n = " + str::format_count(n) + " …");
            });
            std::ostringstream o;
            for (auto &pt : br.ref) o << pt.n << ' ' << pt.ns << ' ' << pt.heap << ' ' << pt.stack << ' ' << pt.ops << '\n';
            fs::write_file(cache, o.str());
        }
        std::string b;
        br.ref_time = choose_time_class(fit_work(br.ref, small), fit_time(br.ref, small), b);
        br.ref_space = fit_space(br.ref, small);
    }
    br.ran = br.time.ok();
    if (!br.ran && br.skipped.empty()) br.skipped = "not enough data points (" + br.stopped + ")";
    return br;
}

Report Judge::analyze(const Problem &p, const std::string &source, const Options &opt) {
    Report rep;
    if (opt.status) opt.status("Compiling …");
    rep.build = builder_.build(p, source, {});
    if (!rep.build.ok) { rep.verdict = Verdict::CompileError; return rep; }
    std::string ref_bin;
    if (p.bench.enabled) {
        if (opt.status) opt.status("Preparing reference …");
        auto rb = builder_.build_reference(p);
        if (rb.ok) ref_bin = rb.binary;
    }
    rep.bench = bench(p, source, rep.build.binary, ref_bin, false, opt.status);
    rep.verdict = Verdict::Accepted;
    return rep;
}

Report Judge::run_custom(const Problem &p, const std::string &source, const std::vector<std::string> &input,
                         bool sanitize) {
    Report rep;
    BuildOptions bo;
    bo.sanitize = sanitize;
    rep.build = builder_.build(p, source, bo);
    if (!rep.build.ok) { rep.verdict = Verdict::CompileError; return rep; }
    CaseResult cr = run_case(p, rep.build.binary, input, p.timeout * (sanitize ? 4 : 1), sanitize);
    cr.kind = "custom";
    cr.index = 1;
    auto rb = builder_.build_reference(p);
    if (rb.ok) {
        CaseResult ref = run_case(p, rb.binary, input, 30, false);
        if (ref.verdict == Verdict::Accepted) cr.expected = ref.got;
        else if (ref.verdict == Verdict::JudgeError) cr.detail = ref.detail;
    }
    if (cr.verdict == Verdict::Accepted && !cr.expected.empty() && !outputs_match(cr.expected, cr.got))
        cr.verdict = Verdict::WrongAnswer;
    rep.verdict = cr.verdict;
    rep.examples.push_back(cr);
    return rep;
}

std::optional<std::string> Judge::reference_output(const Problem &p, const std::vector<std::string> &input,
                                                   std::string &error) {
    auto rb = builder_.build_reference(p);
    if (!rb.ok) { error = rb.log; return std::nullopt; }
    CaseResult cr = run_case(p, rb.binary, input, 60, false);
    if (cr.verdict != Verdict::Accepted) {
        error = verdict_name(cr.verdict) + ": " + cr.detail + " " + cr.user_stderr;
        return std::nullopt;
    }
    return cr.got;
}

}  // namespace leet::judge
