// This is free and unencumbered software released into the public domain.
// For more information, see LICENSE.

#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/freeglut.h>

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "util.h"
#include "umap.h"
#include "fio.h"
#include "lex.h"
#include "comp.h"

#define D_GLYPH     (1.0f / 256)
#define H_GLYPH_PX  (12)
#define V_GLYPH_PX  (16)
#define TAB_SIZE    (8)
#define EMPTY_DIFF  (3 + sizeof(size_t) * 2)
#define SIZE_SPLIT  (2 + sizeof(size_t) * 3)
#define SIZE_AGGR   (1 + sizeof(size_t) * 1)

extern char *__progname;

static struct gl_data {
    GLuint vao;
    GLuint tbo;
    GLuint vbo;
    GLuint prog;
    GLuint col;
    GLuint pos;
    GLuint uv;
    GLuint tex;
} gl_id;

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

static struct window {
    void *data;
    unsigned width;
    unsigned height;
    unsigned scrollback_size;
    unsigned scrollback_pos;
    struct quad_coord *window_mesh;
    struct quad_coord *glyph_mesh;
    struct quad_color *font_color;
} win;

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

struct line_metadata {
    size_t win_lines;
    struct line line;
};

static struct document {
    struct mmap_file file;
    size_t alloc;
    size_t loaded_size;
    size_t y_line_off;
    size_t y_window_line_off;
    size_t x_off;
    struct line_metadata lines[];
} *doc;

static struct diffstack {
    size_t alloc;
    size_t curr_checkpoint_beg;
    size_t curr_checkpoint_end;
    size_t last_checkpoint_beg;
    size_t last_checkpoint_end;
    uint8_t data[];
} *diff;


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

#include "se.h"

uint32_t
first_glyph(uint8_t *beg, uint8_t *end) {
    if (next_utf8_or_null(beg, end) != NULL) {
        return glyph_from_utf8(&beg);
    }
    return *beg;
}

typedef int(*class_fn)(uint32_t);

int
always(uint32_t glyph __unused) {
    return 1;
}

class_fn glyph_classes[] = {
    always
};

int
is_same_class(uint32_t glyph_fst, uint32_t glyph_snd) {
    class_fn *f_beg = glyph_classes;
    class_fn *s_beg = glyph_classes;
    class_fn *end = f_beg + sizeof(glyph_classes) / sizeof(*glyph_classes);

    while (1) {
        if (f_beg == end) {
            return 0;
        }
        if ((*f_beg++)(glyph_fst)) {
            break;
        }
    }
    while (1) {
        if (s_beg == end) {
            return 0;
        }
        if ((*s_beg++)(glyph_snd)) {
            break;
        }
    }
    return s_beg == f_beg;
}

struct diffstack *
diffstack_reserve(struct diffstack **ds, size_t size) {
    struct diffstack *res = *ds;
    size_t alloc;

    if (res->curr_checkpoint_end + size + EMPTY_DIFF >= res->alloc) {
        alloc = res->alloc * 2 + size + EMPTY_DIFF;
        ensure(reallocflexarr((void **) ds
            , sizeof(struct diffstack)
            , alloc
            , sizeof(uint8_t)
        ));
        res = *ds;
        res->alloc = alloc;
    }
    return res;
}

void
diffstack_insert_merge(struct diffstack **ds
    , struct line_metadata *lm
    , size_t size
    , size_t x
    , size_t y
    ) {
    struct diffstack *res = *ds;
    size_t old_x;
    size_t old_y;
    size_t old_size;

    if (size == 0) {
        return;
    }
    res = diffstack_reserve(ds, SIZE_SPLIT);
    ensure(res);

    dbg_assert(x == 0 && y != 0);

    if (res->curr_checkpoint_beg != res->curr_checkpoint_end
        && res->data[res->curr_checkpoint_beg] == DIFF_LINE_MERGE
    ) {
        memcpy(&old_x
            , res->data + res->curr_checkpoint_beg + 1
            , sizeof(size_t)
        );
        memcpy(&old_y
            , res->data + res->curr_checkpoint_beg + 1 + sizeof(size_t)
            , sizeof(size_t)
        );
        memcpy(&old_size
            , res->data + res->curr_checkpoint_beg + 1 + 2 * sizeof(size_t)
            , sizeof(size_t)
        );
        if (old_x != 0 || old_y != y) {
            goto MERGE_DIFFERENT_DIFF_CLASS;
        }
        x = lm[-1].line.size;
        y = y - 1;
        size += old_size;
        memcpy(res->data + res->curr_checkpoint_beg + 1
            , &x
            , sizeof(size_t)
        );
        memcpy(res->data + res->curr_checkpoint_beg + 1 + sizeof(size_t)
            , &y
            , sizeof(size_t)
        );
        memcpy(res->data + res->curr_checkpoint_beg + 1 + 2 * sizeof(size_t)
            , &size
            , sizeof(size_t)
        );
    } else {
MERGE_DIFFERENT_DIFF_CLASS:
        res->curr_checkpoint_beg = res->curr_checkpoint_end;
        res->curr_checkpoint_end = res->curr_checkpoint_beg + SIZE_SPLIT;
        res->data[res->curr_checkpoint_beg] = DIFF_LINE_MERGE;
        memcpy(res->data + res->curr_checkpoint_beg + 1, &x, sizeof(size_t));
        memcpy(res->data + res->curr_checkpoint_beg + 1 + sizeof(size_t)
            , &y
            , sizeof(size_t)
        );
        memcpy(res->data + res->curr_checkpoint_beg + 1 + 2 * sizeof(size_t)
            , &size
            , sizeof(size_t)
        );
        res->data[res->curr_checkpoint_end - 1] = DIFF_SPLIT_SEP;
    }
    res->last_checkpoint_beg = res->curr_checkpoint_beg;
    res->last_checkpoint_end = res->curr_checkpoint_end;
}

void
diffstack_insert_split(struct diffstack **ds
    , size_t size
    , size_t x
    , size_t y
    ) {
    struct diffstack *res = *ds;
    size_t old_x;
    size_t old_y;
    size_t old_size;

    if (size == 0) {
        return;
    }
    res = diffstack_reserve(ds, SIZE_SPLIT);
    ensure(res);

    if (res->curr_checkpoint_beg != res->curr_checkpoint_end
        && res->data[res->curr_checkpoint_beg] == DIFF_LINE_SPLIT
    ) {
        memcpy(&old_x
            , res->data + res->curr_checkpoint_beg + 1
            , sizeof(size_t)
        );
        memcpy(&old_y
            , res->data + res->curr_checkpoint_beg + 1 + sizeof(size_t)
            , sizeof(size_t)
        );
        memcpy(&old_size
            , res->data + res->curr_checkpoint_beg + 1 + 2 * sizeof(size_t)
            , sizeof(size_t)
        );
        if (x != 0 || old_x != 0 || old_y + old_size != y) {
            goto SPLIT_DIFFERENT_DIFF_CLASS;
        }
        old_size += size;
        memcpy(res->data + res->curr_checkpoint_beg + 1 + 2 * sizeof(size_t)
            , &old_size
            , sizeof(size_t)
        );
    } else {
SPLIT_DIFFERENT_DIFF_CLASS:
        res->curr_checkpoint_beg = res->curr_checkpoint_end;
        res->curr_checkpoint_end = res->curr_checkpoint_beg + SIZE_SPLIT;
        res->data[res->curr_checkpoint_beg] = DIFF_LINE_SPLIT;
        memcpy(res->data + res->curr_checkpoint_beg + 1, &x, sizeof(size_t));
        memcpy(res->data + res->curr_checkpoint_beg + 1 + sizeof(size_t)
            , &y
            , sizeof(size_t)
        );
        memcpy(res->data + res->curr_checkpoint_beg + 1 + 2 * sizeof(size_t)
            , &size
            , sizeof(size_t)
        );
        res->data[res->curr_checkpoint_end - 1] = DIFF_SPLIT_SEP;
    }
    res->last_checkpoint_beg = res->curr_checkpoint_beg;
    res->last_checkpoint_end = res->curr_checkpoint_end;
}

