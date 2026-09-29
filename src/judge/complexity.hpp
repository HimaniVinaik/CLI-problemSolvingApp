// Empirical complexity estimation.
//
// The judge runs the solution on generated inputs of growing size n and
// records, per call:  time,  peak extra heap,  and stack depth.
// We then fit each candidate growth function f(n) and pick the best one
// (preferring the simpler class when two fit about equally well, because
// cache effects and timer noise tend to make curves look slightly steeper).
#pragma once

#include <string>
#include <vector>

namespace leet::judge {

struct BenchPoint {
    long long n = 0;
    double ns = 0;
    long long heap = 0;
    long long stack = 0;
    long long ops = 0;    // executed basic blocks (0 = not measured)
    long long memory() const { return heap + stack; }
};

struct Fit {
    std::string cls;      // "n", "nlogn", ... ("" if not enough data)
    double error = 0;     // relative RMS error of the chosen model
    bool ok() const { return !cls.empty(); }
};

// small_domain = inputs grow additively (n = 1,2,3,…), e.g. exponential problems
Fit fit_time(const std::vector<BenchPoint> &pts, bool small_domain);   // wall-clock timings
Fit fit_work(const std::vector<BenchPoint> &pts, bool small_domain);   // operation counts
Fit fit_space(const std::vector<BenchPoint> &pts, bool small_domain);

// Combine the operation-count fit (precise, but blind to work done inside
// non-instrumented library code such as memcpy) with the timing fit (sees
// everything, but noisy).  Returns the final class and sets `basis`.
Fit choose_time_class(const Fit &work, const Fit &wall, std::string &basis);

int class_rank(const std::string &cls);  // orders classes by growth

enum class Match { Optimal, Close, Worse, Unknown };
// precise = the estimate came from deterministic measurements (operation
// counts, memory); otherwise neighbouring classes like n / n log n are
// treated as indistinguishable.
Match compare_class(const std::string &expected, const std::string &estimated, bool precise = false);

}  // namespace leet::judge
