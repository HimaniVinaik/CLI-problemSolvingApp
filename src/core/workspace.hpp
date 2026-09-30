// The user's solution files and progress.
#pragma once

#include <map>
#include <string>

#include "core/config.hpp"
#include "core/problem.hpp"

namespace leet {

class Workspace {
public:
    explicit Workspace(const Config &cfg) : cfg_(cfg) {}

    std::string solution_path(const Problem &p) const;
    bool has_solution(const Problem &p) const;
    // Creates the file from the template if missing. Returns the path.
    std::string ensure_solution(const Problem &p) const;
    // Moves the current file to a timestamped backup and recreates the template.
    std::string reset_solution(const Problem &p) const;
    // Replace the solution file with `content`, backing up the current file first.
    std::string replace_solution(const Problem &p, const std::string &content) const;
    // True if the file is still the untouched template.
    bool is_untouched(const Problem &p) const;
    std::string custom_input_path(const Problem &p) const;
    int open_in_editor(const std::string &path) const;

private:
    const Config &cfg_;
    std::string starter(const Problem &p) const;
};

struct ProgressEntry {
    std::string status;      // "solved" | "attempted"
    int attempts = 0;
    long long last = 0;      // unix time of last submission
    double best_ms = 0;      // slowest test runtime on best accepted run
    std::string time_class;  // estimated complexity on last accepted run
    std::string space_class;
    bool manual = false;     // marked done by hand (leet done), not by the judge
};

class Progress {
public:
    explicit Progress(const Config &cfg);
    const ProgressEntry *get(const std::string &key) const;
    std::string status(const std::string &key) const;  // "" if never tried
    void record(const std::string &key, bool accepted, double ms, const std::string &tcls, const std::string &scls);
    void clear(const std::string &key);
    // Mark a problem done / not done by hand.
    void mark(const std::string &key, bool done);
    bool save() const;
    const std::map<std::string, ProgressEntry> &entries() const { return data_; }
    int streak_days() const;

private:
    std::string path_;
    std::map<std::string, ProgressEntry> data_;
};

}  // namespace leet