void
diffstack_insert_chars_add(struct diffstack **ds
    , uint8_t *str
    , size_t size
    , size_t x
    , size_t y
    ) {
    struct diffstack *res = *ds;
    uint8_t *seq_new_beg = str;
    uint8_t *seq_new_end = str + size;
    uint8_t *seq_old_beg;
    uint8_t *seq_old_end;
    uint32_t glyph_old;
    uint32_t glyph_new;
    size_t old_x;
    size_t old_y;

    if (size == 0) {
        return;
    }
    res = diffstack_reserve(ds, size);
    ensure(res);

    if (res->curr_checkpoint_beg != res->curr_checkpoint_end
        && res->data[res->curr_checkpoint_beg] == DIFF_CHARS_ADD
    ) {
        seq_old_beg = res->data + res->curr_checkpoint_beg + 2
            + sizeof(size_t) * 2;
        seq_old_end = res->data + res->curr_checkpoint_end - 1;
        memcpy(&old_x, res->data + res->curr_checkpoint_beg + 1
            , sizeof(size_t)
        );
        memcpy(&old_y
            , res->data + res->curr_checkpoint_beg + 1 + sizeof(size_t)
            , sizeof(size_t)
        );
        dbg_assert(seq_old_end > seq_old_beg);
        dbg_assert(seq_old_end - seq_old_beg != 0);
        if (old_y != y || old_x + (seq_old_end - seq_old_beg) != x) {
            goto ADD_DIFFERENT_DIFF_CLASS;
        }
        glyph_old = first_glyph(seq_old_beg, seq_old_end);
        glyph_new = first_glyph(seq_new_beg, seq_new_end);
        if (!is_same_class(glyph_old, glyph_new)) {
            goto ADD_DIFFERENT_DIFF_CLASS;
        }
        dbg_assert(seq_old_end[0] == DIFF_CHAR_SEQ);
        memcpy(seq_old_end, str, size);
        seq_old_end[size] = DIFF_CHAR_SEQ;
        res->curr_checkpoint_end += size;
    } else {
ADD_DIFFERENT_DIFF_CLASS:
        res->curr_checkpoint_beg = res->curr_checkpoint_end;
        res->curr_checkpoint_end = res->curr_checkpoint_beg + EMPTY_DIFF
            + size;
        res->data[res->curr_checkpoint_beg] = DIFF_CHARS_ADD;
        memcpy(res->data + res->curr_checkpoint_beg + 1, &x, sizeof(size_t));
        memcpy(res->data + res->curr_checkpoint_beg + 1 + sizeof(size_t)
            , &y
            , sizeof(size_t)
        );
        memcpy(res->data + res->curr_checkpoint_beg + 2 + 2 * sizeof(size_t)
            , str
            , size
        );
        res->data[res->curr_checkpoint_beg + 1 + 2 * sizeof(size_t)]
            = DIFF_CHAR_SEQ;
        res->data[res->curr_checkpoint_beg + EMPTY_DIFF + size - 1]
            = DIFF_CHAR_SEQ;
    }
    res->last_checkpoint_beg = res->curr_checkpoint_beg;
    res->last_checkpoint_end = res->curr_checkpoint_end;
}

void
diffstack_insert_chars_del(struct diffstack **ds
    , uint8_t *str
    , size_t size
    , size_t x
    , size_t y
    ) {
    struct diffstack *res = *ds;
    size_t x_end = x + size;;
    uint8_t *seq_new_beg = str;
    uint8_t *seq_new_end = str + size;
    uint8_t *seq_old_beg;
    uint8_t *seq_old_end;
    uint32_t glyph_old;
    uint32_t glyph_new;
    size_t old_x;
    size_t old_y;

    if (size == 0) {
        return;
    }
    res = diffstack_reserve(ds, size);
    ensure(res);

    if (res->curr_checkpoint_beg != res->curr_checkpoint_end
        && res->data[res->curr_checkpoint_beg] == DIFF_CHARS_DEL
    ) {
        seq_old_beg = res->data + res->curr_checkpoint_beg + 2
            + sizeof(size_t) * 2;
        seq_old_end = res->data + res->curr_checkpoint_end - 1;
        memcpy(&old_x, res->data + res->curr_checkpoint_beg + 1
            , sizeof(size_t)
        );
        memcpy(&old_y
            , res->data + res->curr_checkpoint_beg + 1 + sizeof(size_t)
            , sizeof(size_t)
        );
        dbg_assert(seq_old_end > seq_old_beg);
        dbg_assert(seq_old_end - seq_old_beg != 0);
        if (old_y != y
            || old_x + seq_old_end < seq_old_beg
            || old_x - (seq_old_end - seq_old_beg) != x_end
        ) {
            goto DEL_DIFFERENT_DIFF_CLASS;
        }
        glyph_old = first_glyph(seq_old_beg, seq_old_end);
        glyph_new = first_glyph(seq_new_beg, seq_new_end);
        if (!is_same_class(glyph_old, glyph_new)) {
            goto DEL_DIFFERENT_DIFF_CLASS;
        }
        dbg_assert(seq_old_end[0] == DIFF_CHAR_SEQ);
        rmemcpy(seq_old_end, str, size);
        seq_old_end[size] = DIFF_CHAR_SEQ;
        res->curr_checkpoint_end += size;
    } else {
DEL_DIFFERENT_DIFF_CLASS:
        res->curr_checkpoint_beg = res->curr_checkpoint_end;
        res->curr_checkpoint_end = res->curr_checkpoint_beg + EMPTY_DIFF
            + size;
        res->data[res->curr_checkpoint_beg] = DIFF_CHARS_DEL;
        memcpy(res->data + res->curr_checkpoint_beg + 1
            , &x_end
            , sizeof(size_t)
        );
        memcpy(res->data + res->curr_checkpoint_beg + 1 + sizeof(size_t)
            , &y
            , sizeof(size_t)
        );
        rmemcpy(res->data + res->curr_checkpoint_beg + 2 + 2 * sizeof(size_t)
            , str
            , size
        );
        res->data[res->curr_checkpoint_beg + 1 + 2 * sizeof(size_t)]
            = DIFF_CHAR_SEQ;
        res->data[res->curr_checkpoint_beg + EMPTY_DIFF + size - 1]
            = DIFF_CHAR_SEQ;
    }
    res->last_checkpoint_beg = res->curr_checkpoint_beg;
    res->last_checkpoint_end = res->curr_checkpoint_end;
}

void
diffstack_undo_line_merge(uint8_t *diff_beg
    , uint8_t *diff_end
    , struct document *doc
    ) {
    size_t x;
    size_t y;
    size_t n;
    struct line_metadata *lm_fst;
    struct line_metadata *lm_snd;

    dbg_assert(diff_end == diff_beg + SIZE_SPLIT);
    dbg_assert(*diff_beg == DIFF_LINE_MERGE);

    memcpy(&x, diff_beg + 1, sizeof(size_t));
    memcpy(&y, diff_beg + 1 + sizeof(size_t), sizeof(size_t));
    memcpy(&n, diff_beg + 1 + 2 * sizeof(size_t), sizeof(size_t));

    ensure(n != 0);

    lm_fst = doc->lines + y;
    lm_snd = lm_fst + n;

    dbg_assert(lm_snd < doc->lines + doc->loaded_size);
    dbg_assert(doc->loaded_size + 1 + n <= doc->alloc);

    insert_n_line(x, y, n, &doc);
}

void
diffstack_undo_line_split(uint8_t *diff_beg
    , uint8_t *diff_end
    , struct document *doc
    ) {
    size_t x;
    size_t y;
    size_t n;
    struct line_metadata *lm_fst;
    struct line_metadata *lm_snd;
    struct line_metadata *lm_curr;

    dbg_assert(diff_end == diff_beg + SIZE_SPLIT);
    dbg_assert(*diff_beg == DIFF_LINE_SPLIT);

    memcpy(&x, diff_beg + 1, sizeof(size_t));
    memcpy(&y, diff_beg + 1 + sizeof(size_t), sizeof(size_t));
    memcpy(&n, diff_beg + 1 + 2 * sizeof(size_t), sizeof(size_t));

    ensure(n != 0);

    lm_fst = doc->lines + y;
    lm_snd = lm_fst + n;

    merge_lines(lm_fst, lm_snd, doc);

    for (lm_curr = lm_fst + 1; lm_curr != lm_snd + 1; lm_curr++) {
        free_line(doc, &lm_curr->line);
    }
    memcpy(lm_fst + 1, lm_snd + 1, (doc->loaded_size + 1 - n) * sizeof(*lm_fst));
    lm_fst->win_lines = 0;
    doc->loaded_size -= n;
}

