// Small filesystem helpers (thin wrappers over std::filesystem).
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace leet::fs {

bool exists(const std::string &p);
bool is_dir(const std::string &p);
std::optional<std::string> read_file(const std::string &p);
bool write_file(const std::string &p, const std::string &content);
bool mkdirs(const std::string &p);
bool remove(const std::string &p);
std::vector<std::string> list_dir(const std::string &p);  // sorted file names
std::string join(const std::string &a, const std::string &b);
std::string dirname(const std::string &p);
std::string basename(const std::string &p);
std::string home();
std::string exe_dir();
std::string absolute(const std::string &p);
// Replace $HOME prefix with ~ for display.
std::string pretty(const std::string &p);
long long mtime(const std::string &p);
// Search $PATH for an executable.
std::optional<std::string> which(const std::string &name);

}  // namespace leet::fs
