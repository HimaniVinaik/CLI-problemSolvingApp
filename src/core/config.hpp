// Where things live on disk, and which compilers to use.
#pragma once

#include <string>

namespace leet {

struct Config {
    std::string data_dir;       // contains problems/ and runtime/
    std::string problems_dir;   // data_dir/problems
    std::string runtime_dir;    // data_dir/runtime  (lc/harness.hpp, lc/c_prelude.h)
    std::string workspace;      // user solutions   (default ~/leet-workspace)
    std::string state_dir;      // workspace/.leet  (progress, build cache)
    std::string build_dir;      // state_dir/build
    std::string cxx = "g++";
    std::string cc = "gcc";
    std::string editor;

    static Config load();       // resolve from env / defaults
    bool data_ok() const;
};

}  // namespace leet