void
diffstack_undo_chars_del(uint8_t *diff_beg
    , uint8_t *diff_end
    , struct document *doc
    ) {
    size_t x;
    size_t y;
    uint8_t *seq_beg;
    uint8_t *seq_end;
    size_t seq_size;
    struct line_metadata *lm;
    uint8_t *restore_seq_beg;
    uint8_t *restore_seq_end;

    dbg_assert(diff_end > diff_beg + EMPTY_DIFF);
    dbg_assert(*diff_beg == DIFF_CHARS_DEL);

    memcpy(&x, diff_beg + 1, sizeof(size_t));
    memcpy(&y, diff_beg + 1 + sizeof(size_t), sizeof(size_t));

    seq_beg = diff_beg + 2 + 2 * sizeof(size_t);
    seq_end = diff_end - 1;
    seq_size = seq_end - seq_beg;

    dbg_assert(seq_end > seq_beg);

    lm = doc->lines + y;
    ensure(!is_line_internal(&lm->line, doc));

    reserve_extern_line(&lm->line, seq_size, doc);

    restore_seq_end = lm->line.extern_line->data + x;
    restore_seq_beg = restore_seq_end - seq_size;

    memmove(restore_seq_end, restore_seq_beg, lm->line.size);
    rmemcpy(restore_seq_beg, seq_beg, seq_size);

    lm->line.size += seq_size;
    lm->win_lines = 0;
}

void
diffstack_undo_chars_add(uint8_t *diff_beg
    , uint8_t *diff_end
    , struct document *doc
    ) {
    size_t x;
    size_t y;
    uint8_t *seq_beg;
    uint8_t *seq_end;
    size_t seq_size;
    struct line_metadata *lm;
    uint8_t *restore_seq_beg;
    uint8_t *restore_seq_end;

    dbg_assert(diff_end > diff_beg + EMPTY_DIFF);
    dbg_assert(*diff_beg == DIFF_CHARS_ADD);

    memcpy(&x, diff_beg + 1, sizeof(size_t));
    memcpy(&y, diff_beg + 1 + sizeof(size_t), sizeof(size_t));

    seq_beg = diff_beg + 2 + 2 * sizeof(size_t);
    seq_end = diff_end - 1;
    seq_size = seq_end - seq_beg;

    dbg_assert(seq_end > seq_beg);

    lm = doc->lines + y;
    ensure(!is_line_internal(&lm->line, doc));

    restore_seq_beg = lm->line.extern_line->data + x;
    restore_seq_end = restore_seq_beg + seq_size;

    memmove(restore_seq_beg, restore_seq_end, lm->line.size);

    dbg_assert(lm->line.size >= seq_size);
    lm->line.size -= seq_size;
    lm->win_lines = 0;
}

uint8_t *
diffstack_curr_mvback(uint8_t *curr_end) {
    uint8_t *seq_curr;
    size_t aggr_size;
    size_t i;

    switch (curr_end[-1]) {
        case DIFF_SPLIT_SEP:
            return curr_end - SIZE_SPLIT;
        case DIFF_CHAR_SEQ:
            seq_curr = curr_end - 2;
            while (*seq_curr != DIFF_CHAR_SEQ) {
                seq_curr--;
            }
            return seq_curr - (sizeof(size_t) * 2 + 1);
        case DIFF_AGGR_SEQ:
            curr_end -= SIZE_AGGR;
            memcpy(&aggr_size, curr_end, sizeof(aggr_size));
            for (i = 0; i < aggr_size; i++) {
                curr_end = diffstack_curr_mvback(curr_end);
                ensure(curr_end);
            }
            return curr_end - SIZE_AGGR;
        default:
            dbg_assert(0);
            return NULL;
    }
}

int
diffstack_undo(struct diffstack *ds
    , struct document *doc
    ) {
    uint8_t *diff_beg = ds->data + ds->curr_checkpoint_beg;
    uint8_t *diff_end = ds->data + ds->curr_checkpoint_end;

    if (diff_beg == diff_end) {
        return -1;
    }
    switch (*diff_beg) {
        case DIFF_CHARS_ADD:
            diffstack_undo_chars_add(diff_beg, diff_end, doc);
            break;
        case DIFF_CHARS_DEL:
            diffstack_undo_chars_del(diff_beg, diff_end, doc);
            break;
        case DIFF_LINE_SPLIT:
            diffstack_undo_line_split(diff_beg, diff_end, doc);
            break;
        case DIFF_LINE_MERGE:
            diffstack_undo_line_merge(diff_beg, diff_end, doc);
            break;
        case DIFF_AGGREGATE:
        default:
            ensure(0);
    }
    if (ds->curr_checkpoint_beg != 0) {
        diff_end = diff_beg;
        diff_beg = diffstack_curr_mvback(diff_end);
        ensure(diff_beg >= ds->data);
        ds->curr_checkpoint_beg = diff_beg - ds->data;
        ds->curr_checkpoint_end = diff_end - ds->data;
    } else {
        ds->curr_checkpoint_end = 0;
    }
    return 0;
}

int
init_diffstack(struct diffstack **ds, size_t alloc) {
    struct diffstack *res;

    ensure(reallocflexarr((void **) ds
        , sizeof(struct diffstack)
        , alloc
        , sizeof(uint8_t)
    ));
    res = *ds;
    memset(res, 0, sizeof(struct diffstack));
    res->alloc = alloc;
    return 0;
}

void
window_render(void) {
    unsigned size = win.width * win.height;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6 * size);
    glutSwapBuffers();
}

void
invalidate_win_line_metadata(struct document *doc) {
    struct line_metadata *beg = doc->lines;
    struct line_metadata *end = doc->lines + doc->loaded_size;

    while (beg != end) {
        beg->win_lines = 0;
        beg++;
    }
}

