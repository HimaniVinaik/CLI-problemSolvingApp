// ============================================================================
//  leet judge runtime  —  lc/harness.hpp
// ----------------------------------------------------------------------------
//  Every problem is compiled as ONE translation unit:
//
//      #include "lc/harness.hpp"        <- this file (LeetCode-like prelude)
//      #include "<your solution file>"  <- class Solution { ... };
//      <problem driver>                 <- LC_DRIVER { h.solve(&Solution::f); }
//
//  The resulting executable understands three sub-commands used by the judge:
//
//      drv run   <input> <output> <stats>   run one test case
//      drv gen   <n> <seed>                 print a random test case of size n
//      drv bench <n> <seed> <stats>         time/space profile on a size-n case
//
//  Test-case text format (one argument per line, LeetCode literal syntax):
//      [2,7,11,15]
//      9
// ============================================================================
#pragma once

#include <bits/stdc++.h>
#include <malloc.h>
#include <sys/resource.h>
#include <unistd.h>

using namespace std;

// ---------------------------------------------------------------------------
//  LeetCode data structures (identical to the ones LeetCode pre-defines)
// ---------------------------------------------------------------------------
struct ListNode {
    int val;
    ListNode *next;
    ListNode() : val(0), next(nullptr) {}
    ListNode(int x) : val(x), next(nullptr) {}
    ListNode(int x, ListNode *next) : val(x), next(next) {}
};

struct TreeNode {
    int val;
    TreeNode *left;
    TreeNode *right;
    TreeNode() : val(0), left(nullptr), right(nullptr) {}
    TreeNode(int x) : val(x), left(nullptr), right(nullptr) {}
    TreeNode(int x, TreeNode *left, TreeNode *right) : val(x), left(left), right(right) {}
};

// ---------------------------------------------------------------------------
//  Heap accounting: malloc/free are interposed so that every allocation made
//  by the solution (C malloc *and* C++ new) is counted while a measurement
//  window is open.  Disabled for sanitizer builds (-DLC_NO_TRACK).
// ---------------------------------------------------------------------------
namespace lc { namespace mem {
inline bool      on   = false;
inline long long cur  = 0;
inline long long peak = 0;
inline void add(long long d) { cur += d; if (cur > peak) peak = cur; }
inline void reset() { cur = 0; peak = 0; }
}}  // namespace lc::mem

#ifndef LC_NO_TRACK
extern "C" {
void *__libc_malloc(size_t);
void  __libc_free(void *);
void *__libc_calloc(size_t, size_t);
void *__libc_realloc(void *, size_t);

void *malloc(size_t n) noexcept {
    void *p = __libc_malloc(n);
    if (p && lc::mem::on) lc::mem::add((long long)malloc_usable_size(p));
    return p;
}
void free(void *p) noexcept {
    if (p && lc::mem::on) lc::mem::add(-(long long)malloc_usable_size(p));
    __libc_free(p);
}
void *calloc(size_t a, size_t b) noexcept {
    void *p = __libc_calloc(a, b);
    if (p && lc::mem::on) lc::mem::add((long long)malloc_usable_size(p));
    return p;
}
void *realloc(void *p, size_t n) noexcept {
    long long before = p ? (long long)malloc_usable_size(p) : 0;
    void *q = __libc_realloc(p, n);
    if (lc::mem::on) {
        if (q) lc::mem::add((long long)malloc_usable_size(q) - before);
        else if (n == 0) lc::mem::add(-before);
    }
    return q;
}
}  // extern "C"
#endif

namespace lc {

// ---------------------------------------------------------------------------
//  Stack accounting ("stack painting"): before the measured call we fill a
//  region below the current stack pointer with a marker pattern; afterwards
//  we find the deepest word that was overwritten.  This captures recursion
//  depth, which is part of a solution's space complexity.
// ---------------------------------------------------------------------------
namespace stk {
inline uint64_t *lo = nullptr;
inline size_t    words = 0;
constexpr uint64_t kPattern = 0xA5C3A5C3A5C3A5C3ull;

__attribute__((noinline)) inline void paint(size_t bytes) {
    void *p = alloca(bytes);
    uint64_t *w = (uint64_t *)p;
    size_t n = bytes / sizeof(uint64_t);
    for (size_t i = 0; i < n; ++i) w[i] = kPattern;
    lo = w;
    words = n;
    asm volatile("" : : "r"(p) : "memory");
}
__attribute__((noinline)) inline long long scan() {
    if (!lo) return 0;
    volatile uint64_t *w = lo;
    size_t i = 0;
    while (i < words && w[i] == kPattern) ++i;
    return (long long)((words - i) * sizeof(uint64_t));
}
inline size_t budget() {
    struct rlimit rl;
    size_t want = 256ull << 20;  // 256 MiB is plenty for any bench size we use
    if (getrlimit(RLIMIT_STACK, &rl) == 0 && rl.rlim_cur != RLIM_INFINITY) {
        size_t cap = rl.rlim_cur > (64ull << 20) ? rl.rlim_cur - (32ull << 20) : 0;
        want = std::min(want, cap);
    }
    return want;
}
}  // namespace stk

// Keeps the optimizer from deleting or hoisting the code under measurement.
template <class T> inline void escape(T *p) { asm volatile("" : : "g"(p) : "memory"); }
inline void clobber() { asm volatile("" : : : "memory"); }

}  // namespace lc

