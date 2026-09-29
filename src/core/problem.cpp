#include "core/problem.hpp"

#include <algorithm>
#include <cstdio>
#include <map>

#include "util/fs.hpp"
#include "util/strings.hpp"

namespace leet {

std::string Problem::display_id() const { return is_c() ? "C" + std::to_string(number) : std::to_string(number); }

std::string Problem::file_stem() const {
    char buf[32];
    if (is_c()) std::snprintf(buf, sizeof buf, "c%02d", number);
    else std::snprintf(buf, sizeof buf, "%04d", number);
    return std::string(buf) + "-" + slug;
}

std::string Problem::url() const {
    if (is_c()) return "";
    return "https://leetcode.com/problems/" + slug + "/";
}

size_t Problem::example_count() const {
    return (size_t)std::count_if(tests.begin(), tests.end(), [](const TestCase &t) { return t.example; });
}

std::string complexity_label(const std::string &c) {
    static const std::map<std::string, std::string> names = {
        {"1", "O(1)"},       {"logn", "O(log n)"}, {"log2n", "O(log² n)"}, {"sqrtn", "O(√n)"},  {"n", "O(n)"},
        {"nlogn", "O(n log n)"}, {"n^2", "O(n²)"}, {"n^2logn", "O(n² log n)"}, {"n^3", "O(n³)"},
        {"2^n", "O(2ⁿ)"},    {"n!", "O(n!)"},       {"-", "—"}};
    auto it = names.find(c);
    return it == names.end() ? c : it->second;
}

namespace {

void parse_bench(const std::string &v, BenchSpec &b, std::vector<std::string> &errors, const std::string &where) {
    std::string t = str::trim(v);
    if (t.empty() || t == "off" || t == "none") { b.enabled = false; return; }
    b.enabled = true;
    b.space_class = "-";
    for (auto &tok : str::split(t, ' ', false)) {
        auto eq = tok.find('=');
        if (eq == std::string::npos) { errors.push_back(where + ": bad bench token '" + tok + "'"); continue; }
        std::string k = tok.substr(0, eq), val = tok.substr(eq + 1);
        try {
            if (k == "n") {
                auto dots = val.find("..");
                b.min_n = std::stoll(val.substr(0, dots));
                b.max_n = std::stoll(val.substr(dots + 2));
            } else if (k == "step") {
                b.additive = val[0] == '+';
                b.step = std::stoll(val.substr(val[0] == '+' || val[0] == 'x' ? 1 : 0));
            } else if (k == "time") {
                b.time_class = val;
            } else if (k == "space") {
                b.space_class = val;
            } else if (k == "unit") {
                b.unit = str::replace_all(val, "_", " ");
            } else {
                errors.push_back(where + ": unknown bench key '" + k + "'");
            }
        } catch (...) {
            errors.push_back(where + ": bad bench value '" + tok + "'");
        }
    }
    if (b.time_class.empty()) errors.push_back(where + ": bench needs time=<class>");
}

// Parse the blank-line separated test cases of an examples/tests section.
void parse_tests(const std::vector<std::pair<int, std::string>> &lines, bool example, Problem &p,
                 std::vector<std::string> &errors) {
    TestCase cur;
    bool have = false;
    auto finish = [&](int line) {
        if (!have) return;
        if (cur.expected_line == 0)
            errors.push_back(p.source_file + ":" + std::to_string(line) + ": test case without '> expected' line");
        cur.example = example;
        p.tests.push_back(cur);
        cur = TestCase{};
        have = false;
    };
    for (auto &[ln, raw] : lines) {
        std::string t = str::trim(raw);
        if (t.empty()) { finish(ln); continue; }
        have = true;
        if (t[0] == '>') {
            cur.expected = str::trim(t.substr(1));
            cur.expected_line = ln;
        } else if (t[0] == ':') {
            cur.explanation += (cur.explanation.empty() ? "" : " ") + str::trim(t.substr(1));
        } else {
            cur.args.push_back(t);
        }
    }
    finish(lines.empty() ? 0 : lines.back().first);
}

std::string dedent_block(const std::vector<std::pair<int, std::string>> &lines) {
    std::string s;
    for (auto &l : lines) s += l.second + "\n";
    // trim leading/trailing blank lines
    auto v = str::split(s, '\n');
    size_t a = 0, b = v.size();
    while (a < b && str::trim(v[a]).empty()) ++a;
    while (b > a && str::trim(v[b - 1]).empty()) --b;
    std::string out;
    for (size_t i = a; i < b; ++i) out += v[i] + "\n";
    return out;
}

}  // namespace

std::vector<Problem> parse_problem_file(const std::string &path, const std::string &default_lang,
                                        std::vector<std::string> &errors) {
    std::vector<Problem> out;
    auto content = fs::read_file(path);
    if (!content) { errors.push_back("cannot read " + path); return out; }
    auto lines = str::split(*content, '\n');

    std::string category;
    Problem *p = nullptr;
    std::string section;  // "" = header
    std::vector<std::pair<int, std::string>> buf;

    auto flush_section = [&]() {
        if (!p || section.empty()) { buf.clear(); return; }
        if (section == "description") p->description = dedent_block(buf);
        else if (section == "constraints") p->constraints = dedent_block(buf);
        else if (section == "template") p->tmpl = dedent_block(buf);
        else if (section == "driver") p->driver = dedent_block(buf);
        else if (section == "approach") p->approach = dedent_block(buf);
        else if (section == "solution") p->solution = dedent_block(buf);
        else if (section == "hints") {
            for (auto &l : str::split_lines(dedent_block(buf))) {
                std::string t = str::trim(l);
                if (str::starts_with(t, "- ")) p->hints.push_back(t.substr(2));
                else if (!t.empty() && !p->hints.empty()) p->hints.back() += " " + t;
            }
        } else if (section == "examples") parse_tests(buf, true, *p, errors);
        else if (section == "tests") parse_tests(buf, false, *p, errors);
        else errors.push_back(path + ": unknown section '" + section + "'");
        buf.clear();
    };

    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string &raw = lines[i];
        int ln = (int)i + 1;
        std::string where = path + ":" + std::to_string(ln);
        if (str::starts_with(raw, "@@@ ")) {
            flush_section();
            section.clear();
            auto parts = str::split(str::trim(raw.substr(4)), ' ', false);
            if (parts.size() < 3 || parts[0] != "problem") {
                errors.push_back(where + ": expected '@@@ problem <id> <slug>'");
                p = nullptr;
                continue;
            }
            out.emplace_back();
            p = &out.back();
            p->key = str::lower(parts[1]);
            p->lang = default_lang;
            std::string num = p->key[0] == 'c' ? p->key.substr(1) : p->key;
            try { p->number = std::stoi(num); } catch (...) { errors.push_back(where + ": bad id"); }
            p->slug = parts[2];
            p->category = category;
            p->source_file = path;
            continue;
        }
        if (str::starts_with(raw, "@@ ")) {
            std::string t = str::trim(raw.substr(3));
            if (str::starts_with(t, "category:")) category = str::trim(t.substr(9));
            continue;
        }
        if (str::starts_with(raw, "=== ")) {
            flush_section();
            section = str::trim(raw.substr(4));
            continue;
        }
        if (!p) continue;
        if (section.empty()) {
            std::string t = str::trim(raw);
            if (t.empty() || t[0] == '#') continue;
            auto colon = t.find(':');
            if (colon == std::string::npos) { errors.push_back(where + ": expected 'key: value'"); continue; }
            std::string k = str::trim(t.substr(0, colon)), v = str::trim(t.substr(colon + 1));
            auto list = [&](const std::string &s) {
                std::vector<std::string> r;
                for (auto &x : str::split(s, ',', false)) r.push_back(str::trim(x));
                return r;
            };
            if (k == "title") p->title = v;
            else if (k == "difficulty") p->difficulty = v;
            else if (k == "topics") p->topics = list(v);
            else if (k == "params") p->params = list(v);
            else if (k == "time") p->time = v;
            else if (k == "space") p->space = v;
            else if (k == "lang") p->lang = v;
            else if (k == "category") p->category = v;
            else if (k == "bench") parse_bench(v, p->bench, errors, where);
            else if (k == "stress") p->stress_n = std::stoi(v);
            else if (k == "stress_count") p->stress_count = std::stoi(v);
            else if (k == "timeout") p->timeout = std::stod(v);
            else errors.push_back(where + ": unknown key '" + k + "'");
        } else {
            buf.push_back({ln, raw});
        }
    }
    flush_section();

    for (auto &q : out) {
        std::string w = path + " [" + q.key + "]";
        if (q.title.empty()) errors.push_back(w + ": missing title");
        if (q.tmpl.empty()) errors.push_back(w + ": missing template");
        if (q.driver.empty()) errors.push_back(w + ": missing driver");
        if (q.solution.empty()) errors.push_back(w + ": missing solution");
        if (q.tests.empty()) errors.push_back(w + ": no test cases");
        std::stable_partition(q.tests.begin(), q.tests.end(), [](const TestCase &t) { return t.example; });
    }
    return out;
}

std::vector<Problem> load_problems(const std::string &dir, std::vector<std::string> &errors) {
    std::vector<Problem> all;
    for (const char *lang : {"cpp", "c"}) {
        std::string sub = fs::join(dir, lang);
        for (auto &f : fs::list_dir(sub)) {
            if (!str::ends_with(f, ".lc")) continue;
            auto ps = parse_problem_file(fs::join(sub, f), lang, errors);
            for (auto &p : ps) all.push_back(std::move(p));
        }
    }
    for (size_t i = 0; i < all.size(); ++i) all[i].order = i;
    return all;
}

}  // namespace leet
