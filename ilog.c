#include "util.h"
#include "ilog.h"
#include <stdlib.h>
#include <stdatomic.h>
#include <sys/random.h>

#define ILOG_BASE "se.ilog.dump"

struct ilog *ilogptr;

__attribute__((constructor)) void
ilog_init(void) {
    size_t alloc = 0x1000;

    ilogptr = malloc(sizeof(struct ilog) + sizeof(struct event) * alloc);
    ilogptr->size = 0;
    ilogptr->alloc = alloc;
    xensure(ilogptr);
}

void
ilog_dump_file(const char *fname) {
    FILE *f = fopen(fname, "w+");

    fwrite(ilogptr->data, sizeof(struct event), ilogptr->size, f);
    fflush(f);
    fclose(f);
}

void
ilog_dump(void) {
    static atomic_int lock;
    int expect = 0;

    if (atomic_compare_exchange_strong(&lock, &expect, 1) == 0) {
        return;
    }
    size_t uuid;
    char fname[sizeof(ILOG_BASE) + 16 + 1];
    getrandom(&uuid, sizeof(uuid), 0);
    sprintf(fname, "%s%016lx", ILOG_BASE, uuid);

    ilog_dump_file(fname);
}

void
ilog_push(SDL_Event *ev) {
    size_t alloc = ilogptr->alloc * 2;
    struct ilog *bk;
    struct event e = { .mod = ev->key.keysym.mod
        , .key = ev->key.keysym.sym
        , .type = ev->type
    };
    if (ilogptr->size == ilogptr->alloc) {
        bk = reallocflexarr((void **)&ilogptr, sizeof(struct ilog), alloc
            , sizeof(*ev)
        );
        if (bk == NULL) {
            ilog_dump();
            xensure(bk);
        }
        ilogptr = bk;
        ilogptr->alloc = alloc;
    }
    ilogptr->data[ilogptr->size++] = e;
}

#ifndef NDEBUG
__attribute__((destructor)) void
ilog_dump_fini(void) {
    ilog_dump_file(ILOG_BASE);
}
#endif
