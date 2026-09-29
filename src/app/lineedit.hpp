// Minimal line editor (history, cursor movement, tab completion) for the shell.
#pragma once

#include <functional>
#include <string>
#include <vector>

namespace leet {

class LineEditor {
public:
    using Completer = std::function<std::vector<std::string>(const std::string &buffer)>;

    explicit LineEditor(std::string history_file);
    ~LineEditor();
    // Returns false on EOF (Ctrl-D on an empty line).
    bool read(const std::string &prompt, std::string &out);
    void set_completer(Completer c) { completer_ = std::move(c); }

private:
    std::string history_file_;
    std::vector<std::string> history_;
    Completer completer_;
    bool raw_ = false;

    bool enable_raw();
    void disable_raw();
    bool read_fallback(const std::string &prompt, std::string &out);
    void save_history() const;
};

}  // namespace leet
