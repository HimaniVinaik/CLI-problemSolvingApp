// Compiles a solution (user or reference) together with the problem driver.
#pragma once

#include <string>

#include "core/config.hpp"
#include "core/problem.hpp"

namespace leet::judge {

struct BuildOptions {
    bool sanitize = false;  // -fsanitize=address,undefined (debugging crashes)
    bool reference = false; // build the bundled reference solution instead
    std::string tag;        // cache tag override (dev tools)
    bool count = false;     // operation-counting build for complexity analysis
};

struct BuildResult {
    bool ok = false;
    bool cached = false;
    std::string binary;
    std::string log;        // compiler diagnostics
    std::string command;    // for --verbose
    double seconds = 0;
};

class Builder {
public:
    explicit Builder(const Config &cfg) : cfg_(cfg) {}

    BuildResult build(const Problem &p, const std::string &source_path, const BuildOptions &opt);
    BuildResult build_reference(const Problem &p, bool count = false);
    bool can_count() const;
    std::string work_dir(const Problem &p) const;

private:
    const Config &cfg_;
    std::string pch_dir(const std::vector<std::string> &flags, std::string &log);
    std::string counter_object(std::string &log);
};

}  // namespace leet::judge
