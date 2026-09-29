// Maintainer tools:  leet dev check | verify | bake
//
//   check   parse all problem files and report format errors
//   verify  compile every template, run every reference solution through the
//           full judge and confirm the measured complexity matches the spec
//   bake    fill "> ?" expected outputs of hidden tests from the reference
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <map>
#include <mutex>
#include <thread>

#include "app/app.hpp"
#include "ui/render.hpp"
#include "ui/term.hpp"
#include "util/fs.hpp"
#include "util/strings.hpp"

namespace leet {

using namespace judge;
using std::cout;

namespace {
std::vector<const Problem *> select(const Repository &repo, const Args &a, size_t from) {
    std::vector<const Problem *> out;
    if (a.pos.size() <= from) {
        for (auto &p : repo.all()) out.push_back(&p);
        return out;
    }
    for (size_t i = from; i < a.pos.size(); ++i) {
        std::string q = a.pos[i];
        if (q == "cpp" || q == "c") {
            for (auto &p : repo.all())
                if (p.lang == q) out.push_back(&p);
            continue;
        }
        if (auto *p = repo.find(q)) out.push_back(p);
        else std::cerr << "unknown problem " << q << "\n";
    }
    return out;
}
}  // namespace

int App::cmd_dev(const Args &a) {
    std::string sub = a.pos.empty() ? "" : a.pos[0];

    if (sub == "check") {
        cout << "problems: " << repo_.all().size() << "\n";
        for (auto &e : repo_.errors()) cout << ui::err("error: ") << e << "\n";
        std::map<std::string, int> seen;
        int bad = (int)repo_.errors().size();
        for (auto &p : repo_.all()) {
            if (seen[p.key]++) { cout << ui::err("duplicate key ") << p.key << "\n"; ++bad; }
            size_t pc = p.params.size();
            for (auto &t : p.tests)
                if (pc && t.args.size() != pc) {
                    cout << ui::err("arg count ") << p.key << ": expected " << pc << " got " << t.args.size() << " ["
                         << str::join(t.args, " | ") << "]\n";
                    ++bad;
                }
            if (p.example_count() == 0) { cout << ui::warn("no examples ") << p.key << "\n"; }
            if (p.hints.empty()) cout << ui::warn("no hints ") << p.key << "\n";
            if (p.approach.empty()) cout << ui::warn("no approach ") << p.key << "\n";
        }
        cout << (bad ? ui::err(std::to_string(bad) + " problem(s)") : ui::ok("all good")) << "\n";
        return bad ? 1 : 0;
    }

    if (sub == "bake") {
        auto list = select(repo_, a, 1);
        std::map<std::string, std::map<int, std::string>> edits;  // file -> line -> text
        int filled = 0, failed = 0;
        for (auto *p : list) {
            for (auto &t : p->tests) {
                if (t.expected != "?" && !(a.has("force") && !t.example)) continue;
                std::string err;
                auto out = judge_->reference_output(*p, t.args, err);
                if (!out) {
                    cout << ui::err("fail ") << p->key << ": " << err << "\n";
                    ++failed;
                    continue;
                }
                edits[p->source_file][t.expected_line] = "> " + *out;
                ++filled;
            }
        }
        for (auto &[file, lines] : edits) {
            auto c = fs::read_file(file);
            if (!c) continue;
            auto v = str::split(*c, '\n');
            for (auto &[ln, text] : lines)
                if (ln >= 1 && ln <= (int)v.size()) v[ln - 1] = text;
            fs::write_file(file, str::join(v, "\n"));
        }
        cout << "baked " << filled << " expected outputs" << (failed ? ", " + std::to_string(failed) + " failed" : "") << "\n";
        return failed ? 1 : 0;
    }

    if (sub == "verify") {
        auto list = select(repo_, a, 1);
        bool bench = !a.has("no-bench");
        int jobs = std::max(1, std::atoi(a.get("jobs", bench ? "2" : std::to_string(std::max(1u, std::thread::hardware_concurrency()))).c_str()));
        std::mutex mu;
        std::atomic<size_t> next{0};
        std::atomic<int> failures{0}, warnings{0};

        // Warm the precompiled header once before going parallel.
        if (!list.empty()) {
            std::string tp = fs::join(judge_->builder().work_dir(*list[0]), "template." + list[0]->ext());
            fs::write_file(tp, list[0]->tmpl);
            BuildOptions bo;
            bo.tag = "tmpl";
            judge_->builder().build(*list[0], tp, bo);
        }

        auto worker = [&] {
            Judge judge(cfg_);
            while (true) {
                size_t i = next++;
                if (i >= list.size()) break;
                const Problem &p = *list[i];
                auto t0 = std::chrono::steady_clock::now();
                std::string msg;
                bool fail = false, warn = false;

                // 1) the template must compile against the driver
                std::string tp = fs::join(judge.builder().work_dir(p), "template." + p.ext());
                fs::write_file(tp, p.tmpl);
                BuildOptions bo;
                bo.tag = "tmpl";
                auto tb = judge.builder().build(p, tp, bo);
                if (!tb.ok) { fail = true; msg += "template does not compile:\n" + tb.log; }

                // 2) the reference must pass everything
                Report r;
                if (!fail) {
                    auto rb = judge.builder().build_reference(p);
                    if (!rb.ok) { fail = true; msg += "reference does not compile:\n" + rb.log; }
                    else {
                        Options o;
                        o.bench = bench;
                        o.source_is_reference = true;
                        r = judge.submit(p, fs::join(judge.builder().work_dir(p), "reference." + p.ext()), o);
                        if (r.verdict != Verdict::Accepted) {
                            fail = true;
                            msg += verdict_name(r.verdict) + " " + r.message;
                            if (r.failure) {
                                msg += " on " + r.failure->kind + " #" + std::to_string(r.failure->index) + "\n  input: " +
                                       str::truncate(str::join(r.failure->input, " | "), 300) + "\n  expected: " +
                                       str::truncate(r.failure->expected, 200) + "\n  got: " + str::truncate(r.failure->got, 200) +
                                       "\n  " + r.failure->detail + " " + str::truncate(r.failure->user_stderr, 400);
                            }
                        }
                    }
                }
                // 3) complexity sanity
                std::string cx;
                if (!fail && bench && p.bench.enabled) {
                    auto &b = r.bench;
                    if (!b.ran) { warn = true; msg += "bench did not run: " + b.skipped + " " + b.stopped; }
                    else {
                        cx = "t=" + b.time.cls + " s=" + b.space.cls;
                        if (b.time.cls != p.bench.time_class) {
                            warn = true;
                            msg += "time " + b.time.cls + " != spec " + p.bench.time_class + "  ";
                        }
                        if (p.bench.space_class != "-" && b.space.cls != p.bench.space_class) {
                            warn = true;
                            msg += "space " + b.space.cls + " != spec " + p.bench.space_class + "  ";
                        }
                        if (warn || a.has("verbose")) {
                            msg += "\n    ";
                            for (auto &pt : b.user)
                                msg += str::format_count(pt.n) + ":" + str::format_ns(pt.ns) + "/" + str::format_bytes((double)pt.memory()) + "  ";
                            if (!b.stopped.empty()) msg += "\n    " + b.stopped;
                        }
                    }
                }
                double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
                std::lock_guard<std::mutex> g(mu);
                if (fail) ++failures;
                if (warn) ++warnings;
                std::string tag = fail ? ui::err("FAIL") : warn ? ui::warn("WARN") : ui::ok(" OK ");
                char tm[16];
                std::snprintf(tm, sizeof tm, "%5.1fs", secs);
                cout << tag << " " << str::pad_right(p.display_id(), 5) << str::pad_right(p.slug, 48) << ui::dim(tm) << "  "
                     << ui::dim(std::to_string(r.passed()) + " tests " + cx) << "\n";
                if (!msg.empty()) cout << "     " << msg << "\n";
                cout.flush();
            }
        };
        std::vector<std::thread> pool;
        for (int j = 0; j < jobs; ++j) pool.emplace_back(worker);
        for (auto &t : pool) t.join();
        cout << "\n" << list.size() << " problems, " << failures << " failed, " << warnings << " warnings\n";
        return failures ? 1 : 0;
    }

    cout << "usage: leet dev check | verify [ids…] [--no-bench] [--jobs N] [--verbose] | bake [ids…] [--force]\n";
    return 2;
}

}  // namespace leet