// Operation counting build (see lc/cov.c): basic blocks executed by the call.
#ifdef LC_COUNT
extern "C" {
extern uint64_t lc_cov_count;
extern int lc_cov_on;
}
#endif

namespace lc {
namespace ops {
inline void start() {
#ifdef LC_COUNT
    clobber();
    lc_cov_count = 0;
    lc_cov_on = 1;
    clobber();
#endif
}
inline long long stop() {
#ifdef LC_COUNT
    clobber();
    lc_cov_on = 0;
    clobber();
    return (long long)lc_cov_count;
#else
    return 0;
#endif
}
}  // namespace ops

struct InputError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// ---------------------------------------------------------------------------
//  Value: a tiny JSON-like tree for LeetCode literals
//      [1,2,null]  "abc"  'c'  true  -3.5  [[1,2],[3]]
// ---------------------------------------------------------------------------
struct Value {
    enum Kind { Null, Bool, Num, Str, Arr } kind = Null;
    bool b = false;
    std::string s;               // number text or string contents
    std::vector<Value> a;

    bool is_null() const { return kind == Null; }
    const char *kind_name() const {
        static const char *names[] = {"null", "bool", "number", "string", "array"};
        return names[kind];
    }
};

class Parser {
    const std::string &t;
    size_t i = 0;
    [[noreturn]] void fail(const std::string &why) {
        throw InputError(why + " at column " + std::to_string(i + 1) + " in: " + t);
    }
    void ws() { while (i < t.size() && isspace((unsigned char)t[i])) ++i; }
    std::string quoted(char q) {
        ++i;
        std::string out;
        while (i < t.size() && t[i] != q) {
            if (t[i] == '\\' && i + 1 < t.size()) {
                char e = t[++i];
                switch (e) {
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case 'r': out += '\r'; break;
                    case '0': out += '\0'; break;
                    default: out += e;
                }
                ++i;
            } else {
                out += t[i++];
            }
        }
        if (i >= t.size()) fail("unterminated string");
        ++i;
        return out;
    }

public:
    explicit Parser(const std::string &text) : t(text) {}
    Value parse_all() {
        Value v = parse();
        ws();
        if (i != t.size()) fail("unexpected trailing characters");
        return v;
    }
    Value parse() {
        ws();
        if (i >= t.size()) fail("expected a value");
        Value v;
        char c = t[i];
        if (c == '[') {
            v.kind = Value::Arr;
            ++i;
            ws();
            if (i < t.size() && t[i] == ']') { ++i; return v; }
            while (true) {
                v.a.push_back(parse());
                ws();
                if (i < t.size() && t[i] == ',') { ++i; continue; }
                if (i < t.size() && t[i] == ']') { ++i; break; }
                fail("expected ',' or ']'");
            }
        } else if (c == '"' || c == '\'') {
            v.kind = Value::Str;
            v.s = quoted(c);
        } else if (t.compare(i, 4, "null") == 0) {
            i += 4;
        } else if (t.compare(i, 4, "true") == 0) {
            v.kind = Value::Bool; v.b = true; i += 4;
        } else if (t.compare(i, 5, "false") == 0) {
            v.kind = Value::Bool; v.b = false; i += 5;
        } else if (c == '-' || c == '+' || c == '.' || isdigit((unsigned char)c)) {
            v.kind = Value::Num;
            size_t st = i;
            ++i;
            while (i < t.size() && (isalnum((unsigned char)t[i]) || t[i] == '.' || t[i] == '-' || t[i] == '+')) ++i;
            v.s = t.substr(st, i - st);
        } else {
            fail(std::string("unexpected character '") + c + "'");
        }
        return v;
    }
};

inline Value parse_value(const std::string &line) { return Parser(line).parse_all(); }

// ---------------------------------------------------------------------------
//  Conv<T>: Value -> T  and  T -> LeetCode text
// ---------------------------------------------------------------------------
template <class T, class = void> struct Conv;

inline std::string quote(const std::string &s) {
    std::string o = "\"";
    for (char c : s) {
        switch (c) {
            case '"': o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break;
            case '\t': o += "\\t"; break;
            default: o += c;
        }
    }
    return o + "\"";
}

template <class T>
struct Conv<T, std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool> && !std::is_same_v<T, char>>> {
    static T from(const Value &v) {
        if (v.kind == Value::Bool) return (T)v.b;
        if (v.kind != Value::Num) throw InputError(std::string("expected an integer, got ") + v.kind_name());
        try {
            if constexpr (std::is_unsigned_v<T>) return (T)std::stoull(v.s, nullptr, 0);
            else return (T)std::stoll(v.s);
        } catch (...) {
            throw InputError("bad integer literal: " + v.s);
        }
    }
    static void print(std::string &o, T x) { o += std::to_string(x); }
};

template <class T> struct Conv<T, std::enable_if_t<std::is_floating_point_v<T>>> {
    static T from(const Value &v) {
        if (v.kind != Value::Num) throw InputError(std::string("expected a number, got ") + v.kind_name());
        return (T)std::stod(v.s);
    }
    static void print(std::string &o, T x) {
        char buf[64];
        std::snprintf(buf, sizeof buf, "%.5f", (double)x);
        if (std::strcmp(buf, "-0.00000") == 0) std::strcpy(buf, "0.00000");
        o += buf;
    }
};

template <> struct Conv<bool> {
    static bool from(const Value &v) {
        if (v.kind == Value::Bool) return v.b;
        if (v.kind == Value::Num) return std::stoll(v.s) != 0;
        throw InputError(std::string("expected true/false, got ") + v.kind_name());
    }
    static void print(std::string &o, bool x) { o += x ? "true" : "false"; }
};

template <> struct Conv<char> {
    static char from(const Value &v) {
        if (v.kind == Value::Str && v.s.size() == 1) return v.s[0];
        if (v.kind == Value::Num && v.s.size() == 1) return v.s[0];
        throw InputError("expected a single character like \"a\"");
    }
    static void print(std::string &o, char c) { o += quote(std::string(1, c)); }
};

template <> struct Conv<std::string> {
    static std::string from(const Value &v) {
        if (v.kind == Value::Str) return v.s;
        throw InputError(std::string("expected a string in quotes, got ") + v.kind_name());
    }
    static void print(std::string &o, const std::string &s) { o += quote(s); }
};

template <class T> struct Conv<std::vector<T>> {
    static std::vector<T> from(const Value &v) {
        if (v.kind != Value::Arr) throw InputError(std::string("expected an array, got ") + v.kind_name());
        std::vector<T> out;
        out.reserve(v.a.size());
        for (auto &e : v.a) out.push_back(Conv<T>::from(e));
        return out;
    }
    static void print(std::string &o, const std::vector<T> &xs) {
        o += '[';
        for (size_t i = 0; i < xs.size(); ++i) {
            if (i) o += ',';
            Conv<T>::print(o, xs[i]);
        }
        o += ']';
    }
};

template <> struct Conv<std::vector<bool>> {
    static std::vector<bool> from(const Value &v) {
        if (v.kind != Value::Arr) throw InputError("expected an array");
        std::vector<bool> out;
        for (auto &e : v.a) out.push_back(Conv<bool>::from(e));
        return out;
    }
    static void print(std::string &o, const std::vector<bool> &xs) {
        o += '[';
        for (size_t i = 0; i < xs.size(); ++i) { if (i) o += ','; o += xs[i] ? "true" : "false"; }
        o += ']';
    }
};

template <class A, class B> struct Conv<std::pair<A, B>> {
    static std::pair<A, B> from(const Value &v) {
        if (v.kind != Value::Arr || v.a.size() != 2) throw InputError("expected a pair [a,b]");
        return {Conv<A>::from(v.a[0]), Conv<B>::from(v.a[1])};
    }
    static void print(std::string &o, const std::pair<A, B> &p) {
        o += '[';
        Conv<A>::print(o, p.first);
        o += ',';
        Conv<B>::print(o, p.second);
        o += ']';
    }
};

// ----- linked lists ---------------------------------------------------------
inline ListNode *make_list(const std::vector<int> &xs) {
    ListNode dummy, *t = &dummy;
    for (int x : xs) { t->next = new ListNode(x); t = t->next; }
    return dummy.next;
}
inline std::vector<int> list_values(ListNode *h, size_t cap = 5000000) {
    std::vector<int> out;
    while (h && out.size() < cap) { out.push_back(h->val); h = h->next; }
    if (h) throw std::runtime_error("returned linked list is too long (cycle?)");
    return out;
}
template <> struct Conv<ListNode *> {
    static ListNode *from(const Value &v) { return make_list(Conv<std::vector<int>>::from(v)); }
    static void print(std::string &o, ListNode *h) { Conv<std::vector<int>>::print(o, list_values(h)); }
};

// ----- binary trees (LeetCode level-order with nulls) -----------------------
inline TreeNode *make_tree(const Value &v) {
    if (v.kind != Value::Arr) throw InputError("expected a tree like [1,null,2]");
    if (v.a.empty() || v.a[0].is_null()) return nullptr;
    auto node = [&](const Value &x) -> TreeNode * {
        return x.is_null() ? nullptr : new TreeNode(Conv<int>::from(x));
    };
    TreeNode *root = node(v.a[0]);
    std::queue<TreeNode *> q;
    q.push(root);
    size_t i = 1;
    while (!q.empty() && i < v.a.size()) {
        TreeNode *cur = q.front(); q.pop();
        if (i < v.a.size()) { cur->left = node(v.a[i++]); if (cur->left) q.push(cur->left); }
        if (i < v.a.size()) { cur->right = node(v.a[i++]); if (cur->right) q.push(cur->right); }
    }
    return root;
}
inline std::vector<std::optional<int>> tree_values(TreeNode *root) {
    std::vector<std::optional<int>> out;
    if (!root) return out;
    std::queue<TreeNode *> q;
    q.push(root);
    size_t guard = 0;
    while (!q.empty()) {
        TreeNode *c = q.front(); q.pop();
        if (++guard > 20000000) throw std::runtime_error("returned tree is too large (cycle?)");
        if (!c) { out.push_back(std::nullopt); continue; }
        out.push_back(c->val);
        q.push(c->left);
        q.push(c->right);
    }
    while (!out.empty() && !out.back()) out.pop_back();
    return out;
}
template <> struct Conv<TreeNode *> {
    static TreeNode *from(const Value &v) { return make_tree(v); }
    static void print(std::string &o, TreeNode *r) {
        auto vals = tree_values(r);
        o += '[';
        for (size_t i = 0; i < vals.size(); ++i) {
            if (i) o += ',';
            o += vals[i] ? std::to_string(*vals[i]) : "null";
        }
        o += ']';
    }
};
inline TreeNode *find_node(TreeNode *r, int val) {
    if (!r) return nullptr;
    if (r->val == val) return r;
    if (TreeNode *l = find_node(r->left, val)) return l;
    return find_node(r->right, val);
}

template <class T> std::string to_text(const T &v) {
    std::string o;
    Conv<T>::print(o, v);
    return o;
}
inline std::string to_text(const char *s) { return quote(s); }

// Serialise a list of values as input lines (used by generators).
template <class... T> std::vector<std::string> lines(const T &...v) { return {to_text(v)...}; }

// ---------------------------------------------------------------------------
//  Random input generator helpers
// ---------------------------------------------------------------------------
struct Gen {
    std::mt19937_64 rng;
    explicit Gen(uint64_t seed) : rng(seed * 0x9E3779B97F4A7C15ull + 12345) {}

