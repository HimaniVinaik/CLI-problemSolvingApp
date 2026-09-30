// Submission history: every `leet submit` stores a snapshot of the source
// together with its verdict, so old attempts can be listed, viewed and restored.
//
//   <workspace>/.leet/submissions.tsv                  index (one line per submission)
//   <workspace>/.leet/submissions/<stem>/<id>.<ext>    source snapshots
#pragma once

#include <string>
#include <vector>

#include "core/config.hpp"
#include "core/problem.hpp"

namespace leet {

struct Submission {
    long long id = 0;          // unique, increasing (unix time in ms)
    std::string key;           // problem key ("88", "c7")
    long long time = 0;        // unix seconds
    std::string verdict;       // "Accepted", "Wrong Answer", ...
    int passed = 0, total = 0;
    double max_ms = 0;
    std::string time_class, space_class;   // "-" if not measured
    std::string snapshot;      // path of the saved source

    bool accepted() const { return verdict == "Accepted"; }
};

class History {
public:
    explicit History(const Config &cfg);

    // Store a snapshot of `source_path` and append an index entry.
    Submission add(const Problem &p, const std::string &source_path, const std::string &verdict, int passed,
                   int total, double max_ms, const std::string &tcls, const std::string &scls);

    std::vector<Submission> for_problem(const std::string &key) const;  // oldest first
    std::vector<Submission> recent(size_t limit) const;                 // newest first
    size_t size() const { return all_.size(); }

private:
    std::string index_path_, dir_;
    std::vector<Submission> all_;
    void load();
};

std::string format_when(long long unix_time);   // "today 14:03", "yesterday 09:12", "2026-09-01 10:00"

}  // namespace leet
