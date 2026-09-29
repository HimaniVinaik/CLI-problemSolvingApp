// Application layer: command dispatch shared by the CLI and the interactive shell.
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/config.hpp"
#include "core/repository.hpp"
#include "core/workspace.hpp"
#include "judge/judge.hpp"

namespace leet {

// Parsed command line: positionals + options.
struct Args {
    std::string command;
    std::vector<std::string> pos;
    std::map<std::string, std::string> opts;

    static Args parse(const std::vector<std::string> &argv);
    bool has(const std::string &k) const { return opts.count(k) > 0; }
    std::string get(const std::string &k, const std::string &def = "") const;
};

class App {
public:
    App();
    int main(int argc, char **argv);
    int dispatch(const std::vector<std::string> &argv);

    // exposed for the shell
    const Problem *current() const { return current_; }
    const Repository &repo() const { return repo_; }
    std::string prompt() const;
    std::vector<std::string> command_names() const;

private:
    Config cfg_;
    Repository repo_;
    std::unique_ptr<Workspace> ws_;
    std::unique_ptr<Progress> progress_;
    std::unique_ptr<judge::Judge> judge_;
    const Problem *current_ = nullptr;
    bool in_shell_ = false;

    // resolution
    const Problem *resolve(const Args &a, size_t idx = 0);
    bool confirm(const std::string &question, bool default_yes, const Args &a);

    // commands (commands.cpp)
    int cmd_list(const Args &a);
    int cmd_show(const Args &a);
    int cmd_edit(const Args &a);
    int cmd_test(const Args &a);
    int cmd_run(const Args &a);
    int cmd_submit(const Args &a);
    int cmd_analyze(const Args &a);
    int cmd_solution(const Args &a);
    int cmd_hint(const Args &a);
    int cmd_stats(const Args &a);
    int cmd_random(const Args &a);
    int cmd_categories(const Args &a);
    int cmd_reset(const Args &a);
    int cmd_path(const Args &a);
    int cmd_config(const Args &a);
    int cmd_help(const Args &a);
    int cmd_next(const Args &a, int dir);
    int cmd_dev(const Args &a);   // dev.cpp
    int shell();                  // shell.cpp

    // views (views.cpp)
    void view_header(const Problem &p) const;
    void view_statement(const Problem &p) const;
    void view_examples(const Problem &p) const;
    void view_case(const Problem &p, const judge::CaseResult &c, bool show_expected = true) const;
    void view_compile_error(const Problem &p, const judge::BuildResult &b) const;
    void view_report(const Problem &p, const judge::Report &r) const;
    void view_bench(const Problem &p, const judge::BenchReport &b) const;
    void view_next_steps(const std::vector<std::pair<std::string, std::string>> &steps) const;
    std::string status_icon(const Problem &p) const;
    std::string format_input(const Problem &p, const std::vector<std::string> &args, int indent) const;
};

}  // namespace leet
