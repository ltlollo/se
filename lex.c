// This is free and unencumbered software released into the public domain.
// For more information, see LICENSE.

#include <string.h>

#include "lex.h"
#include "kcol.def.h"

int
is_alpha(uint32_t c) {
    if (c >= 'a' && c <= 'z') {
        return 1;
    }
    if (c >= 'A' && c <= 'Z') {
        return 1;
    }
    return 0;
}

int is_char(uint32_t c) {
    return c < 0x10000;
}

int
is_digit(uint32_t c) {
    return c >= '0' && c <= '9';
}

int
is_alnum(uint32_t c) {
    if (is_alpha(c)) {
        return 1;
    }
    if (is_digit(c)) {
        return 1;
    }
    return 0;
}

int
is_tok(uint32_t c) {
    if (is_alnum(c)) {
        return 1;
    }
    if (c == '_') {
        return 1;
    }
    return 0;
}

int
is_space(uint32_t c) {
    return c == '\t' || c == ' ';
}


int
is_hex(uint32_t c) {
    return is_digit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

int
keyword_match(uint8_t *curr, uint8_t *end, char *str, intptr_t size) {
    if (end - curr < size) {
        return 0;
    }
    if (end - curr > size && is_tok(curr[size])) {
        return 0;
    }
    return memcmp(curr, str, size) == 0;
}

uint8_t *
number_match(uint8_t *curr, uint8_t *end) {
    uint8_t *beg = curr;
    if (end - curr == 0) {
        return beg;
    }
    if (!is_digit(*curr) && *curr != '.') {
        return beg;
    }
    if (end - curr > 2
        && curr[0] == '0'
        && curr[1] == 'x'
        && is_hex(curr[2])
    ) {
        for (curr = curr + 2; curr != end; curr++) {
            if (is_hex(*curr)) {
                continue;
            }
            if (is_tok(*curr)) {
                return beg;
            }
            return curr;
        }
    }
    while (curr != end && is_digit(*curr)) {
        curr++;
    }
    if (curr != end && *curr == '.') {
        curr++;
        while (curr != end && is_digit(*curr)) {
            curr++;
        }
        if (beg != end && *curr == 'f') beg++;
        if (is_tok(*curr)) {
            return beg;
        }
        return curr;
    }
    if (beg != end && *curr == 'u') beg++;
    if (beg != end && *curr == 'l') beg++;
    if (beg != end && *curr == 'l') beg++;
    if (is_tok(*curr)) {
        return beg;
    }
    return curr;
}

struct colored_span
color_span(uint8_t *beg, uint8_t *curr, uint8_t *end) {
    struct colored_span res = {0, 1};
    struct colored_keyword *kw = color_table;
    uint8_t *keyword_end;

    if (*curr <= 0x20 || *curr > 0x7e) {
        return res;
    }
    if (end - curr >= 2 && memcmp(curr, "//", 2) == 0) {
        res.size = end - curr;
        res.color_pos = 4;
        return res;
    }
    if (beg - curr == 0 && *curr == '#') {
        res.color_pos = 1;
        res.size = end - curr;
        return res;
    }
    if (end - curr > 2 && *curr == '\'') {
        for (keyword_end = curr + 1; keyword_end < end; keyword_end++) {
            if (*keyword_end == '\'') {
                break;
            }
        }
        res.color_pos = 6;
        res.size = keyword_end - curr + 1;
        return res;
    }
    if (curr - 1 - beg > 0 && is_tok(curr[-1])) {
        return res;
    }
    while (kw->data) {
        if (keyword_match(curr, end, kw->data, kw->span.size)) {
            return kw->span;
        }
        kw++;
    }
    if (*curr == '"') {
        end = memchr(curr + 1, '"', end - curr - 1);
        if (end) {
            res.size = end - curr + 1;
            res.color_pos = 5;
        }
        return res;
    }
    if (*curr == '<') {
        for (keyword_end = curr + 1; keyword_end < end; keyword_end++) {
            if (is_tok(*keyword_end)
                || *keyword_end == '/'
                || *keyword_end == '.'
                ) {
                continue;
            }
            if (*keyword_end == '>') {
                res.size = keyword_end + 1 - curr;
                res.color_pos = 5;
                return res;
            }
            break;
        }
    }
    keyword_end = number_match(curr, end);
    if (keyword_end != curr) {
        res.size = keyword_end - curr;
        res.color_pos = 6;
        return res;
    }
    return res;
}

