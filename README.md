# leet: LeetCode-style practice in your Linux terminal

`leet` is a terminal app for practicing coding interview problems in **C++** and **C**. It is fully offline.

- **All 150 problems of LeetCode's _Top Interview 150_** list, solved in C++.
- **20 classic C problems**: strings, bit tricks, pointers and memory, and algorithms. You solve them in plain C11.
- **A real judge.** Each submission runs the examples, hidden edge-case tests, 30 randomized tests checked against a reference solution, and one maximum-size test that catches *Time Limit Exceeded*.
- **Complexity analysis.** The judge measures how your time and memory grow with input size. It then tells you whether you hit the optimal O(·).
- **Commented reference solutions**, each with an explanation of the approach, plus progressive hints.
- **A friendly interface**: colors, boxes, syntax highlighting, progress dashboards, and an interactive shell with history and tab completion.

```
  ── 88. Merge Sorted Array  submission ──────────────────────────────────────
  ╭──────────────────────────────────────────────────────────────────────────╮
  │ ✔  Accepted   39 / 39 tests passed                                       │
  │ examples 3  ·  hidden 5  ·  randomized 30  ·  large 1                    │
  ╰──────────────────────────────────────────────────────────────────────────╯
  ── Complexity analysis ─────────────────────────────────────────────────────
       n  time / call                   reference  operations  memory
      64       224 ns  █                    52 ns         744    88 B
     ...
    1.0M      24.1 ms  ███████████████     2.9 ms       49.9M   1.4 KB

  Time   O(n log n)  goal O(m + n)      ⚠ grows faster than the optimal O(n)
  Space  O(log n)    goal O(1)          ⚠ grows faster than the optimal O(1)
  Speed  8.17x the reference time (room to optimize)  at n = 1.0M
```

## Requirements

- Linux
- `g++` (C++17) and `gcc`. `clang` also works, but complexity analysis then falls back to timing only.
- CMake ≥ 3.14 and `make`

## Build and install

```bash
make                 # builds ./build/leet
./build/leet         # start the interactive shell

sudo make install    # optional: installs /usr/local/bin/leet (+ problems in /usr/local/share/leet)
make install PREFIX=$HOME/.local   # or install for your user only
```

Your solutions go to `~/leet-workspace/` (override with `LEET_WORKSPACE=/some/dir`).
Everything you do is saved there. That includes your progress in `.leet/progress.tsv`,
every submission's code and verdict in `.leet/submissions/` (indexed by `.leet/submissions.tsv`),
and backups of files replaced by `reset`/`restore` in `.leet/backups/`.

## Usage

```bash
leet                   # interactive shell (tab completion, history)
leet list              # all problems grouped by topic, with your status
leet list -d easy      # filters: -d easy|medium|hard  -c <topic>  -l cpp|c  -s solved|todo
leet list c            # only the C problems
leet topics            # topics with progress bars

leet show 1            # read a problem (LeetCode number, C id like c3, slug, or title words)
leet edit 1            # create/open your solution file in $EDITOR
leet test 1            # run the examples
leet run 1             # run on your own input (compared against the reference)
leet submit 1          # full judge + complexity analysis
leet analyze 1         # only the complexity analysis

leet hint 1            # first hint;  leet hint 1 2  for the second
leet solution 1        # commented reference solution and explanation
leet stats             # progress dashboard
leet history           # your recent submissions (every submit is saved with its code)
leet history 1         # all submissions of problem 1: verdict, tests, measured complexity
leet history 1 3       # view the code of submission #3
leet restore 1 [3]     # put submission #3 (default: latest accepted) back into your file
leet done 1            # mark a problem as done by hand;  leet undone 1  reverts it
leet random -d medium  # pick an unsolved problem
leet next              # go to the next problem in list order
leet reset 1           # restore the starter template (your file is backed up)
leet help <command>
```

Useful options:

| option | effect |
| --- | --- |
| `--sanitize` | compile with AddressSanitizer and UBSan, to find the exact line of a crash |
| `--no-bench` | skip the complexity analysis on submit |
| `--no-color` | plain output (`NO_COLOR` also works) |

### Writing solutions

**C++ problems** work exactly like LeetCode. `<bits/stdc++.h>` and `using namespace std;` are already available, as are `ListNode` and `TreeNode`. Keep the class and method signature from the template.

