#include "judge/process.hpp"

#include <fcntl.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <thread>

#include "util/fs.hpp"

namespace leet::judge {

namespace {
void redirect(const std::string &path, int fd, int flags) {
    if (path.empty()) {
        int n = ::open("/dev/null", flags, 0644);
        if (n >= 0) { ::dup2(n, fd); ::close(n); }
        return;
    }
    int f = ::open(path.c_str(), flags, 0644);
    if (f >= 0) { ::dup2(f, fd); ::close(f); }
}
}  // namespace

ProcResult run(const std::vector<std::string> &argv, const std::string &in, const std::string &out,
               const std::string &errp, const Limits &lim) {
    ProcResult r;
    std::vector<char *> args;
    for (auto &a : argv) args.push_back(const_cast<char *>(a.c_str()));
    args.push_back(nullptr);

    auto t0 = std::chrono::steady_clock::now();
    pid_t pid = ::fork();
    if (pid < 0) {
        r.error = std::string("fork failed: ") + std::strerror(errno);
        return r;
    }
    if (pid == 0) {
        ::setpgid(0, 0);
        redirect(in, 0, O_RDONLY);
        redirect(out, 1, O_WRONLY | O_CREAT | O_TRUNC);
        redirect(errp, 2, O_WRONLY | O_CREAT | O_TRUNC);
        if (lim.apply) {
            struct rlimit rl;
            rl.rlim_cur = rl.rlim_max = (rlim_t)lim.stack_bytes;
            struct rlimit cur;
            if (::getrlimit(RLIMIT_STACK, &cur) == 0 && cur.rlim_max != RLIM_INFINITY && cur.rlim_max < rl.rlim_cur)
                rl.rlim_cur = rl.rlim_max = cur.rlim_max;
            ::setrlimit(RLIMIT_STACK, &rl);
            if (lim.memory_bytes > 0) {
                rl.rlim_cur = rl.rlim_max = (rlim_t)lim.memory_bytes;
                ::setrlimit(RLIMIT_AS, &rl);
            }
            rl.rlim_cur = (rlim_t)(lim.timeout_s + 1.0);
            rl.rlim_max = rl.rlim_cur + 1;
            ::setrlimit(RLIMIT_CPU, &rl);
            rl.rlim_cur = rl.rlim_max = 0;
            ::setrlimit(RLIMIT_CORE, &rl);
        }
        ::execvp(args[0], args.data());
        std::fprintf(stderr, "exec %s failed: %s\n", args[0], std::strerror(errno));
        ::_exit(127);
    }
    r.started = true;
    int status = 0;
    struct rusage ru;
    std::memset(&ru, 0, sizeof ru);
    auto deadline = t0 + std::chrono::milliseconds((long long)(lim.timeout_s * 1000));
    int sleep_us = 100;
    while (true) {
        pid_t w = ::wait4(pid, &status, WNOHANG, &ru);
        if (w == pid) break;
        if (w < 0 && errno != EINTR) break;
        if (std::chrono::steady_clock::now() > deadline) {
            r.timed_out = true;
            ::kill(-pid, SIGKILL);
            ::kill(pid, SIGKILL);
            ::wait4(pid, &status, 0, &ru);
            break;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(sleep_us));
        if (sleep_us < 5000) sleep_us *= 2;
    }
    r.wall_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    r.max_rss_kb = ru.ru_maxrss;
    if (WIFEXITED(status)) r.exit_code = WEXITSTATUS(status);
    else if (WIFSIGNALED(status)) r.signal = WTERMSIG(status);
    if (r.signal == SIGXCPU || r.signal == SIGKILL) {
        if (r.wall_ms >= lim.timeout_s * 1000 * 0.9 || r.signal == SIGXCPU) r.timed_out = true;
    }
    return r;
}

ProcResult run_capture(const std::vector<std::string> &argv, std::string &output, double timeout_s) {
    char tmpl[] = "/tmp/leet-capture-XXXXXX";
    int fd = ::mkstemp(tmpl);
    if (fd >= 0) ::close(fd);
    Limits lim;
    lim.timeout_s = timeout_s;
    lim.apply = false;
    ProcResult r = run(argv, "", tmpl, tmpl, lim);
    output = fs::read_file(tmpl).value_or("");
    ::unlink(tmpl);
    return r;
}

std::string signal_name(int sig) {
    switch (sig) {
        case SIGSEGV: return "SIGSEGV";
        case SIGABRT: return "SIGABRT";
        case SIGFPE: return "SIGFPE";
        case SIGBUS: return "SIGBUS";
        case SIGILL: return "SIGILL";
        case SIGKILL: return "SIGKILL";
        case SIGXCPU: return "SIGXCPU";
        case SIGTRAP: return "SIGTRAP";
        default: return "signal " + std::to_string(sig);
    }
}

std::string signal_explanation(int sig) {
    switch (sig) {
        case SIGSEGV: return "segmentation fault: invalid memory access (null pointer, out-of-bounds index, or stack overflow)";
        case SIGABRT: return "aborted: failed assertion, uncaught exception, or heap corruption detected by the allocator";
        case SIGFPE: return "arithmetic error: integer division or modulo by zero";
        case SIGBUS: return "bus error: misaligned or invalid memory access";
        case SIGILL: return "illegal instruction: often a missing return statement in a non-void function (UB)";
        case SIGKILL: return "killed: exceeded a resource limit";
        case SIGXCPU: return "CPU time limit exceeded";
        default: return "terminated by " + signal_name(sig);
    }
}

}  // namespace leet::judge