void
resize_display_matrix(int win_width_px, int win_height_px) {
    unsigned win_width = win_width_px / H_GLYPH_PX;
    unsigned win_height = win_height_px / V_GLYPH_PX;
    unsigned size = win_width * win_height;

    gen_display_matrix(&win, win_width, win_height);
    invalidate_win_line_metadata(doc);
    fill_screen(&doc, &win);

    glBufferData(GL_ARRAY_BUFFER
        , (sizeof(struct quad_coord) * 2 + sizeof(struct quad_color)) * size
        , NULL
        , GL_DYNAMIC_DRAW
    );
    glBufferSubData(GL_ARRAY_BUFFER
        , 0
        , sizeof(struct quad_coord) * size
        , win.window_mesh
    );
    glBufferSubData(GL_ARRAY_BUFFER
        , sizeof(struct quad_coord) * size
        , sizeof(struct quad_coord) * size
        , win.glyph_mesh
    );
    glBufferSubData(GL_ARRAY_BUFFER
        , sizeof(struct quad_coord) * 2 * size
        , sizeof(struct quad_color) * size
        , win.font_color
    );
    glVertexAttribPointer(gl_id.pos, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glVertexAttribPointer(gl_id.uv, 2, GL_FLOAT, GL_FALSE, 0
        , (void *)(sizeof(struct quad_coord) * size)
    );
    glVertexAttribPointer(gl_id.col, 3, GL_FLOAT, GL_FALSE, 0
        , (void *)(sizeof(struct quad_coord) * 2 * size)
    );
}

void
window_resize(int win_width_px, int win_height_px) {
    glViewport(0, 0, win_width_px, win_height_px);
    doc->y_window_line_off = 0;
    resize_display_matrix(win_width_px, win_height_px);
}

static const char *vs_src = "\n\
#version 130                 \n\
in vec4 s_pos;               \n\
in vec4 s_color;             \n\
in vec2 s_uv;                \n\
out vec4 color;              \n\
out vec2 uv;                 \n\
void main() {                \n\
    color = s_color;         \n\
    uv = s_uv;               \n\
    gl_Position = s_pos;     \n\
}                            \n\
";

static const char *fs_src  = "                      \n\
#version 130                                        \n\
in vec4 color;                                      \n\
in vec2 uv;                                         \n\
out vec4 fColor;                                    \n\
uniform sampler2D texture;                          \n\
void main() {                                       \n\
    vec4 bg = vec4(0.152, 0.156, 0.13, 1.0);        \n\
    fColor = texture2D(texture, uv);                \n\
    if (color.r <= 1.0) {                           \n\
        fColor = fColor.r * color + bg;             \n\
    } else {                                        \n\
        vec4 hilight = vec4(1.0, 1.0, 1.0, 1.0);    \n\
        fColor = (1.0 - fColor.r) * hilight;        \n\
    }                                               \n\
}                                                   \n\
";

void
key_input(unsigned char key, int x __unused, int y __unused) {

    switch (key) {
        case 'd':
            diff_line_insert(&diff, 0, doc->lines, doc, "asd", 3);
            break;
        case 's':
            diff_line_remove(&diff, 0, doc->lines, doc, 3);
            break;
        case 'u':
            diffstack_undo(diff, doc);
            break;
        case 'p':
            diff_line_split(&diff, 4, 0, &doc);
            break;
        case 'm':
            diff_line_merge(&diff, 0, 1, &doc);
            break;
        case 'q': case 27:
            exit(0);
        default:
            return;
    }
    fill_screen(&doc, &win);
    gl_buffers_upload(&win);
    glutPostRedisplay();
}

void
key_special_input(int key, int x __unused, int y __unused) {

    switch (key) {
        case GLUT_KEY_DOWN:
            move_scrollback(&win, MV_VERT_DOWN, 1, &doc);
            break;
        case GLUT_KEY_UP:
            move_scrollback(&win, MV_VERT_UP, 1, &doc);
            break;
        case GLUT_KEY_PAGE_DOWN:
            move_scrollback(&win, MV_VERT_DOWN, win.height, &doc);
            break;
        case GLUT_KEY_PAGE_UP:
            move_scrollback(&win, MV_VERT_UP, win.height, &doc);
            break;
        default:
            return;
    }
    glutPostRedisplay();
}

int
gl_check_program(GLuint id, int status) {
    static char info[1024];
    int len = 1024;
    GLint ok;

    if (status == GL_COMPILE_STATUS) {
        glGetShaderiv(id, status, &ok);
        if (ok == GL_FALSE) {
            glGetShaderInfoLog(id, len, &len, info);
        }
    } else {
        xensure(status == GL_LINK_STATUS);
        glGetProgramiv(id, status, &ok);
        if (ok == GL_FALSE) {
            glGetProgramInfoLog(id, len, &len, info);
        }
    }
    if (ok == GL_FALSE) {
        fprintf(stderr, "GL_ERROR :\n%s\n", info);
    }
    return ok == GL_TRUE;
}

GLuint
gl_compile_program(const char *vs_src, const char *fs_src) {
    GLuint pr = glCreateProgram();
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    xensure(glGetError() == GL_NO_ERROR);

    glShaderSource(vs, 1, &vs_src, NULL);
    glCompileShader(vs);
    xensure(gl_check_program(vs, GL_COMPILE_STATUS));

    glShaderSource(fs, 1, &fs_src, NULL);
    glCompileShader(fs);
    xensure(gl_check_program(fs, GL_COMPILE_STATUS));

    glAttachShader(pr, vs);
    glAttachShader(pr, fs);
    glLinkProgram(pr);
    xensure(gl_check_program(pr, GL_LINK_STATUS));
    return pr;
}

char *
str_intercalate(char *buf
    , size_t bufsz
    , char **strarr
    , size_t strnum
    , char c
    ) {
    char **str_curr = strarr;
    char **str_end = strarr + strnum;
    char *curr_end = buf;
    size_t len;

    while (bufsz && str_curr != str_end) {
        if (str_curr != strarr) {
            *curr_end++ = c;
            if (--bufsz == 0) {
                break;
            }
        }
        curr_end = strncat(curr_end, *str_curr++, bufsz);
        len = strlen(curr_end);
        curr_end += len;
        bufsz -= len;
    }
    return buf;
}

int
win_init(int argc, char *argv[]) {
    static char window_title[128];

    str_intercalate(window_title, sizeof(window_title) - 1, argv, argc, ' '); 
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
    glutCreateWindow(window_title);
    glutReshapeFunc(window_resize);
    glutDisplayFunc(window_render);
    glutKeyboardFunc(key_input);
    glutSpecialFunc(key_special_input);
    xensure(glewInit() == GLEW_OK);
    glClearColor(0.152, 0.156, 0.13, 1.0);
    return 0;
}

void
set_quad_coord(struct quad_coord *q, float x, float y, float dx, float dy) {
    q->vertex_pos[0].x = x;
    q->vertex_pos[0].y = y;
    q->vertex_pos[1].x = x;
    q->vertex_pos[1].y = y + dy;
    q->vertex_pos[2].x = x + dx;
    q->vertex_pos[2].y = y;
    q->vertex_pos[3].x = x + dx;
    q->vertex_pos[3].y = y;
    q->vertex_pos[4].x = x;
    q->vertex_pos[4].y = y + dy;
    q->vertex_pos[5].x = x + dx;
    q->vertex_pos[5].y = y + dy;
}

void
set_quad_color(struct quad_color *q, float r, float g, float b) {
    int i;

    for (i = 0; i < 6; i++) {
        q->vertex_color[i].r = r;
        q->vertex_color[i].g = g;
        q->vertex_color[i].b = b;
    }
}

void
fill_window_mesh(struct window *win, unsigned width, unsigned height) {
    float x_beg = -1.0f;
    float y_beg = +1.0f;
    float dx = +2.0f / width;
    float dy = -2.0f / height;
    float x;
    float y;
    unsigned i;
    unsigned j;

    y = y_beg;
    for (i = 0; i < height; i++) {
        x = x_beg;
        for (j = 0; j < width; j++) {
            set_quad_coord(win->window_mesh + i * width + j
                , x - dx * 1/128
                , y - dy * 1/128
                , dx + dx * 2/128
                , dy + dy * 2/128
            );
            x += dx;
        }
        y += dy;
    }
}

void
gen_display_matrix(struct window *win, unsigned width, unsigned height) {
    size_t scrollback = 2 * height;
    size_t window_area = width * height;
    size_t scrollback_area = width * scrollback;
    size_t window_mesh_sz = sizeof(struct quad_coord) * window_area;
    size_t glyph_mesh_sz  = sizeof(struct quad_coord) * scrollback_area;
    size_t font_color_sz  = sizeof(struct quad_color) * scrollback_area;
    size_t win_mem_sz =  window_mesh_sz + glyph_mesh_sz + font_color_sz;

    xensure(reallocarr(&win->data, win_mem_sz, 1));
    win->width = width;
    win->height = height;
    win->scrollback_size = scrollback;
    win->scrollback_pos = 0;
    win->window_mesh = win->data;
    win->glyph_mesh = win->window_mesh + window_area;
    win->font_color = (void *)(win->glyph_mesh + scrollback_area);
    fill_window_mesh(win, width, height);
    memzero(win->glyph_mesh, 1, glyph_mesh_sz + font_color_sz);
}

int
load_font(const char *fname __unused, struct mmap_file *file) {
#ifdef LINK_FONT
    extern const char _binary_ext_unifont_cfp_start;
    extern const char _binary_ext_unifont_cfp_end;
    file->size = &_binary_ext_unifont_cfp_end - &_binary_ext_unifont_cfp_start;
    file->data = (void *)&_binary_ext_unifont_cfp_start;
    return 0;
#else
    return load_file(fname, file);
#endif
}

void
unload_font(struct mmap_file *file __unused) {
#ifndef LINK_FONT
    unload_file(file);
#endif
}

int
gl_pipeline_init() {
    unsigned win_width_px = 0;
    unsigned win_height_px = 0;
    void *font_data = malloc(0x1440000);
    struct mmap_file file;
    struct rfp_file *rfp;

    xensure(font_data);
    xensure(load_font("unifont.cfp", &file) == 0);
    rfp = load_cfp_file(&file);
    rfp_decompress(rfp, font_data);
    gl_id.prog = gl_compile_program(vs_src, fs_src);
    glGenVertexArrays(1, &gl_id.vao);
    glBindVertexArray(gl_id.vao);
    glGenBuffers(1, &gl_id.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, gl_id.vbo);
    gl_id.pos = glGetAttribLocation(gl_id.prog, "s_pos");
    gl_id.uv = glGetAttribLocation(gl_id.prog, "s_uv");
    gl_id.col = glGetAttribLocation(gl_id.prog, "s_color");
    xensure(glGetError() == GL_NO_ERROR);

    memzero(&win, 1, sizeof(win));
    resize_display_matrix(win_width_px, win_height_px);

    glEnableVertexAttribArray(gl_id.pos);
    glEnableVertexAttribArray(gl_id.col);
    glEnableVertexAttribArray(gl_id.uv);

    glEnable(GL_TEXTURE_2D);
    glGenTextures(1, &gl_id.tbo);
    glBindTexture(GL_TEXTURE_2D, gl_id.tbo);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 0x1200, 0x1200, 0, GL_RED
        , GL_UNSIGNED_BYTE
        , font_data
    );
    unload_font(&file);
    free(font_data);

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glActiveTexture(GL_TEXTURE0);
    gl_id.tex = glGetUniformLocation(gl_id.prog, "texture");
    glUniform1i(gl_id.tex, 0);

    glUseProgram(gl_id.prog);
    return 0;
}