    long long range(long long lo, long long hi) {  // inclusive
        if (hi < lo) std::swap(lo, hi);
        return lo + (long long)(rng() % (unsigned long long)(hi - lo + 1));
    }
    int irange(long long lo, long long hi) { return (int)range(lo, hi); }
    double real(double lo, double hi) { return lo + (hi - lo) * (double)(rng() >> 11) / 9007199254740992.0; }
    bool coin(double p = 0.5) { return real(0, 1) < p; }

    std::vector<int> ints(int n, long long lo, long long hi) {
        std::vector<int> v(std::max(n, 0));
        for (auto &x : v) x = (int)range(lo, hi);
        return v;
    }
    std::vector<int> sorted_ints(int n, long long lo, long long hi) {
        auto v = ints(n, lo, hi);
        std::sort(v.begin(), v.end());
        return v;
    }
    // n distinct values in [lo, hi] (requires hi-lo+1 >= n), random order
    std::vector<int> distinct(int n, long long lo, long long hi) {
        std::vector<int> v;
        long long span = hi - lo + 1;
        if (span < n) n = (int)span;
        if (span <= 4ll * n) {
            for (long long x = lo; x <= hi; ++x) v.push_back((int)x);
            shuffle(v);
            v.resize(n);
        } else {
            std::unordered_set<long long> seen;
            while ((int)v.size() < n) {
                long long x = range(lo, hi);
                if (seen.insert(x).second) v.push_back((int)x);
            }
        }
        return v;
    }
    std::vector<int> perm(int n, int base = 0) {
        std::vector<int> v(n);
        std::iota(v.begin(), v.end(), base);
        shuffle(v);
        return v;
    }
    template <class T> void shuffle(std::vector<T> &v) { std::shuffle(v.begin(), v.end(), rng); }
    template <class T> const T &pick(const std::vector<T> &v) { return v[range(0, (long long)v.size() - 1)]; }

