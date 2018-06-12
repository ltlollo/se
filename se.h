// This is free and unencumbered software released into the public domain.
// For more information, see LICENSE.

#ifndef SE_H
#define SE_H

#include <SDL2/SDL.h>

#define EMPTY_DIFF     (3 + sizeof(size_t) * 2)
#define SIZE_SPLIT     (2 + sizeof(size_t) * 3)
#define SIZE_AGGR      (1 + sizeof(size_t) * 1)
#define DIFF_CHARS_OFF (2 + sizeof(size_t) * 2)
#define DIFF_ADD_EOD   (~0ull)


typedef int(*class_fn)(uint32_t);

struct gl_data {
    SDL_Window *window;
    SDL_GLContext glcontext;
    GLuint vao;
    GLuint tbo;
    GLuint vbo;
    GLuint prog;
    GLuint col;
    GLuint pos;
    GLuint uv;
    GLuint tex;
};

struct coord {
    GLfloat x;
    GLfloat y;
};

struct color {
    GLfloat r;
    GLfloat b;
    GLfloat g;
};

struct quad_coord {
    struct coord vertex_pos[6];
};

struct quad_color {
    struct color vertex_color[6];
};

struct window {
    unsigned width;
    unsigned height;
    unsigned scrollback_size;
    unsigned scrollback_pos;
    struct quad_coord *window_mesh;
    struct quad_coord *glyph_mesh;
    struct quad_color *font_color;
    char data[];
};

struct selection {
    size_t line;
    size_t glyph_beg;
    size_t glyph_end;
};

struct selectarr {
    size_t alloc;
    size_t size;
    struct selection *focus;
    struct selection data[];
};

enum UTF8_STATUS {
    UTF8_CLEAN = 0,
    UTF8_DIRTY = 1,
};

enum MV_VERT_DIRECTION {
    MV_VERT_UP = 0,
    MV_VERT_DOWN = 1,
};

enum MV_HORZ_DIRECTION {
    MV_HORZ_LEFT = 2,
    MV_HORZ_RIGHT = 3,
};

enum MV_DIRECTION {
    MV_UP = MV_VERT_UP,
    MV_DOWN = MV_VERT_DOWN,
    MV_LEFT = MV_HORZ_LEFT,
    MV_RIGHT = MV_HORZ_RIGHT,
};

struct extern_line {
    uint8_t utf8_status;
    uint8_t data[];
};

struct line {
    size_t size;
    size_t alloc;
    union {
        struct extern_line *extern_line;
        uint8_t *intern_line;
        void *ptr;
    };
};

struct line_non_utf8 {
    size_t size;
    uint8_t *data;
};

struct document {
    struct mmap_file file;
    size_t alloc;
    size_t loaded_size;
    size_t line_off;
    size_t glyph_off;
    struct line lines[];
};

struct diffstack {
    size_t alloc;
    size_t curr_checkpoint_beg;
    size_t curr_checkpoint_end;
    size_t last_checkpoint_beg;
    size_t last_checkpoint_end;
    uint8_t data[];
};

struct diffaggr_info {
    size_t old_checkpoint_beg;
    size_t old_checkpoint_end;
    size_t aggregate_beg;
    size_t size;
};

enum DIFF {
    DIFF_CHARS_ADD  = 'a',
    DIFF_CHARS_DEL  = 'd',
    DIFF_LINE_SPLIT = 's',
    DIFF_LINE_MERGE = 'm',
    DIFF_AGGREGATE  = 'A',
};

enum DIFF_DELIM {
    DIFF_CHAR_SEQ  = '\n',
    DIFF_AGGR_SEQ  = '\0',
    DIFF_SPLIT_SEP = '\1',
};

struct editor {
    struct window *win;
    struct document *doc;
    struct diffstack *diff;
    struct selectarr *selv;
};

#endif // SE_H
