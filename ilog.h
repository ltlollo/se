// This is free and unencumbered software released into the public domain.
// For more information, see LICENSE.

#ifndef ILOG_H
#define ILOG_H
#include <SDL2/SDL.h>

struct event {
    int key;
    int mod;
    unsigned type;
};

struct ilog {
    size_t size;
    size_t alloc;
    struct event data[];
};

void ilog_dump(void);
void ilog_dump_sig(int);
void ilog_push(SDL_Event *);

#endif // ILOG_H
