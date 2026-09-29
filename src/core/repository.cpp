#include "core/repository.hpp"

#include <algorithm>

#include "util/strings.hpp"

namespace leet {

bool Repository::load(const std::string &dir) {
    errors_.clear();
    problems_ = load_problems(dir, errors_);
    return !problems_.empty();
}

const Problem *Repository::find(const std::string &q0) const {
    std::string q = str::lower(str::trim(q0));
    if (q.empty()) return nullptr;
    if (q[0] == '#') q = q.substr(1);
    for (auto &p : problems_)
        if (p.key == q || p.slug == q) return &p;
    // "0088" style numbers
    if (std::all_of(q.begin(), q.end(), ::isdigit)) {
        int n = std::stoi(q);
        for (auto &p : problems_)
            if (!p.is_c() && p.number == n) return &p;
        return nullptr;
    }
    // "c07"
    if (q.size() > 1 && q[0] == 'c' && std::all_of(q.begin() + 1, q.end(), ::isdigit)) {
        int n = std::stoi(q.substr(1));
        for (auto &p : problems_)
            if (p.is_c() && p.number == n) return &p;
        return nullptr;
    }
    const Problem *hit = nullptr;
    int count = 0;
    for (auto &p : problems_) {
        if (str::lower(p.title) == q) return &p;
        if (str::contains_ci(p.title, q) || str::contains_ci(p.slug, q)) { hit = &p; ++count; }
    }
    return count == 1 ? hit : nullptr;
}

std::vector<const Problem *> Repository::suggest(const std::string &q0, size_t max) const {
    std::string q = str::lower(str::trim(q0));
    std::vector<std::pair<int, const Problem *>> scored;
    auto words = str::split(q, ' ', false);
    for (auto &p : problems_) {
        int s = 0;
        std::string hay = str::lower(p.title + " " + p.slug + " " + str::join(p.topics, " "));
        for (auto &w : words)
            if (hay.find(w) != std::string::npos) s += 2;
        if (str::contains_ci(p.title, q)) s += 5;
        if (s > 0) scored.push_back({s, &p});
    }
    std::stable_sort(scored.begin(), scored.end(), [](auto &a, auto &b) { return a.first > b.first; });
    std::vector<const Problem *> out;
    for (auto &s : scored) {
        if (out.size() >= max) break;
        out.push_back(s.second);
    }
    return out;
}

std::vector<const Problem *> Repository::filter(const Filter &f) const {
    std::vector<const Problem *> out;
    for (auto &p : problems_) {
        if (!f.lang.empty() && p.lang != str::lower(f.lang)) continue;
        if (!f.difficulty.empty() && str::lower(p.difficulty) != str::lower(f.difficulty)) continue;
        if (!f.category.empty() && !str::contains_ci(p.category, f.category)) continue;
        if (!f.search.empty()) {
            std::string hay = p.title + " " + p.slug + " " + str::join(p.topics, " ") + " " + p.category;
            bool all = true;
            for (auto &w : str::split(f.search, ' ', false))
                if (!str::contains_ci(hay, w)) all = false;
            if (!all) continue;
        }
        out.push_back(&p);
    }
    return out;
}

std::vector<std::string> Repository::categories() const {
    std::vector<std::string> out;
    for (auto &p : problems_)
        if (std::find(out.begin(), out.end(), p.category) == out.end()) out.push_back(p.category);
    return out;
}

const Problem *Repository::next(const Problem &p, int dir) const {
    long idx = (long)p.order + dir;
    if (idx < 0 || idx >= (long)problems_.size()) return nullptr;
    return &problems_[idx];
}

}  // namespace leet
