#include "judge/complexity.hpp"

#include <algorithm>
#include <cmath>
#include <map>

namespace leet::judge {

namespace {

double growth(const std::string &c, double n) {
    double lg = std::log2(std::max(n, 2.0));
    if (c == "1") return 1;
    if (c == "logn") return lg;
    if (c == "log2n") return lg * lg;
    if (c == "sqrtn") return std::sqrt(n);
    if (c == "n") return n;
    if (c == "nlogn") return n * lg;
    if (c == "n^2") return n * n;
    if (c == "n^2logn") return n * n * lg;
    if (c == "n^3") return n * n * n;
    if (c == "2^n") return std::pow(2.0, n);
    if (c == "3^n") return std::pow(3.0, n);
    if (c == "4^n") return std::pow(4.0, n);
    if (c == "n!") return std::tgamma(n + 1);
    return n;
}

struct Model {
    double a = 0, b = 0, err = 1e18;
    bool valid = false;
};

// Fit y ≈ a + b·f(n) minimising relative error (weights 1/y), a >= 0.
Model fit_relative(const std::vector<std::pair<double, double>> &xy, const std::string &cls) {
    Model m;
    double S11 = 0, S12 = 0, S22 = 0, R1 = 0, R2 = 0;
    for (auto &[n, y] : xy) {
        double u = 1.0 / y, f = growth(cls, n);
        S11 += u * u; S12 += f * u * u; S22 += f * f * u * u;
        R1 += u; R2 += f * u;
    }
    if (cls == "1") {
        m.a = R1 / S11; m.b = 0;
    } else {
        double det = S11 * S22 - S12 * S12;
        if (std::fabs(det) > 1e-300) {
            m.a = (R1 * S22 - R2 * S12) / det;
            m.b = (S11 * R2 - S12 * R1) / det;
        }
        if (!(m.a >= 0) || !(m.b > 0)) { m.a = 0; m.b = R2 / S22; }
        if (!(m.b > 0)) return m;
    }
    double e = 0;
    for (auto &[n, y] : xy) {
        double r = (m.a + m.b * growth(cls, n) - y) / y;
        e += r * r;
    }
    m.err = std::sqrt(e / xy.size());
    m.valid = std::isfinite(m.err);
    return m;
}

// Fit y ≈ a + b·f(n) with plain least squares (memory is deterministic).
Model fit_absolute(const std::vector<std::pair<double, double>> &xy, const std::string &cls, double scale) {
    Model m;
    double k = xy.size(), sf = 0, sy = 0, sff = 0, sfy = 0;
    for (auto &[n, y] : xy) {
        double f = growth(cls, n);
        sf += f; sy += y; sff += f * f; sfy += f * y;
    }
    double det = k * sff - sf * sf;
    if (std::fabs(det) < 1e-300) return m;
    m.b = (k * sfy - sf * sy) / det;
    m.a = (sy - m.b * sf) / k;
    if (!(m.b > 0)) return m;
    double e = 0;
    for (auto &[n, y] : xy) {
        double r = (m.a + m.b * growth(cls, n) - y) / scale;
        e += r * r;
    }
    m.err = std::sqrt(e / k);
    m.valid = std::isfinite(m.err);
    return m;
}

std::vector<std::string> candidates(bool small, double max_n) {
    if (small) return {"1", "n", "n^2", "n^3", "2^n", "3^n", "4^n", "n!"};
    std::vector<std::string> c = {"1", "logn", "log2n", "n", "nlogn", "n^2", "n^3"};
    if (max_n <= 64) c.push_back("2^n");
    return c;
}

}  // namespace

int class_rank(const std::string &c) {
    static const std::map<std::string, int> r = {{"1", 0},    {"logn", 10},  {"log2n", 12},  {"sqrtn", 15},   {"n", 20},
                                                 {"nlogn", 30}, {"n^2", 40}, {"n^2logn", 45}, {"n^3", 50},
                                                 {"2^n", 60}, {"3^n", 62}, {"4^n", 64}, {"n!", 70}};
    auto it = r.find(c);
    return it == r.end() ? -1 : it->second;
}

Fit fit_time(const std::vector<BenchPoint> &pts, bool small) {
    Fit out;
    std::vector<std::pair<double, double>> xy;
    for (auto &p : pts)
        if (p.ns > 0) xy.push_back({(double)p.n, std::max(p.ns, 1.0)});
    if (xy.size() < 3) return out;
    double lo = 1e300, hi = 0;
    for (auto &[n, y] : xy) { lo = std::min(lo, y); hi = std::max(hi, y); }

    std::vector<std::pair<std::string, Model>> models;
    for (auto &c : candidates(small, xy.back().first)) {
        Model m = fit_relative(xy, c);
        if (m.valid) models.push_back({c, m});
    }
    if (models.empty()) return out;
    double best = 1e18;
    for (auto &[c, m] : models) best = std::min(best, m.err);
    // Prefer the simplest class that is nearly as good as the best fit.
    for (auto &[c, m] : models) {
        if (m.err <= best * 1.15 + 0.03) {
            out.cls = c;
            out.error = m.err;
            break;
        }
    }
    // Barely-changing timings are constant time no matter what fits best.
    if (!small && hi / lo < 1.6 && class_rank(out.cls) > class_rank("logn")) out.cls = "1";
    return out;
}

Fit fit_work(const std::vector<BenchPoint> &pts, bool small) {
    Fit out;
    std::vector<std::pair<double, double>> xy;
    for (auto &p : pts) {
        if (p.ops <= 0) return out;  // counting unavailable
        xy.push_back({(double)p.n, (double)p.ops});
    }
    if (xy.size() < 3) return out;
    std::vector<std::pair<std::string, Model>> models;
    for (auto &c : candidates(small, xy.back().first)) {
        Model m = fit_relative(xy, c);
        if (m.valid) models.push_back({c, m});
    }
    if (models.empty()) return out;
    double best = 1e18;
    for (auto &[c, m] : models) best = std::min(best, m.err);
    // Counts are deterministic, so only a small tolerance is needed.
    for (auto &[c, m] : models) {
        if (m.err <= best * 1.1 + 0.01) {
            out.cls = c;
            out.error = m.err;
            break;
        }
    }
    return out;
}

Fit choose_time_class(const Fit &work, const Fit &wall, std::string &basis) {
    static const std::vector<std::string> order = {"1", "logn", "log2n", "n", "nlogn", "n^2", "n^3", "2^n", "3^n", "4^n", "n!"};
    auto idx = [&](const std::string &c) {
        auto it = std::find(order.begin(), order.end(), c);
        return it == order.end() ? -1 : (int)(it - order.begin());
    };
    if (!work.ok()) {
        basis = "timing";
        return wall;
    }
    if (wall.ok() && idx(wall.cls) >= idx(work.cls) + 2) {
        basis = "timing (extra work happens inside library calls)";
        return wall;
    }
    basis = "operation count";
    return work;
}

Fit fit_space(const std::vector<BenchPoint> &pts, bool small) {
    Fit out;
    std::vector<std::pair<double, double>> xy;
    for (auto &p : pts) xy.push_back({(double)p.n, (double)p.memory()});
    if (xy.size() < 3) return out;
    double lo = 1e300, hi = -1e300;
    for (auto &[n, y] : xy) { lo = std::min(lo, y); hi = std::max(hi, y); }
    double range = hi - lo;
    // Constant: grows by less than a few hundred bytes over a huge range of n.
    double growth_n = xy.back().first / std::max(1.0, xy.front().first);
    if (range <= 320 || (growth_n >= 64 && range <= 0.02 * std::max(hi, 1.0))) {
        out.cls = "1";
        return out;
    }
    std::vector<std::pair<std::string, Model>> models;
    for (auto &c : candidates(small, xy.back().first)) {
        if (c == "1") continue;
        Model m = fit_absolute(xy, c, range);
        if (m.valid) models.push_back({c, m});
    }
    if (models.empty()) { out.cls = "1"; return out; }
    double best = 1e18;
    for (auto &[c, m] : models) best = std::min(best, m.err);
    for (auto &[c, m] : models) {
        if (m.err <= best + 0.02) {
            out.cls = c;
            out.error = m.err;
            break;
        }
    }
    return out;
}

Match compare_class(const std::string &expected, const std::string &estimated, bool precise) {
    int e = class_rank(expected), g = class_rank(estimated);
    if (e < 0 || g < 0) return Match::Unknown;
    if (g <= e) return Match::Optimal;
    if (precise) return Match::Worse;
    auto pair_is = [&](const char *a, const char *b) { return expected == a && estimated == b; };
    if (pair_is("1", "logn") || pair_is("n", "nlogn") || pair_is("n^2", "n^2logn") || pair_is("logn", "sqrtn") || pair_is("logn", "log2n"))
        return Match::Close;
    return Match::Worse;
}

}  // namespace leet::judge