    std::string str(int n, const std::string &alphabet) {
        std::string s(std::max(n, 0), ' ');
        for (auto &c : s) c = alphabet[range(0, (long long)alphabet.size() - 1)];
        return s;
    }
    std::string lower(int n, int k = 26) { return str(n, std::string("abcdefghijklmnopqrstuvwxyz").substr(0, k)); }

    // random binary tree shape with n nodes (expected O(log n) height)
    TreeNode *tree(int n, long long lo, long long hi) {
        if (n <= 0) return nullptr;
        std::vector<TreeNode *> open;
        TreeNode *root = new TreeNode((int)range(lo, hi));
        open.push_back(root);
        for (int i = 1; i < n; ++i) {
            size_t k = (size_t)range(0, (long long)open.size() - 1);
            TreeNode *p = open[k];
            TreeNode *c = new TreeNode((int)range(lo, hi));
            bool left = (!p->left && !p->right) ? coin() : !p->left;
            (left ? p->left : p->right) = c;
            if (p->left && p->right) { open[k] = open.back(); open.pop_back(); }
            open.push_back(c);
        }
        return root;
    }
    // BST built by inserting distinct random keys in random order
    TreeNode *bst(int n, long long lo, long long hi) {
        auto keys = distinct(n, lo, hi);
        TreeNode *root = nullptr;
        for (int k : keys) {
            TreeNode **cur = &root;
            while (*cur) cur = k < (*cur)->val ? &(*cur)->left : &(*cur)->right;
            *cur = new TreeNode(k);
        }
        return root;
    }
};

// Builds the two input lines of a "design" problem:
//   ["LRUCache","put","get"]
//   [[2],[1,1],[1]]
struct Ops {
    std::vector<std::string> names;
    std::vector<std::string> args;
    template <class... T> Ops &add(const std::string &name, const T &...a) {
        names.push_back(name);
        std::string s = "[";
        bool first = true;
        ((s += (first ? "" : ","), s += to_text(a), first = false), ...);
        args.push_back(s + "]");
        return *this;
    }
    std::vector<std::string> lines() const {
        std::string a = "[", b = "[";
        for (size_t i = 0; i < names.size(); ++i) {
            if (i) { a += ','; b += ','; }
            a += quote(names[i]);
            b += args[i];
        }
        return {a + "]", b + "]"};
    }
};

// ---------------------------------------------------------------------------
//  Case: one execution of the solution (a test, or one benchmark repetition)
// ---------------------------------------------------------------------------
enum class Mode { Run, Bench };

struct Stats {
    double ns = 0;          // time of the measured call(s), per call
    long long heap = 0;     // peak extra heap bytes during the call
    long long stack = 0;    // deepest stack usage during the call
    long long ops = 0;      // basic blocks executed (LC_COUNT builds only)
};

class Case {
    std::vector<std::string> lines_;
    size_t pos_ = 0;
    std::string out_;
    bool timed_ = false;

public:
    Mode mode = Mode::Run;
    bool readonly = false;   // bench: call may be repeated on the same input
    bool measure_stack = false;
    Stats stats;

