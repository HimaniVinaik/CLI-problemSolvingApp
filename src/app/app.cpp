#include "app/app.hpp"

#include <unistd.h>

#include <iostream>
#include <set>

#include "ui/render.hpp"
#include "ui/term.hpp"
#include "util/fs.hpp"
#include "util/strings.hpp"

namespace leet {

using std::cout;

// ---------------------------------------------------------------------------
//  Args
// ---------------------------------------------------------------------------
namespace {
// options that consume a value
const std::set<std::string> kValued = {"difficulty", "category", "lang", "status", "input", "search", "jobs"};
const std::map<std::string, std::string> kShort = {
    {"d", "difficulty"}, {"c", "category"}, {"l", "lang"}, {"s", "status"}, {"i", "input"},
    {"a", "all"},        {"y", "yes"},      {"h", "help"}, {"v", "verbose"}, {"f", "flat"}};
}  // namespace

Args Args::parse(const std::vector<std::string> &argv) {
    Args a;
    for (size_t i = 0; i < argv.size(); ++i) {
        const std::string &s = argv[i];
        if (s.size() > 1 && s[0] == '-' && !(s.size() > 1 && std::isdigit((unsigned char)s[1]))) {
            std::string k = s.substr(s[1] == '-' ? 2 : 1), v = "1";
            auto eq = k.find('=');
            if (eq != std::string::npos) { v = k.substr(eq + 1); k = k.substr(0, eq); }
            if (kShort.count(k)) k = kShort.at(k);
            if (eq == std::string::npos && kValued.count(k) && i + 1 < argv.size()) v = argv[++i];
            a.opts[k] = v;
        } else if (a.command.empty()) {
            a.command = s;
        } else {
            a.pos.push_back(s);
        }
    }
    return a;
}

std::string Args::get(const std::string &k, const std::string &def) const {
    auto it = opts.find(k);
    return it == opts.end() ? def : it->second;
}

// ---------------------------------------------------------------------------
//  App
// ---------------------------------------------------------------------------
App::App() {
    ui::init_terminal();
    cfg_ = Config::load();
    ws_ = std::make_unique<Workspace>(cfg_);
    progress_ = std::make_unique<Progress>(cfg_);
    history_ = std::make_unique<History>(cfg_);
    judge_ = std::make_unique<judge::Judge>(cfg_);
}

int App::main(int argc, char **argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    for (auto &a : args) {
        if (a == "--no-color") ui::set_color(false);
        if (a == "--color") ui::set_color(true);
    }
    if (!cfg_.data_ok()) {
        std::cerr << "leet: cannot find the problem data (problems/ and runtime/).\n"
                     "      Set LEET_DATA to the project directory, e.g.  export LEET_DATA=/path/to/CLI-problemSolvingApp\n";
        return 2;
    }
    repo_.load(cfg_.problems_dir);
    if (!repo_.errors().empty() && (std::getenv("LEET_DEBUG") || (args.size() && args[0] == "dev"))) {
        for (auto &e : repo_.errors()) std::cerr << ui::warn("problem file: ") << e << "\n";
    }
    if (args.empty() || (args.size() == 1 && (args[0] == "shell" || args[0] == "--no-color" || args[0] == "--color"))) {
        if (!ui::is_tty() && args.empty()) return cmd_help(Args{});
        return shell();
    }
    return dispatch(args);
}

std::vector<std::string> App::command_names() const {
    return {"list", "show", "edit", "test", "run", "submit", "analyze", "solution", "hint", "stats", "random",
            "topics", "reset", "history", "restore", "done", "undone", "path", "config", "help", "next", "prev", "open", "clear", "quit"};
}

std::string App::prompt() const {
    std::string p = ui::accent("leet");
    if (current_) p += ui::dim(" (") + ui::accent2(current_->display_id() + " " + current_->slug) + ui::dim(")");
    return p + " " + ui::accent2(ui::sym().arrow) + " ";
}

int App::dispatch(const std::vector<std::string> &argv) {
    Args a = Args::parse(argv);
    std::string c = str::lower(a.command);
    if (a.has("help") && !c.empty() && c != "help") {
        Args h;
        h.pos = {c};
        return cmd_help(h);
    }
    if (c.empty() || c == "help" || c == "?") return cmd_help(a);
    if (c == "list" || c == "ls" || c == "l") return cmd_list(a);
    if (c == "show" || c == "open" || c == "cd" || c == "view" || c == "cat") return cmd_show(a);
    if (c == "edit" || c == "start" || c == "solve" || c == "e") return cmd_edit(a);
    if (c == "test" || c == "t") return cmd_test(a);
    if (c == "run" || c == "r") return cmd_run(a);
    if (c == "submit" || c == "s" || c == "judge") return cmd_submit(a);
    if (c == "analyze" || c == "analyse" || c == "complexity" || c == "bench") return cmd_analyze(a);
    if (c == "solution" || c == "sol" || c == "answer") return cmd_solution(a);
    if (c == "hint" || c == "hints") return cmd_hint(a);
    if (c == "stats" || c == "progress") return cmd_stats(a);
    if (c == "random" || c == "pick") return cmd_random(a);
    if (c == "topics" || c == "categories" || c == "cats") return cmd_categories(a);
    if (c == "reset") return cmd_reset(a);
    if (c == "history" || c == "submissions" || c == "subs") return cmd_history(a);
    if (c == "restore") return cmd_restore(a);
    if (c == "done" || c == "mark") return cmd_mark(a, true);
    if (c == "undone" || c == "unmark" || c == "todo") return cmd_mark(a, false);
    if (c == "path" || c == "where") return cmd_path(a);
    if (c == "config" || c == "doctor") return cmd_config(a);
    if (c == "next" || c == "n") return cmd_next(a, +1);
    if (c == "prev" || c == "p") return cmd_next(a, -1);
    if (c == "shell" || c == "repl") return in_shell_ ? 0 : shell();
    if (c == "dev") return cmd_dev(a);
    if (c == "version" || c == "--version") {
        cout << "leet 1.0  -  " << repo_.all().size() << " problems\n";
        return 0;
    }
    // Bare problem id: `leet 88` == `leet show 88`
    if (repo_.find(a.command)) {
        Args s = a;
        s.pos.insert(s.pos.begin(), a.command);
        return cmd_show(s);
    }
    std::cerr << ui::err("Unknown command: ") << a.command << "\n";
    std::cerr << ui::dim("Run `leet help` to see all commands.") << "\n";
    return 2;
}

const Problem *App::resolve(const Args &a, size_t idx) {
    if (a.pos.size() <= idx) {
        if (current_) return current_;
        std::cerr << ui::err("Which problem?") << " Pass an id, e.g. " << ui::code("leet " + a.command + " 1")
                  << ui::dim("  (LeetCode number, C-problem id like c3, or a slug)") << "\n";
        return nullptr;
    }
    std::string q = a.pos[idx];
    // allow multi-word titles: `leet show two sum`
    for (size_t i = idx + 1; i < a.pos.size(); ++i) q += " " + a.pos[i];
    const Problem *p = repo_.find(a.pos[idx]);
    if (!p) p = repo_.find(q);
    if (!p) {
        std::cerr << ui::err("No problem matches ") << ui::code(q) << "\n";
        auto sug = repo_.suggest(q);
        if (!sug.empty()) {
            std::cerr << ui::dim("Did you mean:") << "\n";
            for (auto *s : sug)
                std::cerr << "  " << ui::accent2(str::pad_left(s->display_id(), 4)) << "  " << s->title << "\n";
        }
        return nullptr;
    }
    current_ = p;
    return p;
}

bool App::confirm(const std::string &q, bool def, const Args &a) {
    if (a.has("yes")) return true;
    if (!::isatty(STDIN_FILENO)) return def;
    cout << "  " << q << ui::dim(def ? " [Y/n] " : " [y/N] ") << std::flush;
    std::string line;
    if (!std::getline(std::cin, line)) return def;
    line = str::lower(str::trim(line));
    if (line.empty()) return def;
    return line[0] == 'y';
}

// ---------------------------------------------------------------------------
int App::cmd_help(const Args &a) {
    std::string topic = a.pos.empty() ? "" : str::lower(a.pos[0]);
    struct Cmd { std::string usage, desc, detail; };
    std::vector<std::pair<std::string, std::vector<Cmd>>> groups = {
        {"Browse", {
            {"list [filters]", "List problems (grouped by topic)",
             "Filters: -d easy|medium|hard   -c <topic>   -l cpp|c   -s solved|attempted|todo   --search <words>   --flat"},
            {"show <id>", "Read a problem statement", "id = LeetCode number (1), C problem (c3), slug (two-sum) or title words"},
            {"topics", "Topics with your progress", ""},
            {"random [filters]", "Pick a random unsolved problem", "Accepts the same filters as list"},
            {"next / prev", "Move through the list in order", ""},
        }},
        {"Solve", {
            {"edit <id>", "Create/open your solution in $EDITOR", "Files live in ~/leet-workspace (override with LEET_WORKSPACE)"},
            {"test <id>", "Run the example tests", "--sanitize  compile with AddressSanitizer + UBSan to locate crashes"},
            {"run <id> [-i file]", "Run on your own input (compared to the reference)",
             "Input: one argument per line, LeetCode syntax, e.g.  [2,7,11,15]  then  9"},
            {"submit <id>", "Full judge + complexity analysis",
             "Examples, hidden tests, randomized tests vs. the reference solution, a large\n"
             "performance test and an empirical time/space complexity estimate.\n"
             "--no-bench  skip the complexity analysis   --sanitize  debug build"},
            {"analyze <id>", "Only measure time/space complexity", ""},
            {"hint <id> [k]", "Reveal hints one by one", ""},
            {"solution <id>", "Commented reference solution + explanation", ""},
            {"reset <id>", "Restore the starter template (backs up your file)", ""},
        }},
        {"Track", {
            {"history [id] [n]", "Your saved submissions (all, one problem, or view #n)",
             "Every submit stores a snapshot of your code with its verdict.\n"
             "`leet history` shows recent submissions, `leet history 1` those of problem 1,\n"
             "`leet history 1 3` prints the code of submission #3."},
            {"restore <id> [n]", "Put an old submission back into your file",
             "Without n: the latest accepted submission (or the latest one). Your current file is backed up."},
            {"done <id>", "Mark a problem as done by hand", "Counts as solved in list/stats. `leet undone <id>` reverts it."},
            {"undone <id>", "Mark a problem as not done", ""},
        }},
        {"Other", {
            {"stats", "Progress dashboard", ""},
            {"path <id>", "Print the solution file path", ""},
            {"config", "Show paths, compilers, editor", ""},
            {"(no command)", "Interactive shell with history and tab completion", ""},
        }},
    };
    if (!topic.empty()) {
        for (auto &[g, cmds] : groups)
            for (auto &c : cmds)
                if (str::starts_with(c.usage, topic)) {
                    cout << "\n  " << ui::code("leet " + c.usage) << "\n  " << c.desc << "\n";
                    if (!c.detail.empty()) cout << ui::markdown(c.detail, ui::width(), 4) << "\n";
                    cout << "\n";
                    return 0;
                }
    }
    cout << "\n  " << ui::accent("leet") << ui::dim("  -  LeetCode-style practice in your terminal (C++ & C)") << "\n";
    size_t cpp = 0, c = 0;
    for (auto &p : repo_.all()) (p.is_c() ? c : cpp)++;
    cout << "  " << ui::dim(std::to_string(cpp) + " Top-Interview-150 problems in C++  " + ui::sym().dot + "  " +
                            std::to_string(c) + " classic C problems") << "\n\n";
    for (auto &[g, cmds] : groups) {
        cout << "  " << ui::bold(g) << "\n";
        for (auto &cm : cmds) cout << "    " << ui::code(str::pad_right(cm.usage, 22)) << ui::dim(cm.desc) << "\n";
        cout << "\n";
    }
    cout << "  " << ui::bold("Typical session") << "\n";
    cout << "    " << ui::dim("$ ") << "leet show 1      " << ui::dim("# read the problem") << "\n";
    cout << "    " << ui::dim("$ ") << "leet edit 1      " << ui::dim("# write your solution") << "\n";
    cout << "    " << ui::dim("$ ") << "leet test 1      " << ui::dim("# check the examples") << "\n";
    cout << "    " << ui::dim("$ ") << "leet submit 1    " << ui::dim("# full judge + complexity") << "\n\n";
    cout << "  " << ui::dim("`leet help <command>` for details.  Colors: --no-color / NO_COLOR.") << "\n\n";
    return 0;
}

int App::cmd_config(const Args &) {
    cout << "\n" << ui::rule("Configuration") << "\n\n";
    auto row = [](const std::string &k, const std::string &v, bool good = true) {
        cout << "  " << ui::dim(str::pad_right(k, 14)) << (good ? v : ui::err(v)) << "\n";
    };
    row("data", fs::pretty(cfg_.data_dir));
    row("workspace", fs::pretty(cfg_.workspace));
    auto cxx = fs::which(cfg_.cxx), cc = fs::which(cfg_.cc);
    row("C++ compiler", cxx ? *cxx : cfg_.cxx + " (not found!)", (bool)cxx);
    row("C compiler", cc ? *cc : cfg_.cc + " (not found!)", (bool)cc);
    row("editor", cfg_.editor.empty() ? "(none: set $EDITOR)" : cfg_.editor, !cfg_.editor.empty());
    row("problems", std::to_string(repo_.all().size()));
    if (!repo_.errors().empty()) row("file errors", std::to_string(repo_.errors().size()) + " (run `leet dev check`)", false);
    cout << "\n  " << ui::dim("Environment: LEET_WORKSPACE, LEET_DATA, LEET_CXX, LEET_CC, LEET_EDITOR / EDITOR, NO_COLOR") << "\n\n";
    return 0;
}

}  // namespace leet
