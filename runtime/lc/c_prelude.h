/* ==========================================================================
 *  leet judge runtime — C prelude
 *  Force-included (gcc -include) before every C solution, so you can use the
 *  common headers without writing the #includes yourself.
 * ========================================================================== */
#ifndef LC_C_PRELUDE_H
#define LC_C_PRELUDE_H

#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Singly-linked list node used by the linked-list problems. */
struct ListNode {
    int val;
    struct ListNode *next;
};

#endif