void
free_line(struct document *doc, struct line *line) {
    if (!is_line_internal(line, doc)) {
        free(line->extern_line);
    }
}

struct extern_line *
reserve_line(struct line *line, size_t size, struct document *doc) {

    if (is_line_internal(line, doc)) {
        return reserve_intern_line(line, size, doc);
    } else {
        return reserve_extern_line(line, size, doc);
    }
}

struct extern_line *
reserve_extern_line(struct line *line, size_t size, struct document *doc) {
    size_t alloc = line->alloc;
    uint8_t *str_beg;
    uint8_t *str_end;

    dbg_assert(!is_line_internal(line, doc));
    dbg_assert(line->ptr == NULL || line->alloc != 0);

    if (line->size + size > alloc || line->ptr == NULL) {
        alloc = line->size + line->size / 2 + size;
        ensure(reallocflexarr((void **)&line->extern_line
            , sizeof(struct extern_line)
            , alloc
            , sizeof(uint8_t)
        ) != NULL);
        line->alloc = alloc;
    }
    str_beg = line->extern_line->data + line->size;
    str_end = line->extern_line->data + line->size + size;

    if (RUNTIME_INSTR) {
        memzero(str_beg, str_end - str_beg, sizeof(*str_beg));
    }
    return line->extern_line;
}

struct extern_line *
reserve_intern_line(struct line *line, size_t size, struct document *doc) {
    struct extern_line *el = NULL;
    uint8_t *str_beg;
    uint8_t *str_end;
    size_t alloc;

    dbg_assert(is_line_internal(line, doc));

    alloc = line->size + size;
    ensure(reallocflexarr((void **)&el
        , sizeof(struct extern_line)
        , alloc
        , sizeof(uint8_t)
    ) != NULL);
    line->alloc = alloc;
    str_beg = line->intern_line;
    str_end = line->intern_line + line->size;

    el->utf8_status = UTF8_CLEAN;
    memcpy(el->data, str_beg, str_end - str_beg);

    line->extern_line = el;

    str_beg = el->data + line->size;
    str_end = str_beg + size;

    if (RUNTIME_INSTR) {
        memzero(str_beg, str_end - str_beg, sizeof(*str_beg));
    }
    return el;
}

void
merge_lines(struct line_metadata *lm_fst
    , struct line_metadata *lm_snd
    , struct document *doc
    ) {
    size_t s_size = lm_snd->line.size;
    size_t f_size = lm_fst->line.size;
    struct extern_line *el = reserve_line(&lm_fst->line, s_size, doc);

    if (!is_line_internal(&lm_snd->line, doc)) {
        el->utf8_status |= lm_snd->line.extern_line->utf8_status;
    }
    memcpy(el->data + f_size, begin_line_metadata(lm_snd, doc), s_size);
    lm_fst->line.size += s_size;
}

void
init_extern_line(struct line *line
    , uint8_t *str
    , size_t size
    , enum UTF8_STATUS utf8_status
    , struct document *doc
    ) {
    ensure(line->ptr == NULL);
    reserve_extern_line(line, size, doc);
    line->extern_line->utf8_status = utf8_status;
    line->size = size;
    memcpy(line->extern_line->data, str, size);
}

uint8_t *
load_line(struct line_metadata *lm
    , uint8_t *beg
    , uint8_t *end
    , unsigned win_width
    ) {
    size_t win_lines = 0;
    unsigned curr_width = 0;
    uint8_t *curr = beg;
    size_t line_size;

    ensure(lm->line.size == 0);
    ensure(lm->line.alloc == 0);
    ensure(win_width > 0);

    do {
        if (curr == end) {
            win_lines++;
            lm->win_lines = win_lines;
            lm->line.size = curr - beg;
            lm->line.intern_line = beg;
            return curr;
        }
        if (*curr == '\n') {
            win_lines++;
            lm->win_lines = win_lines;
            lm->line.size = curr - beg;
            lm->line.intern_line = beg;
            return curr + 1;
        }
        curr_width++;
        if (*curr == '\t') {
            curr_width += curr_width % TAB_SIZE;
        }
        if (curr_width >= win_width) {
            win_lines++;
            curr_width = 0;
        }
        curr = next_utf8_or_null(curr, end);
        if (curr == NULL) {
            goto PARSE_DIRTY;
        }
    } while (1);

PARSE_DIRTY:
    curr = beg;
    while (curr < end && *curr != '\n') {
        curr++;
    }
    line_size = curr - beg;
    lm->line.ptr = NULL;
    init_extern_line(&lm->line, beg, line_size, UTF8_DIRTY, doc);
    lm->win_lines = line_size / win_width + (line_size % win_width != 0);

    if (curr == end) {
        return curr;
    } else {
        return curr + 1;
    }
}

uint8_t *
next_utf8_or_null(uint8_t *curr, uint8_t *end) {
    if (curr == end) {
        return curr;
    }
    if ((*curr & 0x80) == 0) {
        curr += 1;
    } else if ((*curr & 0xe0) == 0xc0) {
        if ((curr + 2 >= end)
            || ((curr[1] & 0xc0) != 0x80)
        ) {
            return NULL;
        }
        curr += 2;
    } else if ((*curr & 0xf0) == 0xe0) {
        if ((curr + 3 >= end)
            || ((curr[1] & 0xc0) != 0x80)
            || ((curr[2] & 0xc0) != 0x80)
        ) {
            return NULL;
        }
        curr += 3;
    } else if ((*curr & 0xf8) == 0xf0) {
        if ((curr + 4 >= end)
            || ((curr[1] & 0xc0) != 0x80)
            || ((curr[2] & 0xc0) != 0x80)
            || ((curr[3] & 0xc0) != 0x80)
        ) {
            return NULL;
        }
        curr += 4;
    } else {
        return NULL;
    }
    return curr;
}

int
is_fully_loaded(struct document *doc) {
    struct line_metadata *last_line = doc->lines + doc->loaded_size;
    uint8_t *doc_end = doc->file.data + doc->file.size;
    return last_line->line.intern_line == doc_end;
}

int
is_line_internal(struct line *line, struct document *doc) {
    uint8_t *doc_beg = doc->file.data;
    uint8_t *doc_end = doc->file.data + doc->file.size;
    if (line->intern_line >= doc_beg && line->intern_line <= doc_end) {
        return 1;
    }
    return 0;
} 

int
is_line_utf8(struct line *line, struct document *doc) {
    if (is_line_internal(line, doc)) {
        return 1;
    }
    return line->extern_line->utf8_status == UTF8_CLEAN;
}

