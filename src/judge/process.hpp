// Running child processes with limits (the judge's sandbox).
#pragma once

#include <string>
#include <vector>

namespace leet::judge {

struct Limits {
    double timeout_s = 5.0;             // wall clock
    long long stack_bytes = 1ll << 30;  // generous: deep recursion is legal
    long long memory_bytes = 0;         // address-space cap (0 = none)
    bool apply = true;
};

struct ProcResult {
    bool started = false;
    int exit_code = -1;
    int signal = 0;
    bool timed_out = false;
    double wall_ms = 0;
    long max_rss_kb = 0;
    std::string error;  // launch failure description

    bool ok() const { return started && !timed_out && signal == 0 && exit_code == 0; }
};

ProcResult run(const std::vector<std::string> &argv, const std::string &stdin_path,
               const std::string &stdout_path, const std::string &stderr_path, const Limits &limits);

// Run and capture stdout+stderr together (for compilers).
ProcResult run_capture(const std::vector<std::string> &argv, std::string &output, double timeout_s = 120);

std::string signal_name(int sig);
std::string signal_explanation(int sig);

}  // namespace leet::judge
