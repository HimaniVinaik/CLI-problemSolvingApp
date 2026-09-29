// The Problem model and the loader for *.lc problem files.
//
// A problem file contains one or more problems:
//
//   @@ category: Array / String            (applies to following problems)
//   @@@ problem 88 merge-sorted-array
//   title: Merge Sorted Array
//   difficulty: Easy
//   topics: Array, Two Pointers
//   params: nums1, m, nums2, n
//   time: O(m + n)
//   space: O(1)
//   bench: n=64..1048576 time=n space=1
//   === description   (markdown)
//   === constraints   (markdown)
//   === hints         ("- " bullet per hint)
//   === template      (starter code given to the user)
//   === driver        (C++ that tells the judge how to call the solution)
//   === approach      (markdown explanation of the reference solution)
//   === solution      (fully commented reference solution)
//   === examples      (test cases shown in the statement)
//   === tests         (hidden test cases)
//
// Test cases are blank-line separated; each is one argument per line, then
//   > expected output          ("> ?" = filled in by `leet dev bake`)
//   : explanation (examples only, optional)
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace leet {

struct TestCase {
    std::vector<std::string> args;
    std::string expected;
    std::string explanation;
    bool example = false;
    int expected_line = 0;  // line number of "> ..." in the source file (for baking)
};

struct BenchSpec {
    bool enabled = false;
    long long min_n = 64;
    long long max_n = 1 << 20;
    bool additive = false;  // step: n += step (else n *= step)
    long long step = 2;
    std::string time_class;   // expected, e.g. "n", "nlogn"
    std::string space_class;  // expected, "-" to skip
    std::string unit;         // human description of n ("n = nums.length")
};

struct Problem {
    std::string key;          // "88" or "c7"
    int number = 0;
    std::string slug, title, difficulty, category, lang = "cpp";
    std::vector<std::string> topics, params, hints;
    std::string time, space;  // expected complexity (display)
    BenchSpec bench;
    int stress_n = 20;        // max n for randomized tests
    int stress_count = 30;
    double timeout = 3.0;     // seconds per test
    std::string description, constraints, tmpl, driver, approach, solution;
    std::vector<TestCase> tests;  // examples first
    std::string source_file;
    size_t order = 0;

    bool is_c() const { return lang == "c"; }
    std::string ext() const { return is_c() ? "c" : "cpp"; }
    std::string display_id() const;   // "88" / "C7"
    std::string file_stem() const;    // "0088-merge-sorted-array" / "c07-reverse-string"
    std::string url() const;          // leetcode link for cpp problems
    size_t example_count() const;
};

// Parse every *.lc file below `dir`. Errors are appended to `errors`.
std::vector<Problem> load_problems(const std::string &dir, std::vector<std::string> &errors);

// Parse one problem file (exposed for tooling).
std::vector<Problem> parse_problem_file(const std::string &path, const std::string &default_lang,
                                        std::vector<std::string> &errors);

// Pretty complexity class name: "nlogn" -> "O(n log n)".
std::string complexity_label(const std::string &cls);

}  // namespace leet
