/* ==========================================================================
 *  leet judge runtime — operation counter
 *
 *  For complexity analysis the solution is compiled a second time with
 *  -fsanitize-coverage=trace-pc: the compiler then calls this hook at the
 *  start of every executed basic block.  Counting those calls while the
 *  solution runs gives a deterministic measure of the work it performs,
 *  unaffected by CPU caches, frequency scaling or other processes.
 *
 *  This file is compiled WITHOUT the coverage flag (otherwise the hook would
 *  instrument itself).
 * ========================================================================== */
#include <stdint.h>

uint64_t lc_cov_count = 0;
int lc_cov_on = 0;

void __sanitizer_cov_trace_pc(void) {
    if (lc_cov_on) ++lc_cov_count;
}
