#include "judge/builder.hpp"

#include <unistd.h>

#include <chrono>
#include <functional>
#include <thread>

#include "judge/process.hpp"
#include "ui/term.hpp"
#include "util/fs.hpp"
#include "util/strings.hpp"

namespace leet::judge {

namespace {

const char *kCountFlag = "-fsanitize-coverage=trace-pc";

std::vector<std::string> cxx_flags(bool sanitize, bool count = false) {
    std::vector<std::string> f = {"-std=c++17", "-pipe"};
    if (count) {
        f.push_back("-O2");
        f.push_back(kCountFlag);
        f.push_back("-DLC_COUNT");
        return f;
    }
    if (sanitize) {
        for (const char *x : {"-O1", "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                              "-fno-sanitize-recover=undefined", "-DLC_NO_TRACK"})
            f.push_back(x);
    } else {
        f.push_back("-O2");
    }
    return f;
}

std::vector<std::string> c_flags(bool sanitize, bool count = false) {
    std::vector<std::string> f = {"-std=c11", "-pipe", "-Wall", "-Wno-unused-variable", "-Wno-unused-parameter"};
    if (count) {
        f.push_back("-O2");
        f.push_back(kCountFlag);
        return f;
    }
    if (sanitize) {
        for (const char *x : {"-O1", "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                              "-fno-sanitize-recover=undefined"})
            f.push_back(x);
    } else {
        f.push_back("-O2");
    }
    return f;
}

// Unique temporary suffix so concurrent builds never share a half-written file.
std::string tmp_suffix() {
    return ".tmp" + std::to_string(::getpid()) + "-" +
           std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id()) % 1000000);
}

bool is_gcc(const std::string &cxx) {
    std::string b = fs::basename(cxx);
    return b.find("g++") != std::string::npos && b.find("clang") == std::string::npos;
}

// Tidy compiler output: make paths short and drop the noise GCC adds.
std::string tidy_log(const std::string &log, const std::string &work) {
    std::string out;
    for (auto &line : str::split_lines(log)) {
        std::string l = str::replace_all(line, work + "/", "");
        std::string plain = str::strip_ansi(l);
        if (plain.find("In file included from tu-") != std::string::npos) continue;
        if (plain.find("from tu-") != std::string::npos) continue;
        out += l + "\n";
    }
    return out;
}

}  // namespace

std::string Builder::work_dir(const Problem &p) const { return fs::join(cfg_.build_dir, p.file_stem()); }

// Operation counting relies on GCC's -fsanitize-coverage=trace-pc (clang's
// variant wants its sanitizer runtime), so it is only used with GCC.
bool Builder::can_count() const {
    std::string c = fs::basename(cfg_.cc);
    return is_gcc(cfg_.cxx) && c.find("clang") == std::string::npos;
}

std::string Builder::counter_object(std::string &log) {
    std::string src = fs::join(cfg_.runtime_dir, "lc/cov.c");
    auto content = fs::read_file(src).value_or("");
    std::string obj = fs::join(cfg_.build_dir, "cov-" + str::hex(str::fnv1a(content + cfg_.cc)).substr(0, 12) + ".o");
    if (fs::exists(obj)) return obj;
    fs::mkdirs(cfg_.build_dir);
    std::string out;
    std::string tmp = obj + tmp_suffix();
    auto r = run_capture({cfg_.cc, "-O2", "-c", src, "-o", tmp}, out);
    if (!r.ok()) { log += out; fs::remove(tmp); return ""; }
    std::rename(tmp.c_str(), obj.c_str());
    return obj;
}

// Precompile the (heavy) harness header once per compiler+flags combination.
std::string Builder::pch_dir(const std::vector<std::string> &flags, std::string &log) {
    std::string harness = fs::join(cfg_.runtime_dir, "lc/harness.hpp");
    if (!is_gcc(cfg_.cxx)) return cfg_.runtime_dir;
    auto content = fs::read_file(harness).value_or("");
    std::string key = str::hex(str::fnv1a(content + cfg_.cxx + str::join(flags, " ")));
    std::string dir = fs::join(cfg_.build_dir, "pch-" + key.substr(0, 12));
    std::string hdr = fs::join(dir, "lc/harness.hpp");
    std::string gch = hdr + ".gch";
    if (fs::exists(gch)) return dir;
    fs::write_file(hdr, content);
    std::vector<std::string> cmd = {cfg_.cxx};
    cmd.insert(cmd.end(), flags.begin(), flags.end());
    std::string tmp = gch + tmp_suffix();
    cmd.insert(cmd.end(), {"-x", "c++-header", hdr, "-o", tmp});
    std::string out;
    auto r = run_capture(cmd, out);
    if (!r.ok()) {
        log += out;
        fs::remove(tmp);
        return cfg_.runtime_dir;  // fall back to no PCH
    }
    std::rename(tmp.c_str(), gch.c_str());
    return dir;
}

BuildResult Builder::build(const Problem &p, const std::string &source_path, const BuildOptions &opt) {
    BuildResult res;
    auto t0 = std::chrono::steady_clock::now();
    std::string work = work_dir(p);
    fs::mkdirs(work);

    auto src = fs::read_file(source_path);
    if (!src) {
        res.log = "cannot read " + source_path;
        return res;
    }
    std::string abs_src = fs::absolute(source_path);
    if (opt.count && !can_count()) {
        res.log = "operation counting needs GCC";
        return res;
    }
    std::vector<std::string> flags = cxx_flags(opt.sanitize, opt.count);
    std::vector<std::string> cflags = c_flags(opt.sanitize, opt.count);
    if (ui::color()) {
        flags.push_back("-fdiagnostics-color=always");
        cflags.push_back("-fdiagnostics-color=always");
    }

    std::string harness = fs::read_file(fs::join(cfg_.runtime_dir, "lc/harness.hpp")).value_or("");
    std::string prelude = fs::read_file(fs::join(cfg_.runtime_dir, "lc/c_prelude.h")).value_or("");
    std::string tag = !opt.tag.empty() ? opt.tag : opt.reference ? "ref" : (opt.sanitize ? "asan" : "user");
    if (opt.count) tag += "count";
    std::string key = str::hex(str::fnv1a(*src + "\x1f" + p.driver + "\x1f" + harness + "\x1f" + prelude + "\x1f" +
                                          str::join(flags, " ") + cfg_.cxx + cfg_.cc + abs_src + "v3"));
    res.binary = fs::join(work, tag + "-" + key.substr(0, 16));
    if (fs::exists(res.binary)) {
        res.ok = res.cached = true;
        return res;
    }
    // Remove stale binaries of the same kind.
    for (auto &f : fs::list_dir(work))
        if (str::starts_with(f, tag + "-")) fs::remove(fs::join(work, f));

    std::vector<std::string> base_flags = cxx_flags(opt.sanitize, opt.count);
    std::string inc = pch_dir(base_flags, res.log);

    // Translation unit: harness, (C++) user solution, driver.
    std::string tu = "#include \"lc/harness.hpp\"\n";
    if (!p.is_c()) tu += "#include \"" + abs_src + "\"\n";
    else tu += "// C solution is compiled separately and linked in\n";
    tu += "#line 1 \"[driver for " + p.slug + "]\"\n" + p.driver;
    std::string tu_path = fs::join(work, "tu-" + tag + ".cpp");
    fs::write_file(tu_path, tu);

    std::vector<std::string> link_objs;
    if (opt.count) {
        std::string cov = counter_object(res.log);
        if (cov.empty()) return res;
        link_objs.push_back(cov);
    }
    if (p.is_c()) {
        std::string obj = fs::join(work, tag + ".o");
        std::vector<std::string> cc = {cfg_.cc};
        cc.insert(cc.end(), cflags.begin(), cflags.end());
        cc.insert(cc.end(), {"-include", fs::join(cfg_.runtime_dir, "lc/c_prelude.h"), "-c", abs_src, "-o", obj});
        std::string out;
        auto r = run_capture(cc, out);
        res.command = str::join(cc, " ");
        if (!r.ok()) {
            res.log += tidy_log(out, work);
            res.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            return res;
        }
        if (!opt.count) res.log += tidy_log(out, work);  // warnings are useful in C
        link_objs.push_back(obj);
    }

    std::vector<std::string> cmd = {cfg_.cxx};
    cmd.insert(cmd.end(), flags.begin(), flags.end());
    cmd.push_back("-I" + inc);
    cmd.push_back(tu_path);
    cmd.insert(cmd.end(), link_objs.begin(), link_objs.end());
    cmd.insert(cmd.end(), {"-lm", "-o", res.binary + ".tmp"});
    std::string out;
    auto r = run_capture(cmd, out);
    res.command += (res.command.empty() ? "" : "\n") + str::join(cmd, " ");
    res.log += tidy_log(out, work);
    if (r.ok() && fs::exists(res.binary + ".tmp")) {
        std::rename((res.binary + ".tmp").c_str(), res.binary.c_str());
        res.ok = true;
    }
    res.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    return res;
}

BuildResult Builder::build_reference(const Problem &p, bool count) {
    std::string path = fs::join(work_dir(p), "reference." + p.ext());
    auto cur = fs::read_file(path);
    if (!cur || *cur != p.solution) fs::write_file(path, p.solution);
    BuildOptions o;
    o.reference = true;
    o.count = count;
    return build(p, path, o);
}

}  // namespace leet::judge
