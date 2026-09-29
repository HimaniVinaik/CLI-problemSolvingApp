#include "util/fs.hpp"

#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "util/strings.hpp"

namespace stdfs = std::filesystem;

namespace leet::fs {

bool exists(const std::string &p) {
    std::error_code ec;
    return stdfs::exists(p, ec);
}

bool is_dir(const std::string &p) {
    std::error_code ec;
    return stdfs::is_directory(p, ec);
}

std::optional<std::string> read_file(const std::string &p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return std::nullopt;
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool write_file(const std::string &p, const std::string &content) {
    mkdirs(dirname(p));
    std::string tmp = p + ".tmp" + std::to_string(::getpid());
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out << content;
        if (!out) return false;
    }
    std::error_code ec;
    stdfs::rename(tmp, p, ec);
    return !ec;
}

bool mkdirs(const std::string &p) {
    if (p.empty()) return true;
    std::error_code ec;
    stdfs::create_directories(p, ec);
    return is_dir(p);
}

bool remove(const std::string &p) {
    std::error_code ec;
    return stdfs::remove_all(p, ec) > 0;
}

std::vector<std::string> list_dir(const std::string &p) {
    std::vector<std::string> out;
    std::error_code ec;
    for (auto &e : stdfs::directory_iterator(p, ec)) out.push_back(e.path().filename().string());
    std::sort(out.begin(), out.end());
    return out;
}

std::string join(const std::string &a, const std::string &b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    if (a.back() == '/') return a + b;
    return a + "/" + b;
}

std::string dirname(const std::string &p) { return stdfs::path(p).parent_path().string(); }
std::string basename(const std::string &p) { return stdfs::path(p).filename().string(); }

std::string home() {
    const char *h = std::getenv("HOME");
    return h ? h : "/tmp";
}

std::string exe_dir() {
    std::error_code ec;
    auto p = stdfs::read_symlink("/proc/self/exe", ec);
    if (ec) return ".";
    return p.parent_path().string();
}

std::string absolute(const std::string &p) {
    std::error_code ec;
    auto a = stdfs::weakly_canonical(stdfs::absolute(p, ec), ec);
    return a.string();
}

std::string pretty(const std::string &p) {
    std::string h = home();
    if (!h.empty() && str::starts_with(p, h + "/")) return "~" + p.substr(h.size());
    return p;
}

long long mtime(const std::string &p) {
    std::error_code ec;
    auto t = stdfs::last_write_time(p, ec);
    if (ec) return 0;
    return (long long)t.time_since_epoch().count();
}

std::optional<std::string> which(const std::string &name) {
    if (name.find('/') != std::string::npos) return exists(name) ? std::optional<std::string>(name) : std::nullopt;
    const char *path = std::getenv("PATH");
    if (!path) return std::nullopt;
    for (auto &dir : str::split(path, ':', false)) {
        std::string c = join(dir, name);
        if (::access(c.c_str(), X_OK) == 0) return c;
    }
    return std::nullopt;
}

}  // namespace leet::fs
