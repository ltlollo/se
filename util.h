// This is free and unencumbered software released into the public domain.
// For more information, see LICENSE.

#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>
#include <err.h>
#include <stdio.h>


#define __pack       __attribute__((packed))
#define __unused     __attribute__((unused))
#define __align(n)   __attribute__((aligned(n)))
#define __likey(x)   __builtin_expect(!!(x), 1)
#define __unlikey(x) __builtin_expect(!!(x), 0)

#ifdef NDEBUG
#define xensure_f(f, cond)\
    do {\
        if (__unlikey((cond) == 0)) {\
            f(1, "Invariant violated %s:%d\n  %s",__FILE__, __LINE__, #cond);\
        }\
    } while (0)
#else
#define xensure_f(f, cond)\
    do{\
        if (__unlikey((cond) == 0)) {\
            fprintf(stderr, "Invariant violated %s:%d\n  %s"\
                ,__FILE__, __LINE__, #cond\
            );\
            /* __asm__ volatile ("int $3\n\t"); */ \
        }\
    } while (0)
#endif


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
void rmemcpy(void *restrict, void *restrict, size_t);

#endif // UTIL_H
