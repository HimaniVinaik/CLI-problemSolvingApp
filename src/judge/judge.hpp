// The judge: runs a compiled solution against a problem's tests.
//
// Pipeline for `submit`:
//   1. compile            (Compile Error)
//   2. example tests      (fixed input + expected output)
//   3. hidden tests       (fixed input + expected output, edge cases)
//   4. randomized tests   (generator -> reference solution -> compare)
//   5. large test         (max-size input; catches Time Limit Exceeded)
//   6. complexity bench   (growing n -> fitted time / space classes)
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "core/config.hpp"
#include "core/problem.hpp"
#include "judge/builder.hpp"
#include "judge/complexity.hpp"

namespace leet::judge {

enum class Verdict { Accepted, WrongAnswer, RuntimeError, TimeLimit, CompileError, JudgeError };
std::string verdict_name(Verdict v);

struct CaseResult {
    Verdict verdict = Verdict::JudgeError;
    std::string kind;              // "example" | "test" | "random" | "large" | "custom"
    int index = 0;                 // 1-based within kind
    std::vector<std::string> input;
    std::string expected, got;
    std::string user_stdout, user_stderr;
    std::string detail;            // signal explanation / exception text
    std::string explanation;       // example explanation
    double ms = 0;                 // measured solution call time
    double wall_ms = 0;
    long long n = 0;               // generator size for random/large tests
};

struct BenchReport {
    bool ran = false;
    std::string skipped;           // reason when not run
    std::vector<BenchPoint> user, ref;
    Fit time, space;               // final estimates
    Fit wall, work;                // time estimate from timings / from operation counts
    std::string basis;             // what the time estimate is based on
    Fit ref_time, ref_space;
    std::string stopped;           // why the sweep ended early
};

struct Report {
    Verdict verdict = Verdict::JudgeError;
    BuildResult build;
    std::vector<CaseResult> examples;      // all example results (for `test`)
    int tests_passed = 0, tests_total = 0;
    int random_passed = 0, random_total = 0;
    bool large_ran = false;
    std::optional<CaseResult> failure;
    double max_ms = 0, total_ms = 0;
    BenchReport bench;
    std::string message;                   // judge error text
    int passed() const;
    int total() const;
};

struct Options {
    bool examples_only = false;
    bool stress = true;
    bool large = true;
    bool bench = true;
    bool sanitize = false;
    std::string source;                    // override solution path (dev tools)
    bool source_is_reference = false;
    std::function<void(const std::string &)> status;  // progress text
};

class Judge {
public:
    explicit Judge(const Config &cfg) : cfg_(cfg), builder_(cfg) {}

    Report submit(const Problem &p, const std::string &source, const Options &opt);
    // Run the user's code on custom input lines; also runs the reference.
    Report run_custom(const Problem &p, const std::string &source, const std::vector<std::string> &input,
                      bool sanitize);
    // Only the complexity analysis.
    Report analyze(const Problem &p, const std::string &source, const Options &opt);
    // Output of the reference solution for given input lines (for baking).
    std::optional<std::string> reference_output(const Problem &p, const std::vector<std::string> &input,
                                                std::string &error);

    Builder &builder() { return builder_; }

private:
    const Config &cfg_;
    Builder builder_;

    CaseResult run_case(const Problem &p, const std::string &bin, const std::vector<std::string> &input,
                        double timeout, bool sanitize);
    bool generate(const std::string &bin, long long n, unsigned long long seed, std::vector<std::string> &lines,
                  std::string &err);
    BenchReport bench(const Problem &p, const std::string &source, const std::string &bin, const std::string &ref_bin,
                      bool is_ref, const std::function<void(const std::string &)> &status);
    std::vector<BenchPoint> sweep(const Problem &p, const std::string &bin, const std::string &count_bin,
                                  std::string &stopped, const std::function<void(long long)> &tick);
};

bool outputs_match(const std::string &expected, const std::string &got);

}  // namespace leet::judge