    explicit Case(std::vector<std::string> l) : lines_(std::move(l)) {}

    size_t arg_count() const { return lines_.size(); }
    Value value() {
        if (pos_ >= lines_.size())
            throw InputError("missing input line #" + std::to_string(pos_ + 1) + " (one argument per line)");
        return parse_value(lines_[pos_++]);
    }
    template <class T> T arg() {
        size_t idx = pos_;
        Value v = value();
        try {
            return Conv<T>::from(v);
        } catch (InputError &e) {
            throw InputError("argument #" + std::to_string(idx + 1) + ": " + e.what());
        }
    }

    // Measure the solution call.  In bench+readonly mode the call is repeated
    // in a tight loop and the per-call average is reported.
    template <class F> void time(F &&f) {
        using clk = std::chrono::steady_clock;
        if (measure_stack) stk::paint(stk::budget());
        mem::reset();
        mem::on = true;
#ifdef LC_COUNT
        if (true) {  // counting is deterministic: a single call suffices
            ops::start();
            f();
            stats.ops = ops::stop();
        } else
#endif
        if (mode == Mode::Bench && readonly) {
            size_t k = 1;
            while (true) {
                auto t0 = clk::now();
                for (size_t i = 0; i < k; ++i) { f(); clobber(); }
                auto t1 = clk::now();
                double ns = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
                if (ns >= 3e6 || k >= (1u << 22)) { stats.ns = ns / (double)k; break; }
                k *= 2;
            }
        } else {
            auto t0 = clk::now();
            f();
            clobber();
            auto t1 = clk::now();
            stats.ns = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
        }
        mem::on = false;
        stats.heap = std::max(0ll, mem::peak);
        if (measure_stack) stats.stack = stk::scan();
        timed_ = true;
    }
    bool timed() const { return timed_; }

