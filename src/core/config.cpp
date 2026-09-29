#include "core/config.hpp"

#include <cstdlib>

#include "util/fs.hpp"

#ifndef LEET_SOURCE_DIR
#define LEET_SOURCE_DIR "."
#endif
#ifndef LEET_INSTALL_DATA_DIR
#define LEET_INSTALL_DATA_DIR ""
#endif

namespace leet {

namespace {
std::string env(const char *k) {
    const char *v = std::getenv(k);
    return v ? v : "";
}
bool looks_like_data(const std::string &d) {
    return !d.empty() && fs::is_dir(fs::join(d, "problems")) && fs::exists(fs::join(d, "runtime/lc/harness.hpp"));
}
}  // namespace

Config Config::load() {
    Config c;
    // 1) explicit override, 2) next to the executable (build tree or prefix/share),
    // 3) the install prefix baked at configure time, 4) the source tree.
    std::string exe = fs::exe_dir();
    for (const std::string &cand : {env("LEET_DATA"), fs::join(exe, ".."), fs::join(exe, "../share/leet"), exe,
                                     std::string(LEET_INSTALL_DATA_DIR), std::string(LEET_SOURCE_DIR)}) {
        if (looks_like_data(cand)) { c.data_dir = fs::absolute(cand); break; }
    }
    c.problems_dir = fs::join(c.data_dir, "problems");
    c.runtime_dir = fs::join(c.data_dir, "runtime");

    std::string ws = env("LEET_WORKSPACE");
    c.workspace = ws.empty() ? fs::join(fs::home(), "leet-workspace") : fs::absolute(ws);
    c.state_dir = fs::join(c.workspace, ".leet");
    c.build_dir = fs::join(c.state_dir, "build");

    if (!env("LEET_CXX").empty()) c.cxx = env("LEET_CXX");
    else if (!fs::which("g++") && fs::which("clang++")) c.cxx = "clang++";
    if (!env("LEET_CC").empty()) c.cc = env("LEET_CC");
    else if (!fs::which("gcc") && fs::which("clang")) c.cc = "clang";

    c.editor = env("LEET_EDITOR");
    if (c.editor.empty()) c.editor = env("VISUAL");
    if (c.editor.empty()) c.editor = env("EDITOR");
    if (c.editor.empty()) {
        for (const char *e : {"nano", "vim", "vi", "micro", "emacs"})
            if (fs::which(e)) { c.editor = e; break; }
    }
    return c;
}

bool Config::data_ok() const { return looks_like_data(data_dir); }

}  // namespace leet
