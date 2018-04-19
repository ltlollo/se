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

#define cxkeyword(color, str) {                                     \
    .data = str, { .color_pos = color, .size = sizeof(str) - 1 },   \
}

uint8_t *number_match(uint8_t *, uint8_t *);
int is_tok(uint32_t);
int is_digit(uint32_t);
int is_hex(uint32_t);
int keyword_match(uint8_t *, uint8_t *, char *, intptr_t);
struct colored_span color_span(uint8_t *, uint8_t *, uint8_t *);

#endif // LEX_H
