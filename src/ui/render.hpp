// Higher level rendering: markdown-lite, code highlighting, boxes, tables.
#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace leet::ui {

// Render a small Markdown subset (paragraphs, `code`, **bold**, *italic*,
// "- " bullets, ``` fenced code) wrapped to `width` with `indent` spaces.
std::string markdown(const std::string &text, int width, int indent = 2);

// Syntax highlight C/C++ source; returns one string per line.
std::vector<std::string> highlight(const std::string &code, const std::string &lang = "cpp");

// Code in a frame with line numbers.
std::string code_block(const std::string &code, const std::string &lang, const std::string &title = "");

// A rounded box around pre-rendered lines.
std::string box(const std::vector<std::string> &lines, const std::string &title = "",
                const std::string &sgr = "38;5;240", int width = 0);

// ── Section title ─────────────────
std::string rule(const std::string &title, const std::string &sgr = "38;5;45");

// [██████░░░░] style bar.
std::string progress_bar(double frac, int width, const std::string &sgr = "38;5;45");

// Simple column table.
class Table {
public:
    enum Align { Left, Right, Center };
    void column(const std::string &header, Align a = Left, int max_width = 0);
    void row(std::vector<std::string> cells);
    std::string render(int indent = 2) const;
    size_t size() const { return rows_.size(); }

private:
    struct Col { std::string header; Align align; int max_width; };
    std::vector<Col> cols_;
    std::vector<std::vector<std::string>> rows_;
};

// Animated spinner shown while a long action runs (no-op when not a tty).
class Spinner {
public:
    explicit Spinner(std::string label);
    ~Spinner();
    void set_label(const std::string &label);
    void stop(const std::string &final_line = "");

private:
    std::string label_;
    std::mutex mu_;
    std::atomic<bool> running_{false};
    std::thread th_;
};

}  // namespace leet::ui
