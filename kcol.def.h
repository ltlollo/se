// This is free and unencumbered software released into the public domain.
// For more information, see LICENSE.

#ifndef KCOL_H
#define KCOL_H

#include "lex.h"

static struct colored_keyword color_table[] = {
    cxkeyword(2, "if"),
    cxkeyword(2, "else"),
    cxkeyword(2, "do"),
    cxkeyword(2, "for"),
    cxkeyword(2, "while"),
    cxkeyword(2, "switch"),
    cxkeyword(2, "case"),
    cxkeyword(2, "break"),
    cxkeyword(2, "continue"),
    cxkeyword(2, "goto"),
    cxkeyword(2, "return"),
    cxkeyword(2, "sizeof"),
    cxkeyword(2, "return"),
    cxkeyword(3, "struct"),
    cxkeyword(3, "enum"),
    cxkeyword(3, "union"),
    cxkeyword(3, "static"),
    cxkeyword(3, "auto"),
    cxkeyword(3, "register"),
    cxkeyword(3, "extern"),
    cxkeyword(3, "const"),
    cxkeyword(3, "volatile"),
    cxkeyword(3, "char"),
    cxkeyword(3, "unsigned"),
    cxkeyword(3, "float"),
    cxkeyword(3, "double"),
    cxkeyword(3, "signed"),
    cxkeyword(3, "long"),
    cxkeyword(3, "int"),
    cxkeyword(3, "void"),
    cxkeyword(3, "short"),
    cxkeyword(3, "uint8_t"),
    cxkeyword(3, "uint16_t"),
    cxkeyword(3, "uint32_t"),
    cxkeyword(3, "uint64_t"),
    cxkeyword(3, "int8_t"),
    cxkeyword(3, "int16_t"),
    cxkeyword(3, "int32_t"),
    cxkeyword(3, "int64_t"),
    cxkeyword(3, "size_t"),
    cxkeyword(3, "ssize_t"),
    cxkeyword(3, "GLuint"),
    cxkeyword(3, "GLfloat"),
    cxkeyword(6, "NULL"),

    cxkeyword(0,  NULL),
};

#endif // KCOL_H