    template <class T> void out(const T &v) { Conv<T>::print(out_, v); }
    void out_text(const std::string &s) { out_ += s; }
    const std::string &output() const { return out_; }
};

// ---------------------------------------------------------------------------
//  Output normalisation for "any order" answers
// ---------------------------------------------------------------------------
// Pass a parsed argument to the solution.  Reference parameters get the
// stored object; by-value parameters get it moved in (when the call happens
// only once), so the judge's copy is not billed as the solution's memory.
template <class A, class T> decltype(auto) pass(T &x, bool move_ok) {
    if constexpr (std::is_reference_v<A>) return (x);
    else return move_ok ? T(std::move(x)) : T(x);
}

template <class T> struct is_vec : std::false_type {};
template <class T, class A> struct is_vec<std::vector<T, A>> : std::true_type {};

template <class T> void sort_outer(T &v) {
    if constexpr (is_vec<T>::value) std::sort(v.begin(), v.end());
}
template <class T> void sort_deep(T &v) {
    if constexpr (is_vec<T>::value) {
        for (auto &x : v) sort_deep(x);
        std::sort(v.begin(), v.end());
    }
}

struct SolveOpts {
    bool sort = false, deep = false;
    int inplace = -1;          // print this argument (after the call) instead of the result
    int prefix = -1;           // result k: print first k items of this argument
    bool prefix_sorted = false;
    SolveOpts &sort_output() { sort = true; return *this; }
    SolveOpts &sort_deep() { deep = true; return *this; }
    SolveOpts &print_arg(int i) { inplace = i; return *this; }
    SolveOpts &print_prefix(int i, bool sorted = false) { prefix = i; prefix_sorted = sorted; return *this; }
};

template <class T> void normalise(T &v, const SolveOpts &o) {
    if (o.deep) sort_deep(v);
    else if (o.sort) sort_outer(v);
}

template <class Tup, size_t... I>
void print_arg_n(Case &c, Tup &t, int k, const SolveOpts &o, std::index_sequence<I...>) {
    (((int)I == k ? (void)([&] {
        auto v = std::get<I>(t);
        normalise(v, o);
        c.out(v);
    }()) : (void)0), ...);
}
template <class Tup, size_t... I>
void print_prefix_n(Case &c, Tup &t, int k, long long len, bool sorted, std::index_sequence<I...>) {
    (((int)I == k ? (void)([&] {
        using E = std::tuple_element_t<I, Tup>;
        if constexpr (is_vec<E>::value) {
            E v = std::get<I>(t);
            if (len < 0 || len > (long long)v.size())
                throw std::runtime_error("returned length " + std::to_string(len) + " is out of range");
            v.resize((size_t)len);
            if (sorted) std::sort(v.begin(), v.end());
            c.out(v);
        }
    }()) : (void)0), ...);
}

// ---------------------------------------------------------------------------
//  Design problems:  ["MinStack","push","getMin"]  [[],[1],[]]
// ---------------------------------------------------------------------------
template <class... A, size_t... I>
std::tuple<std::decay_t<A>...> args_from_array(const Value &v, std::index_sequence<I...>) {
    if (v.kind != Value::Arr || v.a.size() != sizeof...(A))
        throw InputError("expected an argument list with " + std::to_string(sizeof...(A)) + " value(s)");
    return std::tuple<std::decay_t<A>...>{Conv<std::decay_t<A>>::from(v.a[I])...};
}

template <class T, class... CtorArgs> class Design {
public:
    using Call = std::function<void(T &, std::string &)>;
    using Binder = std::function<Call(const Value &)>;
    std::map<std::string, Binder> methods;

    template <class R, class... A> Design &method(const std::string &name, R (T::*m)(A...)) {
        return bind<R, A...>(name, m);
    }
    template <class R, class... A> Design &method(const std::string &name, R (T::*m)(A...) const) {
        return bind<R, A...>(name, m);
    }

private:
    template <class R, class... A, class M> Design &bind(const std::string &name, M m) {
        methods[name] = [m](const Value &a) -> Call {
            auto tup = std::make_shared<std::tuple<std::decay_t<A>...>>(
                args_from_array<A...>(a, std::index_sequence_for<A...>{}));
            return [m, tup](T &obj, std::string &res) {
                if constexpr (std::is_void_v<R>) {
                    std::apply([&](auto &...x) { (obj.*m)(pass<A>(x, true)...); }, *tup);
                    res = "null";
                } else {
                    R r = std::apply([&](auto &...x) { return (obj.*m)(pass<A>(x, true)...); }, *tup);
                    res.clear();
                    Conv<std::decay_t<R>>::print(res, r);
                }
            };
        };
        return *this;
    }

public:
    void run(Case &c) const {
        Value ops = c.value(), args = c.value();
        if (ops.kind != Value::Arr || args.kind != Value::Arr || ops.a.size() != args.a.size() || ops.a.empty())
            throw InputError("design input needs two arrays of equal length: operations and arguments");
        auto ctor = std::make_shared<std::tuple<CtorArgs...>>(
            args_from_array<CtorArgs...>(args.a[0], std::index_sequence_for<CtorArgs...>{}));
        std::vector<Call> calls;
        std::vector<std::string> results(ops.a.size(), "null");
        for (size_t i = 1; i < ops.a.size(); ++i) {
            const std::string &name = Conv<std::string>::from(ops.a[i]);
            auto it = methods.find(name);
            if (it == methods.end()) throw InputError("unknown operation \"" + name + "\"");
            calls.push_back(it->second(args.a[i]));
        }
        T *obj = nullptr;
        c.time([&] {
            obj = std::apply([](auto &...x) { return new T(x...); }, *ctor);
            for (size_t i = 0; i < calls.size(); ++i) calls[i](*obj, results[i + 1]);
            escape(obj);
        });
        std::string o = "[";
        for (size_t i = 0; i < results.size(); ++i) { if (i) o += ','; o += results[i]; }
        c.out_text(o + "]");
    }
};

// ---------------------------------------------------------------------------
//  Harness: the problem driver registers how to call the solution
// ---------------------------------------------------------------------------
class Harness {
public:
    using Fn = std::function<void(Case &)>;
    using GenFn = std::function<std::vector<std::string>(Gen &, int)>;

