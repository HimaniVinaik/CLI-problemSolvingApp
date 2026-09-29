// In-memory catalogue of all problems with lookup and filtering.
#pragma once

#include <string>
#include <vector>

#include "core/problem.hpp"

namespace leet {

struct Filter {
    std::string category;    // substring match
    std::string difficulty;  // easy | medium | hard
    std::string lang;        // cpp | c
    std::string search;      // matches title/slug/topics
    std::string status;      // solved | attempted | todo  (applied by caller)
};

class Repository {
public:
    bool load(const std::string &problems_dir);
    const std::vector<std::string> &errors() const { return errors_; }

    const std::vector<Problem> &all() const { return problems_; }
    // Accepts "88", "c7", "C7", "two-sum", or a unique title fragment.
    const Problem *find(const std::string &query) const;
    // Close matches for "did you mean".
    std::vector<const Problem *> suggest(const std::string &query, size_t max = 5) const;
    std::vector<const Problem *> filter(const Filter &f) const;
    std::vector<std::string> categories() const;  // in list order
    const Problem *next(const Problem &p, int dir) const;

private:
    std::vector<Problem> problems_;
    std::vector<std::string> errors_;
};

}  // namespace leet