int
load_lines(size_t nlines, struct document **doc) {
    struct document *res = *doc;
    struct line_metadata *trampoline = res->lines + res->loaded_size;
    uint8_t *doc_beg = res->file.data;
    uint8_t *doc_end = doc_beg + res->file.size;
    uint8_t *to_load;
    struct line_metadata *lm_end;

    dbg_assert(trampoline->line.size == 0
        && trampoline->line.alloc == 0
        && trampoline->line.intern_line >= doc_beg
        && trampoline->line.intern_line <= doc_end
    );

    if (res->file.size == 0) {
        return 0;
    }
    if (trampoline->line.ptr == doc_end) {
        return 0;
    }
    ensure(resize_document_by(nlines, doc) == 0);
    res = *doc;
    trampoline = res->lines + res->loaded_size;
    doc_end = res->file.data + res->file.size;

    to_load = trampoline->line.intern_line;
    lm_end = trampoline + nlines;
    while (trampoline != lm_end && to_load != doc_end) {
        to_load = load_line(trampoline, to_load, doc_end, win.width);
        trampoline++;
    }
    dbg_assert(trampoline == res->lines + res->loaded_size + nlines
        || to_load == doc_end);
    dbg_assert(to_load <= doc_end);
    trampoline->line.ptr = to_load;
    res->loaded_size = trampoline - res->lines;

    if (trampoline->line.ptr == doc_end) {
        res->alloc = res->loaded_size + 1;
        ensure(reallocflexarr((void **)doc
                , sizeof(struct document)
                , res->alloc
                , sizeof(struct line_metadata)
        ));
    }
    return 0;
}

uint32_t
glyph_from_utf8(uint8_t **line) {
    uint8_t *curr = *line;

    if ((*curr & 0x80) == 0x00) {
        *line = curr + 1;
        return 0u
            | curr[0]
            ;
    }
    if ((*curr & 0xe0) == 0xc0) {
        *line = curr + 2;
        return 0u
            | ((curr[0] & 0x1f) << 0x06)
            | ((curr[1] & 0x3f) << 0x00)
            ;
    }
    if ((*curr & 0xf0) == 0xe0) {
        *line = curr + 3;
        return 0u
            | ((curr[0] & 0x0f) << 0x0c)
            | ((curr[1] & 0x3f) << 0x06)
            | ((curr[2] & 0x3f) << 0x00)
            ;
    }
    if ((*curr & 0xf8) == 0xf0) {
        *line = curr + 4;
        return 0u
            | ((curr[0] & 0x07) << 0x12)
            | ((curr[1] & 0x3f) << 0x0c)
            | ((curr[2] & 0x3f) << 0x06)
            | ((curr[3] & 0x3f) << 0x00)
            ;
    }
    dbg_assert(0);
    return 0u;
}

int
resize_document_by(size_t size, struct document **doc) {
    struct document *res = *doc;
    size_t alloc = res->alloc;
    size_t incr_size = res->loaded_size + 1 + size;
    struct line_metadata *lm;

    dbg_assert(res->alloc > res->loaded_size && size > 0);

    if (incr_size > alloc) {
        alloc = incr_size + size / 2;
        res = reallocflexarr((void **)doc
            , sizeof(struct document)
            , alloc
            , sizeof(struct line_metadata)
        );
        if (res == NULL) {
            return -1;
        }
        res->alloc = alloc;
    }
    lm = res->lines + res->loaded_size + 1;
    memzero(lm, size, sizeof(*lm));

    dbg_assert(res->alloc > res->loaded_size);
    return 0;
}

int
init_doc(const char *fname, struct document **doc) {
    int fd = open(fname, O_RDONLY);
    struct mmap_file file;
    struct document *res;
    struct line_metadata *fst_line;
    size_t alloc;
    off_t length;

    xensure_errno(fd != -1);
    length = lseek(fd, 0, SEEK_END);
    xensure_errno(length != -1);
    xensure_errno(lseek(fd, 0, SEEK_SET) != -1);
    file.size = length;

    // mappint one byte past the file size, so that the pointer to end of the
    // file will never overlap with user allocated memory
    file.data = mmap(NULL, file.size + 1, PROT_READ, MAP_PRIVATE, fd, 0);
    xensure_errno(file.data != MAP_FAILED);
    close(fd);

    alloc = file.size / 32 + 1;
    res = reallocflexarr((void **)doc
        , sizeof(struct document)
        , alloc
        , sizeof(struct line_metadata)
    );
    ensure(res != NULL);
    res = *doc;
    res->file = file;
    res->alloc = alloc;
    res->loaded_size = 0;
    res->y_line_off = 0;
    res->y_window_line_off = 0;
    res->x_off = 0;
    fst_line = res->lines;
    fst_line->win_lines = 0;
    fst_line->line.size = 0;
    fst_line->line.alloc = 0;
    fst_line->line.intern_line = file.data;
    return 0;
}

void
fill_glyph_narrow(unsigned i, unsigned j, uint32_t glyph, struct window *win) {
    float d_glyph = D_GLYPH;
    float glyph_height = D_GLYPH;
    float glyph_width = D_GLYPH * 23/32;
    uint32_t glyph_x = (glyph >> 0) & 0xff;
    uint32_t glyph_y = (glyph >> 8) & 0xff;

    set_quad_coord(win->glyph_mesh + i * win->width + j
        , -d_glyph * .08 + glyph_x * d_glyph + d_glyph * 1/18
        , -d_glyph * .00 + glyph_y * d_glyph + d_glyph * 1/18
        , -d_glyph * .08 + glyph_width  - d_glyph * 1/18
        , -d_glyph * .00 + glyph_height - d_glyph * 1/18
    );
}

void
fill_glyph_wide(unsigned i, unsigned j, uint32_t glyph, struct window *win) {
    float d_glyph = D_GLYPH;
    float glyph_height = D_GLYPH;
    float glyph_width = D_GLYPH;
    uint32_t glyph_x = (glyph >> 0) & 0xff;
    uint32_t glyph_y = (glyph >> 8) & 0xff;

    set_quad_coord(win->glyph_mesh + i * win->width + j
        , glyph_x * d_glyph
        , glyph_y * d_glyph
        , glyph_width
        , glyph_height 
    );
}

unsigned
fill_glyph(unsigned i, unsigned j, uint32_t glyph, struct window *win) {
    if (glyph == '\t') {
        while (1) {
            fill_glyph_narrow(i, j, 0x20, win);
            if (++j % TAB_SIZE == 0 || j == win->width) {
                return j;
            }
        }
    } else if (glyph < 0x10000) {
        if (!is_glyph_wide(glyph)) {
            fill_glyph_narrow(i, j, glyph, win);
        } else {
            fill_glyph_wide(i, j, glyph, win);
        }
    } else {
        fill_glyph_wide(i, j, 0xffff, win);
    }
    return j + 1;
}

static struct color colors_table[] = {
    [0] = {0.85, 1.00, 1.00}, // light gray
    [1] = {1.00, 0.00, 0.37}, // dark pink
    [2] = {1.00, 0.84, 0.37}, // yellow
    [3] = {0.52, 0.68, 0.37}, // green
    [4] = {0.52, 0.37, 0.52}, // dark purple
    [5] = {0.68, 0.68, 0.52}, // light green
    [6] = {0.84, 0.52, 0.37}, // orange
};

unsigned
fill_line(unsigned i
    , enum UTF8_STATUS utf8_status
    , uint8_t *line_beg
    , uint8_t *line_end
    , struct window *win
    ) {
    uint8_t *line_curr = line_beg;
    unsigned j = 0;
    float def_r = colors_table[0].r;
    float def_g = colors_table[0].g;
    float def_b = colors_table[0].b;
    struct colored_span cw = { .color_pos = 0, .size = 0 };
    struct quad_color *qc_curr;
    uint32_t glyph;


    if (utf8_status == UTF8_CLEAN) {
        while (line_curr != line_end) {
            if (cw.size == 0) {
                cw = color_span(line_beg, line_curr, line_end);
            }
            qc_curr = win->font_color + i * win->width + j;
            set_quad_color(qc_curr
                , colors_table[cw.color_pos].r
                , colors_table[cw.color_pos].g
                , colors_table[cw.color_pos].b
            );
            glyph = glyph_from_utf8(&line_curr);
            dbg_assert(line_curr <= line_end);
            j = fill_glyph(i, j, glyph, win);
            if (j == win->width) {
                j = 0;
                i++;
            }
            if (i == win->scrollback_size) {
                return i;
            }
            cw.size--;
        }
    } else {
        while (line_curr != line_end) {
            qc_curr = win->font_color + i * win->width + j;
            set_quad_color(qc_curr, def_r, def_g, def_b);
            j = fill_glyph(i, j, *line_curr++, win);
            if (j == win->width) {
                j = 0;
                i++;
            }
            if (i == win->scrollback_size) {
                return i;
            }
        }
    }
    while (j < win->width) {
        qc_curr = win->font_color + i * win->width + j;
        set_quad_color(qc_curr, def_r, def_g, def_b);
        fill_glyph(i, j, 0x20, win);
        j++;
    }
    if (j == win->width) {
        i++;
    }
    return i;
}