**C problems** are compiled as C11 with `gcc -O2 -Wall`. `stdio.h`, `stdlib.h`, `string.h`, `stdbool.h`, `stdint.h`, `limits.h` and `ctype.h` are pre-included. Some C problems also check your memory management. The judge tracks every `malloc`/`free` and reports leaks.

**Custom input** (`leet run`) uses one argument per line in LeetCode syntax:

```
[2,7,11,15]
9
```

## How the judge works

Every problem is compiled into one executable: judge runtime + your solution + a problem-specific driver.

1. **Compile.** Build errors are shown with a hint when the function signature was changed. The heavy runtime header is precompiled once, so rebuilds are fast.
2. **Examples and hidden tests.** These are fixed inputs with expected outputs.
3. **Randomized tests.** A problem-specific generator creates 30 inputs of growing size. Each one runs through the bundled reference solution and yours, and the outputs must match. Problems with several valid answers (any order, any peak, any valid topological order, any minimal window) use checkers that validate or canonicalize the output.
4. **Large test.** One input at the benchmark's maximum size. It catches brute-force solutions (*Time Limit Exceeded*).
5. **Complexity analysis**, described in the next section.

Each run is a separate sandboxed process with a wall-clock limit, a CPU limit, an address-space limit, and a 1 GiB stack, so deep recursion is allowed. Crashes are explained. For example, SIGSEGV is reported as a null pointer, out-of-bounds access or stack overflow. Anything your code prints goes to a separate "Stdout" panel instead of corrupting the answer.

### How complexity is measured

The judge generates inputs of size n = 64, 128, … up to about 1M, or n = 1, 2, 3, … for exponential problems. Then it measures three things:

- **Operations.** Your solution is compiled a second time with `-fsanitize-coverage=trace-pc`, which counts every executed basic block. This count is deterministic and unaffected by CPU caches or machine load, so O(n) and O(n log n) can be told apart reliably.
- **Wall time.** This is what the time column shows. If time grows at least two classes faster than the operation count, the time estimate is used instead. That happens when the extra work hides inside library calls such as `memcpy`.
- **Memory.** Peak heap is measured by intercepting `malloc`/`free`, which also sees C++ `new`. Stack depth is measured by "stack painting", so recursion counts toward space.

Each measurement series is fitted against O(1), O(log n), O(log² n), O(n), O(n log n), O(n²), O(n³), O(2ⁿ), O(3ⁿ), O(4ⁿ) and O(n!). The result is compared with the optimal class and with the reference solution's speed.

## Architecture

```
src/
  main.cpp
  app/        CLI dispatch, commands, views, interactive shell + line editor, dev tools
  core/       Problem model + .lc parser, repository (search/filter), config, workspace, progress
  judge/      process sandbox, builder (PCH, caching, C/C++ linking), judge pipeline,
              complexity fitting
  ui/         terminal capabilities, colors, markdown rendering, syntax highlighting,
              boxes, tables, spinner
  util/       strings, filesystem
runtime/lc/
  harness.hpp   judge runtime linked into every solution: LeetCode-literal parser/printer,
                ListNode/TreeNode, generators, timing, heap + stack measurement,
                solve()/design()/custom() driver API
  c_prelude.h   force-included into C solutions
  cov.c         operation counter hook for complexity analysis
problems/
  cpp/NN-topic.lc   the 150 Top Interview problems, one file per topic
  c/01-c-classics.lc
```

### Problem file format

Each problem is a self-contained block in a `.lc` file:

```
@@@ problem 1 two-sum
title: Two Sum
difficulty: Easy
topics: Array, Hash Table
params: nums, target
time: O(n)
space: O(n)
bench: n=64..1048576 time=n space=n
=== description        (markdown)
=== constraints
=== hints              (one "- " bullet per hint)
=== template           (starter code)
=== driver             (how to call the solution, plus a random input generator)
=== approach           (markdown explanation)
=== solution           (commented reference solution)
=== examples           (shown in the statement)
=== tests              (hidden; "> ?" is filled in by `leet dev bake`)
```

A typical driver is two lines:

```cpp
LC_DRIVER {
    h.solve(&Solution::twoSum).sort_output();
    h.gen([](lc::Gen &g, int n) { /* build an input of size n */ return lc::lines(nums, target); });
}
```

### Maintainer tools

```bash
leet dev check            # validate every problem file
leet dev bake [ids]       # compute "> ?" expected outputs from the reference solutions
leet dev verify [ids]     # compile each template, run each reference through the full judge,
                          # and confirm the measured complexity matches the spec
make verify               # the same for everything
```