    // Call a Solution member function with arguments parsed from the input.
    template <class C, class R, class... A> SolveOpts &solve(R (C::*f)(A...)) { return solve_impl<C, R, A...>(f); }
    template <class C, class R, class... A> SolveOpts &solve(R (C::*f)(A...) const) { return solve_impl<C, R, A...>(f); }

    template <class T, class... CtorArgs> Design<T, CtorArgs...> &design() {
        auto d = std::make_shared<Design<T, CtorArgs...>>();
        holder_ = d;
        fn_ = [d](Case &c) { d->run(c); };
        return *d;
    }

    // Fully custom driver: parse with c.arg<T>(), call inside c.time(...), print with c.out(...)
    void custom(Fn f) { fn_ = std::move(f); }
    void gen(GenFn g) { gen_ = std::move(g); }
    void readonly(bool r = true) { readonly_ = r; }

    int main(int argc, char **argv);

private:
    template <class C, class R, class... A, class F> SolveOpts &solve_impl(F f) {
        auto o = std::make_shared<SolveOpts>();
        opts_ = o;
        fn_ = [f, o](Case &c) {
            using Tup = std::tuple<std::decay_t<A>...>;
            Tup args{c.arg<std::decay_t<A>>()...};
            if (c.arg_count() > sizeof...(A)) { /* extra lines are ignored */ }
            C obj;
            escape(&args);
            const bool mv = !(c.mode == Mode::Bench && c.readonly);  // readonly benches call repeatedly
            if constexpr (std::is_void_v<R>) {
                c.time([&] { std::apply([&](auto &...a) { (obj.*f)(pass<A>(a, mv)...); }, args); });
                print_arg_n(c, args, o->inplace < 0 ? 0 : o->inplace, *o, std::index_sequence_for<A...>{});
            } else {
                std::optional<std::decay_t<R>> r;
                c.time([&] {
                    r.emplace(std::apply([&](auto &...a) { return (obj.*f)(pass<A>(a, mv)...); }, args));
                    escape(&r);
                });
                if (o->prefix >= 0) {
                    if constexpr (std::is_integral_v<std::decay_t<R>>)
                        print_prefix_n(c, args, o->prefix, (long long)*r, o->prefix_sorted, std::index_sequence_for<A...>{});
                } else if (o->inplace >= 0) {
                    print_arg_n(c, args, o->inplace, *o, std::index_sequence_for<A...>{});
                } else {
                    auto v = *r;
                    normalise(v, *o);
                    c.out(v);
                }
            }
        };
        return *o;
    }