uint8_t *
next_utf8_char(uint8_t *curr) {
    if ((*curr & 0x80) == 0) {
        curr += 1;
    } else if ((*curr & 0xe0) == 0xc0) {
        curr += 2;
    } else if ((*curr & 0xf0) == 0xe0) {
        curr += 3;
    } else if ((*curr & 0xf8) == 0xf0) {
        curr += 4;
    } else {
        dbg_assert(0);
    }
    return curr;
}

size_t
glyphs_in_utf8_line(uint8_t *curr, uint8_t *end, struct window *win) {
    size_t curr_width = 0;
    size_t win_lines = 0;

    while (curr < end) {
        curr_width++;
        if (*curr == '\t') {
            curr_width += curr_width % TAB_SIZE;
        }
        if (curr_width >= win->width) {
            win_lines++;
            curr_width = 0;
        }
        curr = next_utf8_char(curr);
    }
    return curr_width + win_lines * win->width;
}

uint8_t *
offsetof_utf8_width(uint8_t *curr
    , uint8_t *end
    , struct window *win
    , size_t width
    ) {
    size_t curr_width = 0;
    size_t win_lines = 0;
    size_t i = 0;

    while (curr < end && i < width) {
        curr_width++;
        i++;
        if (*curr == '\t') {
            curr_width += curr_width % TAB_SIZE;
        }
        if (curr_width >= win->width) {
            win_lines++;
            i -= curr_width % win->width;
            curr_width = 0;
        }
        curr = next_utf8_char(curr);
    }
    return curr;
}

uint8_t *
offsetof_width(struct line_metadata *lm
    , struct window *win
    , struct document *doc
    , size_t width) {
    if (is_line_internal(&lm->line, doc)) {
        return offsetof_utf8_width(lm->line.intern_line
            , lm->line.intern_line + lm->line.size
            , win
            , width
        );
    }
    else if (lm->line.extern_line->utf8_status == UTF8_CLEAN) {
        return offsetof_utf8_width(lm->line.extern_line->data
            , lm->line.extern_line->data + lm->line.size
            , win
            , width
        );
    }
    return lm->line.extern_line->data + width;
}

void
recompute_win_lines_metadata(struct line_metadata *lm
    , struct document *doc
    , struct window *win
    ) {
    size_t nglyphs;

    ensure(win->width != 0);

    if (lm->line.size == 0) {
        lm->win_lines = 1;
        return;
    }
    if (is_line_internal(&lm->line, doc)) {
        nglyphs = glyphs_in_utf8_line(lm->line.intern_line
            , lm->line.intern_line + lm->line.size
            , win
        );
        lm->win_lines = nglyphs / win->width + (nglyphs % win->width != 0);
    } else if (lm->line.extern_line->utf8_status == UTF8_CLEAN) {
        nglyphs = glyphs_in_utf8_line(lm->line.extern_line->data
            , lm->line.extern_line->data + lm->line.size
            , win
        );
        lm->win_lines = nglyphs / win->width + (nglyphs % win->width != 0);
    } else {
        nglyphs = lm->line.size;
        lm->win_lines = nglyphs / win->width + (nglyphs % win->width != 0);
    }
    if (lm->win_lines == 0) {
        lm->win_lines = 1;
    }
}

void
fill_screen(struct document **doc, struct window *win) {
    struct document *res = *doc;
    struct line_metadata *lm = res->lines + res->y_line_off;
    struct line_metadata *lm_end = res->lines + res->loaded_size;
    struct line_metadata *lm_curr;
    unsigned i = 0;
    unsigned j;
    enum UTF8_STATUS utf8_status;
    size_t win_lines;
    uint8_t *line_beg;
    uint8_t *line_end;

    if (win->scrollback_size * win->width == 0) {
        return;
    }
    if (lm->win_lines == 0 && lm != lm_end) {
        recompute_win_lines_metadata(lm, *doc, win);
    }
    ensure(lm->win_lines > res->y_window_line_off || lm->win_lines == 0);
    win_lines = lm->win_lines - res->y_window_line_off;

    ensure(lm <= lm_end);

    if (!is_fully_loaded(res)) {
        lm_curr = lm + 1;
        while (lm_curr < lm_end) {
            if (win_lines >= win->scrollback_size) {
                break;
            }
            if (lm_curr->win_lines == 0) {
                recompute_win_lines_metadata(lm_curr, *doc, win);
            }
            win_lines += lm_curr->win_lines;
            lm_curr++;
        }
        if (win_lines < win->scrollback_size) {
            ensure(load_lines(win->scrollback_size - win_lines, doc) == 0);
            res = *doc;
            lm = res->lines + res->y_line_off;
            lm_end = res->lines + res->loaded_size;
        }
    }
    if (is_line_internal(&lm->line, res)) {
        utf8_status = UTF8_CLEAN;
        line_beg = lm->line.intern_line + res->x_off;
        line_end = lm->line.intern_line + lm->line.size;
    } else {
        utf8_status = lm->line.extern_line->utf8_status; 
        line_beg = lm->line.extern_line->data + res->x_off;
        line_end = lm->line.extern_line->data + lm->line.size;
    }
    if (lm == lm_end) {
        goto CLEAR_SCREEN;
    }
    while (1) {
        i = fill_line(i, utf8_status, line_beg, line_end, win);
        if (++lm == lm_end || i == win->scrollback_size) {
            break;
        }
        if (is_line_internal(&lm->line, res)) {
            utf8_status = UTF8_CLEAN;
            line_beg = lm->line.intern_line;
            line_end = lm->line.intern_line + lm->line.size;
        } else {
            utf8_status = lm->line.extern_line->utf8_status; 
            line_beg = lm->line.extern_line->data;
            line_end = lm->line.extern_line->data + lm->line.size;
        }
    }
CLEAR_SCREEN:
    while (i != win->scrollback_size) {
        for (j = 0; j < win->width; j++) {
            fill_glyph(i, j, 0x20, win);
        }
        i++;
    }
}

uint8_t *
begin_line_metadata(struct line_metadata *lm, struct document *doc) {
    if (is_line_internal(&lm->line, doc)) {
        return  lm->line.intern_line;
    }
    return lm->line.extern_line->data;
}

uint8_t *
end_line_metadata(struct line_metadata *lm, struct document *doc) {
    return begin_line_metadata(lm, doc) + lm->line.size;
}

void
move_scrollback_up(struct window *win, struct document **docp) {
    struct document *doc = *docp;
    struct line_metadata *lm = doc->lines + doc->y_line_off;
    uint8_t *x_off_winlines;

    if (win->scrollback_pos > 0) {
        win->scrollback_pos--;
        return;
    }
    if (doc->y_line_off == 0 && doc->y_window_line_off == 0) {
        return;
    }
    win->scrollback_pos = win->scrollback_size;
    while (win->scrollback_pos != win->height) {
        win->scrollback_pos--;
        if (doc->y_window_line_off > 0) {
            doc->y_window_line_off--;
        } else {
            dbg_assert(doc->y_line_off > 0);
            doc->y_line_off--;
            lm--;
            if (lm->win_lines == 0) {
                recompute_win_lines_metadata(lm, doc, win);
            }
            doc->y_window_line_off = lm->win_lines - 1;
        }
        if (doc->y_line_off == 0 && doc->y_window_line_off == 0) {
            break;
        }
    }
    x_off_winlines = offsetof_width(lm, win, doc
        , doc->y_window_line_off * win->width
    );
    doc->x_off = x_off_winlines - begin_line_metadata(lm, doc);

    win->scrollback_pos--;
    fill_screen(docp, win);
}

