// This is free and unencumbered software released into the public domain.
// For more information, see LICENSE.

#ifndef LEX_H
#define LEX_H

#include <stddef.h>
#include <stdint.h>

struct colored_span {
    int color_pos;
    unsigned size;
};

struct colored_keyword {
    char *data;
    struct colored_span span;
};

#define KEYWORD(color, str) {\
    .data = str, { .color_pos = color, .size = sizeof(str) - 1 },\
}

#endif // LEX_H