    Fn fn_;
    GenFn gen_;
    bool readonly_ = false;
    std::shared_ptr<SolveOpts> opts_;
    std::shared_ptr<void> holder_;

    static std::vector<std::string> read_lines(const std::string &path) {
        std::ifstream in(path);
        if (!in) throw InputError("cannot open input file " + path);
        std::vector<std::string> v;
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            v.push_back(line);
        }
        while (!v.empty() && v.back().find_first_not_of(" \t") == std::string::npos) v.pop_back();
        return v;
    }
    static void write_stats(const std::string &path, const Stats &s, long long reps) {
        std::ofstream o(path);
        o << std::fixed << std::setprecision(1) << s.ns << ' ' << s.heap << ' ' << s.stack << ' ' << reps << ' '
          << s.ops << '\n';
    }
};

inline int Harness::main(int argc, char **argv) {
    std::ios::sync_with_stdio(true);
    if (!fn_) { std::fprintf(stderr, "driver error: no solve()/design()/custom() registered\n"); return 5; }
    std::string cmd = argc > 1 ? argv[1] : "";
    try {
        if (cmd == "run" && argc >= 5) {
            Case c(read_lines(argv[2]));
            fn_(c);
            std::fflush(stdout);
            std::cout.flush();
            std::ofstream(argv[3]) << c.output() << '\n';
            write_stats(argv[4], c.stats, 1);
            return 0;
        }
        if (cmd == "gen" && argc >= 4) {
            if (!gen_) { std::fprintf(stderr, "this problem has no generator\n"); return 6; }
            Gen g(std::stoull(argv[3]));
            for (auto &l : gen_(g, std::stoi(argv[2]))) std::printf("%s\n", l.c_str());
            return 0;
        }
        if (cmd == "bench" && argc >= 5) {
            if (!gen_) { std::fprintf(stderr, "this problem has no generator\n"); return 6; }
            Gen g(std::stoull(argv[3]));
            auto lines = gen_(g, std::stoi(argv[2]));
            Stats best;
            best.ns = 1e300;
            long long reps = 0;
            auto start = std::chrono::steady_clock::now();
            while (true) {
                Case c(lines);
                c.mode = Mode::Bench;
                c.readonly = readonly_;
                c.measure_stack = (reps == 0);
                fn_(c);
                ++reps;
                best.ns = std::min(best.ns, c.stats.ns);
                best.heap = std::max(best.heap, c.stats.heap);
                best.stack = std::max(best.stack, c.stats.stack);
                best.ops = std::max(best.ops, c.stats.ops);
#ifdef LC_COUNT
                break;
#endif
                double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
                if (readonly_ && reps >= 3) break;
                if (reps >= 25 || (reps >= 3 && wall > 0.25) || (reps >= 1 && wall > 0.8)) break;
            }
            write_stats(argv[4], best, reps);
            return 0;
        }
        if (cmd == "info") {
            std::printf("gen=%d readonly=%d\n", gen_ ? 1 : 0, readonly_ ? 1 : 0);
            return 0;
        }
    } catch (InputError &e) {
        std::fprintf(stderr, "[judge] invalid test input: %s\n", e.what());
        return 3;
    } catch (std::exception &e) {
        std::fprintf(stderr, "[judge] uncaught exception: %s\n", e.what());
        return 4;
    }
    std::fprintf(stderr,
                 "usage: %s run <in> <out> <stats> | gen <n> <seed> | bench <n> <seed> <stats> | info\n",
                 argv[0]);
    return 2;
}

}  // namespace lc

// The problem driver implements this function.
#define LC_DRIVER void lc_main(lc::Harness &h)
void lc_main(lc::Harness &h);

int main(int argc, char **argv) {
    lc::Harness h;
    lc_main(h);
    return h.main(argc, argv);
}