void
move_scrollback_down(struct window *win, struct document **docp) {
    struct document *doc = *docp;
    struct line_metadata *lm = doc->lines + doc->y_line_off;
    struct line_metadata *lm_end = doc->lines + doc->loaded_size;
    uint8_t *x_off_winlines;
    unsigned i;

    if (win->scrollback_pos + win->height < win->scrollback_size) {
        win->scrollback_pos++;
        return;
    }
    if (!is_fully_loaded(doc) && lm + win->height > lm_end) {
        load_lines(win->height, docp);
        doc = *docp;
        lm = doc->lines + doc->y_line_off;
        lm_end = doc->lines + doc->loaded_size;
    }
    if (lm == lm_end) {
        ensure(doc->y_window_line_off == 0);
        return;
    }
    win->scrollback_pos = 0;
    for (i = 0; i < win->height; i++) {
        if (lm->win_lines == 0) {
            recompute_win_lines_metadata(lm, doc, win);
        }
        if (++doc->y_window_line_off == lm->win_lines) {
            doc->y_window_line_off = 0;
            doc->y_line_off++;
            lm++;
        }
        if (lm == lm_end) {
            break;
        }
    }
    x_off_winlines = offsetof_width(lm, win, doc
        , doc->y_window_line_off * win->width
    );
    doc->x_off = x_off_winlines - begin_line_metadata(lm, doc);

    win->scrollback_pos++;
    fill_screen(docp, win);
}

void
gl_buffers_upload(struct window *win) {
    size_t size = win->height * win->width;

    glBufferSubData(GL_ARRAY_BUFFER
        , sizeof(struct quad_coord) * size
        , sizeof(struct quad_coord) * size
        , win->glyph_mesh + win->scrollback_pos * win->width
    );
    glBufferSubData(GL_ARRAY_BUFFER
        , sizeof(struct quad_coord) * 2 * size
        , sizeof(struct quad_color) * size
        , win->font_color + win->scrollback_pos * win->width
    );
}

void
move_scrollback(struct window *win
    , enum MV_VERT_DIRECTION vdir
    , size_t times
    , struct document **docp
    ) {
    size_t i;

    if (vdir == MV_VERT_UP) {
        for (i = 0; i < times; i++) {
            move_scrollback_up(win, docp);
        }
    } else if (vdir == MV_VERT_DOWN) {
        for (i = 0; i < times; i++) {
            move_scrollback_down(win, docp);
        }
    } else {
        dbg_assert(0);
    }
    gl_buffers_upload(win);
}

struct extern_line *
convert_line_external(struct line *line, struct document *doc) {
    return reserve_line(line, 0, doc);
}

void
diff_line_merge(struct diffstack **ds
    , size_t x
    , size_t y
    , struct document **doc
    ) {
    struct document *res = *doc;
    struct line_metadata *lm_from = res->lines + y;
    struct line_metadata *lm_into = res->lines + y - 1;

    ensure(x == 0 && y > 0);

    dbg_assert(lm_from < res->lines + res->loaded_size);

    diffstack_insert_merge(ds, lm_from, 1, x, y);
    merge_lines(lm_into, lm_from, res);

    free_line(res, &lm_from->line);
    memmove(lm_from
        , lm_from + 1
        , (res->loaded_size + 1) * sizeof(struct line_metadata)
    );
    res->loaded_size--;
    lm_into->win_lines = 0;
}

void
insert_n_line(size_t x, size_t y, size_t n, struct document **doc) {
    struct extern_line* el = NULL;
    struct line_metadata *lm_beg;
    struct line_metadata *lm_rst;
    struct line_metadata *lm_end;
    struct line_metadata *lm_it;
    struct document *res;
    uint8_t *fst_beg;
    uint8_t *fst_end;
    uint8_t *snd_beg;
    uint8_t *snd_end;
    size_t rst_size;

    if (n == 0) {
        return;
    }
    resize_document_by(n, doc);
    res = *doc;

    lm_beg = res->lines + y;
    lm_end = res->lines + res->loaded_size + 1;
    lm_rst = lm_beg + n;

    memmove(lm_rst, lm_beg, (lm_end - lm_beg) * sizeof(*lm_beg));
    for (lm_it = lm_beg + 1; lm_it < lm_rst; lm_it++) {
        memzero(&lm_it->line, 1, sizeof(struct line));
        init_extern_line(&lm_it->line, NULL, 0, UTF8_CLEAN, *doc);
        lm_it->win_lines = 1;

    }
    res->loaded_size += n;

    fst_beg = begin_line_metadata(lm_beg, res);
    fst_end = fst_beg + x;
    snd_beg = fst_end;
    snd_end = fst_beg + lm_beg->line.size;
    rst_size = snd_end - snd_beg;

    ensure(reallocflexarr((void **)&el
            , sizeof(struct extern_line)
            , rst_size
            , sizeof(uint8_t)
    ));
    if (next_utf8_or_null(snd_beg, snd_end) == NULL) {
        el->utf8_status = UTF8_DIRTY;
        convert_line_external(&lm_beg->line, res);
        lm_beg->line.extern_line->utf8_status = UTF8_DIRTY;
    } else {
        el->utf8_status = UTF8_CLEAN;
    }
    memcpy(el->data, snd_beg, rst_size * sizeof(*el->data));
    lm_rst->line.alloc = rst_size;
    lm_rst->line.size = rst_size;
    lm_rst->line.extern_line = el;
    lm_beg->line.size = fst_end - fst_beg;
    lm_beg->win_lines = 0;
    lm_rst->win_lines = 0;
}

void
diff_line_split(struct diffstack **ds
    , size_t x
    , size_t y
    , struct document **doc
    ) {
    insert_n_line(x, y, 1, doc);
    diffstack_insert_split(ds, 1, x, y);
}

void
diff_line_insert(struct diffstack **ds
    , size_t pos
    , struct line_metadata *lm
    , struct document *doc
    , uint8_t *data
    , size_t size
    ) {
    struct line *line = &lm->line;
    uint8_t *data_curr = data;
    uint8_t *data_end = data + size;
    struct extern_line *el = reserve_line(line, size, doc);
    uint8_t *el_curr = el->data + pos;
    uint8_t *el_end = el->data + line->size;

    if (next_utf8_or_null(el_curr, el_end) == NULL) {
        el->utf8_status = UTF8_DIRTY;
    }
    while (data_curr != data_end) {
        data_curr = next_utf8_or_null(data_curr, data_end);
        if (data_curr == NULL) {
            el->utf8_status = UTF8_DIRTY;
            break;
        }
    }
    memmove(el_curr + size, el_curr, line->size * sizeof(*el_curr));
    memcpy(el_curr, data, size);
    line->size += size;
    lm->win_lines = 0;
    diffstack_insert_chars_add(ds, data, size, pos, lm - doc->lines);
}

void
diff_line_remove(struct diffstack **ds
    , size_t pos
    , struct line_metadata *lm
    , struct document *doc
    , size_t size
    ) {
    struct line *line = &lm->line;
    struct extern_line *el = convert_line_external(line, doc);
    uint8_t *el_curr = el->data + pos;
    uint8_t *el_end = el->data + line->size;
    uint8_t *data_curr = el_curr;
    uint8_t *data_end = el_curr + size;
    size_t rhs_size = el_end - el_curr;

    ensure(line->size >= size);
    ensure(pos <= line->size && pos + size <= line->size);

    if (next_utf8_or_null(data_curr, el_end) == NULL) {
        el->utf8_status = UTF8_DIRTY;
    }
    if (next_utf8_or_null(data_end, el_end) == NULL) {
        el->utf8_status = UTF8_DIRTY;
    }
    diffstack_insert_chars_del(ds, el_curr, size, pos, lm - doc->lines);
    memmove(data_curr, data_end, rhs_size);
    line->size -= size;
    lm->win_lines = 0;
}

int
main(int argc, char *argv[]) {
    char *fname = "se.c";

    if (argc - 1 > 0) {
        fname = argv[1];
    }
    xensure(init_diffstack(&diff, 0x1000) == 0);
    xensure(init_doc(fname, &doc) == 0);
    xensure(win_init(argc, argv) == 0);
    xensure(gl_pipeline_init() == 0);

    glutMainLoop();
    return 0;
}
