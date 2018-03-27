// This is free and unencumbered software released into the public domain.
// For more information, see LICENSE.

#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <err.h>

#define __pack __attribute__((packed))

#define xensure_f(f, cond)\
    do {\
        if (__builtin_expect((cond) == 0, 0)) {\
            f(1, "Invariant violated %s:%d\n  %s",__FILE__, __LINE__, #cond);\
        }\
    } while (0)

#define xensure(cond)         xensure_f(errx, cond)
#define xensure_errno(cond)   xensure_f(err, cond)
#define min(a, b)             ((a) < (b) ? (a) : (b))
#define max(a, b)             ((a) > (b) ? (a) : (b))
#define ptr_mask(p, m)        ((void *)(((uintptr_t)(p)) & (m)))
#define ptr_align_bound(p, b) (ptr_mask(p, ~((b) - 1)))
#define ptr_align_bits(p, b)  (ptr_align_bound(p, 1 << (b)))

// this will change to a dump and crash like behavior
#define ensure(cond) xensure(cond)

#ifndef NDEBUG
#   define dbg_assert(cond) ensure(cond)
#   define RUNTIME_INSTR    (1)
#else
#   define dbg_assert(cond) ((void)(cond))
#   define RUNTIME_INSTR    (0)
#endif

void *reallocarr(void **, size_t, size_t);
void *reallocflexarr(void **, size_t, size_t, size_t );
void memzero(void *, size_t, size_t);

#endif // UTIL_H
