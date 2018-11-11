// This is free and unencumbered software released into the public domain.
// For more information, see LICENSE.

#ifndef KCOL_H
#define KCOL_H

#include "lex.h"

static struct colored_keyword color_table[] = {
    KEYWORD(2, "if"),
    KEYWORD(2, "else"),
    KEYWORD(2, "do"),
    KEYWORD(2, "for"),
    KEYWORD(2, "while"),
    KEYWORD(2, "switch"),
    KEYWORD(2, "case"),
    KEYWORD(2, "break"),
    KEYWORD(2, "continue"),
    KEYWORD(2, "goto"),
    KEYWORD(2, "return"),
    KEYWORD(2, "sizeof"),
    KEYWORD(2, "return"),
    KEYWORD(3, "struct"),
    KEYWORD(3, "enum"),
    KEYWORD(3, "union"),
    KEYWORD(3, "static"),
    KEYWORD(3, "auto"),
    KEYWORD(3, "register"),
    KEYWORD(3, "extern"),
    KEYWORD(3, "const"),
    KEYWORD(3, "volatile"),
    KEYWORD(3, "char"),
    KEYWORD(3, "unsigned"),
    KEYWORD(3, "float"),
    KEYWORD(3, "double"),
    KEYWORD(3, "signed"),
    KEYWORD(3, "long"),
    KEYWORD(3, "int"),
    KEYWORD(3, "void"),
    KEYWORD(3, "short"),
    KEYWORD(3, "uint8_t"),
    KEYWORD(3, "uint16_t"),
    KEYWORD(3, "uint32_t"),
    KEYWORD(3, "uint64_t"),
    KEYWORD(3, "int8_t"),
    KEYWORD(3, "int16_t"),
    KEYWORD(3, "int32_t"),
    KEYWORD(3, "int64_t"),
    KEYWORD(3, "size_t"),
    KEYWORD(3, "ssize_t"),
    KEYWORD(3, "GLuint"),
    KEYWORD(3, "GLfloat"),
    KEYWORD(6, "NULL"),

    KEYWORD(0,  NULL),
};

#endif // KCOL_H
