// This is free and unencumbered software released into the public domain.
// For more information, see LICENSE.

#include <GL/glew.h>
#include <GL/gl.h>
#include <SDL2/SDL.h>

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
#include <signal.h>

#include "util.h"
#include "fio.h"
#include "lex.h"
#include "comp.h"
#include "se.h"
#include "ilog.h"

#include "se.gen.h"
#include "umap.gen.h"

#include "color.def.h"
#include "class.def.h"

#define D_GLYPH     (1.0f / 256)
#define H_GLYPH_PX  (12)
#define V_GLYPH_PX  (16)

extern char *__progname;
extern struct ilog *ilogptr;
extern int ilog_enable;

static struct color *color_selection = colors_table + 7;
static struct color *color_cursor    = colors_table + 8;
static struct color *color_focus     = colors_table + 9;
static struct color *color_default   = colors_table;
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
static const char *fs_src  = "                  \n\
#version 130                                    \n\
in vec4 color;                                  \n\
in vec2 uv;                                     \n\
out vec4 fColor;                                \n\
uniform sampler2D texture;                      \n\
void main() {                                   \n\
    vec4 bg = vec4(0.152, 0.156, 0.13, 1.0);    \n\
    fColor = texture2D(texture, uv);            \n\
    if (color.r <= 1.0) {                       \n\
        fColor = fColor.r * color + bg;         \n\
    } else {                                    \n\
        fColor = (color - 1.0 - fColor.r);      \n\
    }                                           \n\
}                                               \n\
";

#include "diff.c"
#include "input.c"
#include "conf.c"

uint32_t
first_glyph(uint8_t *beg, uint8_t *end) {
    if (next_utf8_or_null(beg, end) != NULL) {
        return iter_glyph_from_utf8(&beg);
    }
    return *beg;
}

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

void
window_render(struct window *win, struct gl_data *gl_id) {
    unsigned size = window_area(win);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6 * size);
    SDL_GL_SwapWindow(gl_id->window);
}

void
resize_display_matrix(struct editor *ed
    , struct gl_data *gl_id
    , int win_width_px
    , int win_height_px
    ) {
    unsigned win_width = win_width_px / H_GLYPH_PX;
    unsigned win_height = win_height_px / V_GLYPH_PX;
    unsigned size = win_width * win_height;

    ed->win = gen_display_matrix(&ed->win, win_width, win_height);
    ensure(ed->win);

    ed->win->dmg_scrollback_beg = 0;
    ed->win->dmg_scrollback_end = ~0;

    ed->win->scrollback_pos = 0;
    ed->doc->line_off = 0;
    ed->doc->glyph_off = 0;
    fill_screen_glyphs(ed, 0);
    screen_reposition(ed);
    fill_screen_colors(ed,  0);

    glBufferData(GL_ARRAY_BUFFER
        , (sizeof(struct quad_coord) * 2 + sizeof(struct quad_color)) * size
        , NULL
        , GL_DYNAMIC_DRAW
    );
    glBufferSubData(GL_ARRAY_BUFFER
        , 0
        , sizeof(struct quad_coord) * size
        , ed->win->window_mesh
    );
    gl_buffers_upload(ed->win);
    glVertexAttribPointer(gl_id->pos, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glVertexAttribPointer(gl_id->uv, 2, GL_FLOAT, GL_FALSE, 0
        , (void *)(sizeof(struct quad_coord) * size)
    );
    glVertexAttribPointer(gl_id->col, 3, GL_FLOAT, GL_FALSE, 0
        , (void *)(sizeof(struct quad_coord) * 2 * size)
    );
}

void
window_resize(struct editor *ed
    , struct gl_data *gl_id
    , int win_width_px
    , int win_height_px
    ) {
    glViewport(0, 0, win_width_px, win_height_px);
    resize_display_matrix(ed, gl_id, win_width_px, win_height_px);
}

struct selectarr *
reserve_selectarr(struct selectarr **selv, size_t size) {
    struct selectarr *res = *selv;
    size_t focus_pos;
    size_t alloc;

    if (res->size + size > res->alloc) {
        focus_pos = res->focus - res->data;
        alloc = res->size + res->size / 2 + size;
        res = reallocflexarr((void **)selv
            , sizeof(struct selectarr)
            , alloc
            , sizeof(struct selection)
        );
        ensure(res);
        res->alloc = alloc;
        res->focus = res->data + focus_pos;
    }
    return res;
}

void
screen_reposition(struct editor *ed) {
    struct document *doc = ed->doc;
    struct window *win = ed->win;
    size_t cursor_line = ed->selv->focus->line;
    size_t cursor_glyph = ed->selv->focus->glyph_end;
    size_t win_lines;

    if (window_area(win) == 0) {
        return;
    }
    if (cursor_glyph >= doc->glyph_off
        && cursor_glyph < doc->glyph_off + win->width
    ) {
        if (cursor_line >= doc->line_off + win->scrollback_pos
            && cursor_line < doc->line_off + win->scrollback_pos + win->height
        ) {
            return;
        } else if (cursor_line < doc->line_off + win->scrollback_pos) {
            win_lines = doc->line_off + win->scrollback_pos - cursor_line;
            move_scrollback(ed, MV_VERT_UP, win_lines);
        } else {
            dbg_assert(cursor_line
                >= doc->line_off + win->scrollback_pos + win->height
            );
            win_lines = cursor_line + 1 - doc->line_off - win->scrollback_pos
                - win->height;
            move_scrollback(ed, MV_VERT_DOWN, win_lines);
        }
    } else {
        win_dmg_from_lineno(ed->win, 0);

        if (doc->glyph_off > cursor_glyph) {
            while (doc->glyph_off > cursor_glyph) {
                doc->glyph_off -= win->width / 2;
            }
        } else {
            dbg_assert(cursor_glyph >= doc->glyph_off + win->width);
            while (doc->glyph_off + win->width <= cursor_glyph) {
                doc->glyph_off += win->width / 2;
            }
        }
        if (cursor_line < doc->line_off + win->scrollback_pos
            || cursor_line >= doc->line_off + win->scrollback_pos + win->height
        ) {
            win->scrollback_pos = 0;
            doc->line_off = cursor_line;
        }
    }
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
window_init(struct gl_data *gl_id, int argc, char **argv) {
    static char window_title[128];
    int err;

    str_intercalate(window_title, sizeof(window_title) - 1, argv, argc, ' '); 

    err = SDL_Init(SDL_INIT_VIDEO);
    if (err != 0) {
        errx(1, "SDL_Init: %s", SDL_GetError());
    }
    gl_id->window = SDL_CreateWindow(window_title
        , SDL_WINDOWPOS_UNDEFINED
        , SDL_WINDOWPOS_UNDEFINED
        , 0
        , 0
        , SDL_WINDOW_OPENGL | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_RESIZABLE
    );
    if (gl_id->window == NULL) {
        errx(1, "SDL_CreateWindow: %s", SDL_GetError());
    }
    gl_id->glcontext = SDL_GL_CreateContext(gl_id->window);
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
set_quad_color(struct quad_color *q, struct color *color) {
    int i;

    for (i = 0; i < 6; i++) {
        q->vertex_color[i] = *color;
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

struct window*
gen_display_matrix(struct window **win, unsigned width, unsigned height) {
    size_t scrollback = 2 * height;
    size_t window_area = width * height;
    size_t scrollback_area = width * scrollback;
    size_t window_mesh_sz = sizeof(struct quad_coord) * window_area;
    size_t glyph_mesh_sz  = sizeof(struct quad_coord) * scrollback_area;
    size_t font_color_sz  = sizeof(struct quad_color) * scrollback_area;
    size_t win_mem_sz =  window_mesh_sz + glyph_mesh_sz + font_color_sz;
    struct window *res = reallocflexarr((void **)win
        , sizeof(struct window)
        , win_mem_sz
        , 1
    );
    ensure(res);

    res->width = width;
    res->height = height;
    res->scrollback_size = scrollback;
    res->scrollback_pos = 0;
    res->window_mesh = (struct quad_coord*)res->data;
    res->glyph_mesh = res->window_mesh + window_area;
    res->font_color = (void *)(res->glyph_mesh + scrollback_area);
    fill_window_mesh(res, width, height);
    memzero(res->glyph_mesh, 1, glyph_mesh_sz + font_color_sz);
    return res;
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
gl_pipeline_init(struct editor *ed, struct gl_data *gl_id) {
    unsigned win_width_px = 0;
    unsigned win_height_px = 0;
    void *font_data = malloc(0x1440000);
    struct mmap_file file;
    struct rfp_file *rfp;

    xensure(font_data);
    xensure(load_font("unifont.cfp", &file) == 0);
    rfp = load_cfp_file(&file);
    rfp_decompress(rfp, font_data);
    gl_id->prog = gl_compile_program(vs_src, fs_src);
    glGenVertexArrays(1, &gl_id->vao);
    glBindVertexArray(gl_id->vao);
    glGenBuffers(1, &gl_id->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, gl_id->vbo);
    gl_id->pos = glGetAttribLocation(gl_id->prog, "s_pos");
    gl_id->uv = glGetAttribLocation(gl_id->prog, "s_uv");
    gl_id->col = glGetAttribLocation(gl_id->prog, "s_color");
    xensure(glGetError() == GL_NO_ERROR);

    memzero(ed->win, 1, sizeof(struct window));
    resize_display_matrix(ed, gl_id, win_width_px, win_height_px);

    glEnableVertexAttribArray(gl_id->pos);
    glEnableVertexAttribArray(gl_id->col);
    glEnableVertexAttribArray(gl_id->uv);

    glEnable(GL_TEXTURE_2D);
    glGenTextures(1, &gl_id->tbo);
    glBindTexture(GL_TEXTURE_2D, gl_id->tbo);
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
    gl_id->tex = glGetUniformLocation(gl_id->prog, "texture");
    glUniform1i(gl_id->tex, 0);

    glUseProgram(gl_id->prog);
    return 0;
}

void
free_line(struct line *line, struct document *doc) {
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
merge_lines(struct line *fst, struct line *snd, struct document *doc) {
    size_t s_size = snd->size;
    size_t f_size = fst->size;
    struct extern_line *el = reserve_line(fst, s_size, doc);

    if (!is_line_internal(snd, doc)) {
        el->utf8_status |= snd->extern_line->utf8_status;
    }
    memcpy(el->data + f_size, begin_line(snd, doc), s_size);
    fst->size += s_size;
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
load_line(struct line *line
    , uint8_t *beg
    , uint8_t *end
    , struct document *doc
    ) {
    uint8_t *curr = beg;
    size_t line_size;

    ensure(line->size == 0);
    ensure(line->alloc == 0);

    do {
        if (curr == end) {
            line->size = curr - beg;
            line->intern_line = beg;
            return curr;
        }
        if (*curr == '\n') {
            line->size = curr - beg;
            line->intern_line = beg;
            return curr + 1;
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
    line->ptr = NULL;
    init_extern_line(line, beg, line_size, UTF8_DIRTY, doc);

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
    struct line *last_line = doc->lines + doc->loaded_size;
    uint8_t *doc_end = doc->file.data + doc->file.size;
    if (doc->loaded_size == 0) {
        return 0;
    }
    return last_line->intern_line == doc_end;
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

struct document *
load_lines(struct editor *ed, size_t nlines) {
    struct document *res = ed->doc;
    struct line *trampoline = res->lines + res->loaded_size;
    uint8_t *doc_beg = res->file.data;
    uint8_t *doc_end = doc_beg + res->file.size;
    uint8_t *to_load;
    struct line *end;

    dbg_assert(trampoline->size == 0
        && trampoline->alloc == 0
        && trampoline->intern_line >= doc_beg
        && trampoline->intern_line <= doc_end
    );
    if (res->file.size == 0 && res->loaded_size == 0) {
        res = resize_document_by(1, &ed->doc);
        ensure(res);
        res->lines->ptr = NULL;
        init_extern_line(&res->lines[0], NULL, 0, UTF8_CLEAN, res);
        res->loaded_size++;
        trampoline = res->lines + 1;
        trampoline->ptr = doc_end;
        return res;
    }
    if (trampoline->ptr == doc_end) {
        return res;
    }
    ensure(resize_document_by(nlines, &ed->doc) != NULL);
    res = ed->doc;
    trampoline = res->lines + res->loaded_size;
    doc_end = res->file.data + res->file.size;

    to_load = trampoline->intern_line;
    end = trampoline + nlines;
    while (trampoline != end && to_load != doc_end) {
        to_load = load_line(trampoline, to_load, doc_end, ed->doc);
        trampoline++;
    }
    dbg_assert(trampoline == res->lines + res->loaded_size + nlines
        || to_load == doc_end);
    dbg_assert(to_load <= doc_end);
    trampoline->ptr = to_load;
    res->loaded_size = trampoline - res->lines;

    if (trampoline->ptr == doc_end) {
        res->alloc = res->loaded_size + 1;
        ensure(reallocflexarr((void **)&ed->doc
                , sizeof(struct document)
                , res->alloc
                , sizeof(struct line)
        ));
        res = ed->doc;
    }
    return res;
}

uint32_t
glyph_from_utf8(uint8_t *curr) {

    if ((*curr & 0x80) == 0x00) {
        return 0u
            | curr[0]
            ;
    }
    if ((*curr & 0xe0) == 0xc0) {
        return 0u
            | ((curr[0] & 0x1f) << 0x06)
            | ((curr[1] & 0x3f) << 0x00)
            ;
    }
    if ((*curr & 0xf0) == 0xe0) {
        return 0u
            | ((curr[0] & 0x0f) << 0x0c)
            | ((curr[1] & 0x3f) << 0x06)
            | ((curr[2] & 0x3f) << 0x00)
            ;
    }
    if ((*curr & 0xf8) == 0xf0) {
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

uint32_t
iter_glyph_from_utf8(uint8_t **line) {
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

struct document *
resize_document_by(size_t size, struct document **doc) {
    struct document *res = *doc;
    size_t alloc = res->alloc;
    size_t incr_size = res->loaded_size + 1 + size;
    struct line *line;

    dbg_assert(res->alloc > res->loaded_size && size > 0);

    if (incr_size > alloc) {
        alloc = incr_size + size / 2;
        res = reallocflexarr((void **)doc
            , sizeof(struct document)
            , alloc
            , sizeof(struct line)
        );
        if (res == NULL) {
            return res;
        }
        res->alloc = alloc;
    }
    line = res->lines + res->loaded_size + 1;
    memzero(line, size, sizeof(struct line));

    dbg_assert(res->alloc > res->loaded_size);
    return res;
}

int
init_sel(size_t alloc, struct selectarr **selv) {
    struct selectarr *res;
    struct selection *sel;

    ensure(alloc > 0);
    ensure(reallocflexarr((void **)selv
        , sizeof(struct selectarr)
        , alloc
        , sizeof(struct selection)
    ) != NULL);
    res = *selv;
    sel = res->data;
    sel->line = 0;
    sel->glyph_beg = 0;
    sel->glyph_end = 0;
    res->focus = res->data;
    res->alloc = alloc;
    res->size = 1;
    return 0;
}

int
init_doc(const char *fname, struct document **doc) {
    int fd = open(fname, O_RDONLY);
    struct mmap_file file;
    struct document *res;
    struct line *fst_line;
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
        , sizeof(struct line)
    );
    ensure(res != NULL);
    res->file = file;
    res->alloc = alloc;
    res->loaded_size = 0;
    res->line_off = 0;
    fst_line = res->lines;
    fst_line->size = 0;
    fst_line->alloc = 0;
    fst_line->intern_line = file.data;
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

void
fill_glyph(unsigned i, unsigned j, uint32_t glyph, struct window *win) {
    if (glyph < 0x10000) {
        if (!is_glyph_wide(glyph)) {
            fill_glyph_narrow(i, j, glyph, win);
        } else {
            fill_glyph_wide(i, j, glyph, win);
        }
    } else {
        fill_glyph_wide(i, j, 0xffff, win);
    }
}

void
fill_line_glyphs(unsigned i
    , int utf8
    , uint8_t *line_beg
    , uint8_t *line_end
    , unsigned from_glyph
    , struct window *win
    ) {
    uint8_t *line_curr = line_beg;
    unsigned num_glyph = 0;
    unsigned j = 0;
    uint32_t glyph;

    if (utf8) {
        while (line_curr != line_end && j != win->width) {
            glyph = iter_glyph_from_utf8(&line_curr);
            dbg_assert(line_curr <= line_end);
            if (num_glyph++ < from_glyph) {
                continue;
            }
            fill_glyph(i, j, glyph, win);
            j++;
        }
    } else {
        line_curr = min(line_curr + from_glyph, line_end);
        while (line_curr != line_end && j != win->width) {
            fill_glyph(i, j, *line_curr++, win);
            j++;
        }
    }
    while (j < win->width) {
        fill_glyph(i, j, 0x20, win);
        j++;
    }
}

void
fill_line_colors(unsigned i
    , int utf8
    , uint8_t *line_beg
    , uint8_t *line_end
    , unsigned from_glyph
    , struct window *win
    ) {
    uint8_t *line_curr = line_beg;
    unsigned j = 0;
    unsigned num_glyph = 0;

    struct colored_span cw = { .color_pos = 0, .size = 0 };
    struct quad_color *qc_curr;

    if (utf8) {
        while (line_curr != line_end && j != win->width) {
            if (cw.size == 0) {
                cw = color_span(line_beg, line_curr, line_end);
            }
            qc_curr = win->font_color + i * win->width + j;
            set_quad_color(qc_curr, colors_table + cw.color_pos);
            line_curr = next_utf8_char(line_curr);
            dbg_assert(line_curr <= line_end);
            if (num_glyph++ < from_glyph) {
                continue;
            }
            j++;
            cw.size--;
        }
    } else {
        line_curr = min(line_curr + from_glyph, line_end);
        while (line_curr != line_end && j != win->width) {
            qc_curr = win->font_color + i * win->width + j;
            set_quad_color(qc_curr, color_default);
            j++;
        }
    }
    while (j < win->width) {
        qc_curr = win->font_color + i * win->width + j;
        set_quad_color(qc_curr, color_default);
        j++;
    }
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

uint8_t *
prev_utf8_char(uint8_t *curr) {
    uint8_t *end = curr;

    do{
        curr--;
    } while ((*curr & 0xc0) == 0x80);
    dbg_assert(end - curr <= 4);
    return curr;
}

size_t
glyphs_in_utf8_span(uint8_t *curr, uint8_t *end) {
    size_t curr_width;

    for (curr_width = 0; curr < end; curr_width++) {
        curr = next_utf8_char(curr);
    }
    return curr_width;
}

size_t
glyphs_in_line(struct line *line, struct document *doc) {
    uint8_t *beg = begin_line(line, doc);
    uint8_t *end = end_line(line, doc);

    if (is_line_utf8(line, doc)) {
        return glyphs_in_utf8_span(beg, end);
    } else {
        return end - beg;
    }
}

uint8_t *
sync_utf8_width_or_null(uint8_t *curr, uint8_t *end, size_t width) {
    unsigned i;

    for (i = 0; curr < end && i < width; i++) {
        curr = next_utf8_char(curr);
    }
    if (i != width) {
        return NULL;
    }
    return curr;
}

uint8_t *
sync_width_or_null(struct line *line, size_t width, struct document *doc) {

    if (is_line_utf8(line, doc)) {
        return sync_utf8_width_or_null(begin_line(line, doc)
            , end_line(line, doc)
            , width
        );
    } else {
        return width <= line->size ? line->extern_line->data + width : NULL;
    }
}

void
fill_screen(struct editor *ed, unsigned i) {
    fill_screen_glyphs(ed, i);
    fill_screen_colors(ed, i);
}

void
fill_screen_glyphs(struct editor *ed, unsigned i) {
    struct document *doc = ed->doc;
    struct window *win = ed->win;
    struct line *line_beg = doc->lines + doc->line_off;
    struct line *line_end = doc->lines + doc->loaded_size;
    size_t win_lines = doc->loaded_size - doc->line_off;
    unsigned j;

    ensure(line_beg <= line_end);

    if (win->scrollback_size * win->width == 0) {
        return;
    }
    if (win_lines < win->scrollback_size && !is_fully_loaded(doc)) {
        doc = load_lines(ed, win->scrollback_size - win_lines);
        line_beg = doc->lines + doc->line_off;
        line_end = doc->lines + doc->loaded_size;
        win_lines = doc->loaded_size - doc->line_off;
    }
    for (; i < min(win_lines, win->scrollback_size); i++) {
        if (doc->line_off + i < ed->win->dmg_scrollback_beg
            || doc->line_off + i > ed->win->dmg_scrollback_end
        ) {
            continue;
        }
        fill_line_glyphs(i
            , is_line_utf8(line_beg + i, doc)
            , begin_line(line_beg + i, doc)
            , end_line(line_beg + i, doc)
            , doc->glyph_off
            , win
        );
    }
    while (i < win->scrollback_size) {
        for (j = 0; j < win->width; j++) {
            fill_glyph(i, j, 0x20, win);
        }
        i++;
    }
}

void
fill_screen_colors(struct editor *ed, unsigned line) {
    struct document *doc = ed->doc;
    struct window *win = ed->win;
    struct line *line_beg = doc->lines + doc->line_off;
    struct line *line_end = doc->lines + doc->loaded_size;
    size_t win_lines = doc->loaded_size - doc->line_off;
    struct selection *sel_beg = ed->selv->data;
    struct selection *sel_end = ed->selv->data + ed->selv->size;
    struct quad_color *qc_curr;
    unsigned ic;
    unsigned jc;
    unsigned i;
    unsigned j;

    ensure(line_beg <= line_end);

    if (win->scrollback_size * win->width == 0) {
        return;
    }
    dbg_assert(!(win_lines < win->scrollback_size && !is_fully_loaded(doc)));

    for (i = line; i < min(win_lines, win->scrollback_size); i++) {
        if (doc->line_off + i < ed->win->dmg_scrollback_beg
            || doc->line_off + i > ed->win->dmg_scrollback_end
        ) {
            continue;
        }
        fill_line_colors(i
            , is_line_utf8(line_beg + i, doc)
            , begin_line(line_beg + i, doc)
            , end_line(line_beg + i, doc)
            , doc->glyph_off
            , win
        );
    }
    for (; i < win->scrollback_size; i++) {
        for (j = 0; j < win->width; j++) {
            qc_curr = win->font_color + i * win->width + j;
            set_quad_color(qc_curr, color_default);
        }
    }
    while (sel_beg != sel_end && sel_beg->line < doc->line_off) {
        sel_beg++;
    }
    while (sel_beg != sel_end
        && sel_beg->line < doc->line_off + win->scrollback_size
    ) {
        ic = sel_beg->line - doc->line_off;
        for (jc = sel_beg->glyph_beg; jc < sel_beg->glyph_end; jc++) {
            qc_curr = win->font_color + ic * win->width + jc - doc->glyph_off;
            if (jc >= doc->glyph_off && jc < doc->glyph_off + win->width){
                set_quad_color(qc_curr, color_selection);
            }
        }
        qc_curr = win->font_color + ic * win->width + jc - doc->glyph_off;
        if (jc >= doc->glyph_off && jc < doc->glyph_off + win->width){
            set_quad_color(qc_curr, sel_beg == ed->selv->focus
                ? color_focus
                : color_cursor
            );
        }
        sel_beg++;
    }
}

uint8_t *
begin_line(struct line *line, struct document *doc) {
    if (is_line_internal(line, doc)) {
        return  line->intern_line;
    }
    return line->extern_line->data;
}

uint8_t *
end_line(struct line *line, struct document *doc) {
    return begin_line(line, doc) + line->size;
}

size_t
window_area(struct window *win) {
    return win->height * win->width;
}

void
move_scrollback_up(struct editor *ed) {
    struct document *doc = ed->doc;
    struct window *win = ed->win;
    size_t size = window_area(win);

    if (win->scrollback_pos > 0) {
        win->scrollback_pos--;
        return;
    }
    if (doc->line_off == 0) {
        return;
    }
    dbg_assert(doc->line_off >= win->height);

    doc->line_off -= win->height;
    win->scrollback_pos = win->height - 1;

    memcpy(win->glyph_mesh + size
        , win->glyph_mesh
        , size * sizeof(struct quad_coord)
    );
    win_dmg_from_lineno(ed->win, 0);
}

void
move_scrollback_down(struct editor *ed) {
    struct document *doc = ed->doc;
    struct window *win = ed->win;
    struct line *line = doc->lines + doc->line_off;
    struct line *line_end = doc->lines + doc->loaded_size;
    size_t size = window_area(win);

    if (win->scrollback_pos + win->height < win->scrollback_size) {
        win->scrollback_pos++;
        return;
    }
    if (!is_fully_loaded(doc) && line_end - line > win->height) {
        doc = load_lines(ed, line_end - line);
        line = doc->lines + doc->line_off;
        line_end = doc->lines + doc->loaded_size;
    }
    if (line + win->height > line_end) {
        return;
    }
    win->scrollback_pos = 1;
    doc->line_off += win->height;

    memcpy(win->glyph_mesh
        , win->glyph_mesh + size
        , size * sizeof(struct quad_coord)
    );
    win_dmg_from_lineno(ed->win, 0);
}

void
gl_buffers_upload(struct window *win) {
    size_t size = window_area(win);

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
move_scrollback(struct editor *ed, enum MV_VERT_DIRECTION vdir, size_t times) {
    size_t i;

    if (vdir == MV_VERT_UP) {
        for (i = 0; i < times; i++) {
            move_scrollback_up(ed);
        }
    } else if (vdir == MV_VERT_DOWN) {
        for (i = 0; i < times; i++) {
            move_scrollback_down(ed);
        }
    } else {
        dbg_assert(0);
    }
}

struct extern_line *
convert_line_external(struct line *line, struct document *doc) {
    return reserve_line(line, 0, doc);
}

struct document *
insert_empty_lines(size_t y, size_t n, struct document **doc) {
    struct document *res = resize_document_by(n, doc);
    struct line *beg;
    struct line *end;
    struct line *dst;

    if (n == 0) {
        return res;
    }
    dbg_assert(y <= res->loaded_size);

    beg = res->lines + y;
    end = res->lines + res->loaded_size + 1;
    dst = beg + n;

    memmove(dst, beg, (end - beg) * sizeof(struct line));
    memzero(beg, dst - beg, sizeof(struct line));
    for (; beg != dst; beg++) {
        init_extern_line(beg, NULL, 0, UTF8_CLEAN, res);
    }
    res->loaded_size += n;
    return res;
}

void
copy_extern_line(struct line *line
    , uint8_t *src
    , size_t size
    , struct document *doc
    ) {

    ensure(!is_line_internal(line, doc));

    reserve_extern_line(line, size, doc);
    memcpy(line->extern_line->data, src, size); 
    line->size += size;
}

void
insert_n_line(size_t x, size_t y, size_t n, struct document **doc) {
    struct document *res = *doc;
    struct line *beg;
    struct line *rst;
    uint8_t *fst_beg;
    uint8_t *fst_end;
    uint8_t *snd_beg;
    uint8_t *snd_end;
    size_t rst_size;

    if (n == 0) {
        return;
    }
    if (y == DIFF_ADD_EOD) {
        res = insert_empty_lines(res->loaded_size, n, doc);
    } else {
        res = insert_empty_lines(y + 1, n, doc);

        beg = res->lines + y;
        rst = beg + n;

        fst_beg = begin_line(beg, res);
        fst_end = fst_beg + x;
        snd_beg = fst_end;
        snd_end = fst_beg + beg->size;
        rst_size = snd_end - snd_beg;

        copy_extern_line(rst, snd_beg, rst_size, res);

        if (!is_line_utf8(beg, res)
            || next_utf8_or_null(snd_beg, snd_end) == NULL) {
            convert_line_external(beg, res);
            beg->extern_line->utf8_status = UTF8_DIRTY;
            rst->extern_line->utf8_status = UTF8_DIRTY;
        }
        beg->size = fst_end - fst_beg;
    }
}

void
win_dmg_from_lineno(struct window *win, size_t lineno) {
    // lineno is absolute line, so 0 will invalidate all the srollback
    // win_dmg_from_lineno(ed->doc->line_off) does the same thing since that's
    // where the scrollback buffer begins storing lines from
    win->dmg_scrollback_beg = lineno;
    win->dmg_scrollback_end = ~0;
}

void
win_dmg_calc(struct window *win, struct selectarr *selv) {
    win->dmg_scrollback_beg = ~0;
    win->dmg_scrollback_end =  0;

    for (struct selection *s = selv->data; s < selv->data + selv->size; s++) {
        win->dmg_scrollback_beg = min(s->line, win->dmg_scrollback_beg);
        win->dmg_scrollback_end = max(s->line, win->dmg_scrollback_end);
    }
}

void
render_loop(struct editor *ed, struct gl_data *gl_id) {
    int key;
    int mod;
    SDL_Event event;
    struct line *trampoline;

    window_render(ed->win, gl_id);

    while (SDL_WaitEvent(&event)) {
        dbg(ilog_push(&event));

        win_dmg_calc(ed->win, ed->selv);

        key = event.key.keysym.sym;
        mod = event.key.keysym.mod;
        switch(event.type) {
            case SDL_KEYDOWN:
                if (mod & KMOD_LCTRL) {
                    switch (key) {
                        case 'u':
                            diffstack_undo(ed);
                            break;
                        case 'r':
                            diffstack_redo(ed);
                            break;
                        case 'q':
                            SDL_Quit();
                            exit(0);
                            break;
                    }

                }
                switch (key) {
                    case SDLK_LCTRL:    case SDLK_RCTRL:    case SDLK_LSHIFT:
                    case SDLK_RSHIFT:   case SDLK_LALT:     case SDLK_RALT:
                        break;
                    case SDLK_UP:       case SDLK_DOWN:     case SDLK_RIGHT:
                    case SDLK_LEFT:     case SDLK_PAGEUP:   case SDLK_PAGEDOWN:
                        key_move_input(ed, key, mod);
                        break;
                    case SDLK_DELETE:   case SDLK_ESCAPE:
                        break;
                    case SDLK_RETURN:
                        key_return(ed);
                        break;
                    case SDLK_BACKSPACE:
                        key_backspace(ed);
                        break;
                    default:
                        key_insert_chars(ed, key, mod);
                        break;
                }
                break;
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    window_resize(ed
                        , gl_id
                        , event.window.data1
                        , event.window.data2
                    );
                }
                break;
        }
        trampoline = ed->doc->lines + ed->doc->loaded_size;
        dbg_assert(trampoline->ptr
            <= ed->doc->file.data + ed->doc->file.size
        );
        cursors_reposition(ed->selv, ed->doc);
        screen_reposition(ed);
        fill_screen(ed, 0);
        gl_buffers_upload(ed->win);
        window_render(ed->win, gl_id);
    }
}

void
cursors_reposition(struct selectarr *selv, struct document *doc) {
    if (selv->size == 1) {
        selv->data->line = min(selv->data->line, doc->loaded_size - 1);
        return;
    }
}

int
init_editor(struct editor *ed, const char *fname) {
    struct conf_file conf_file;

    init_conf_file(&conf_file);
    xensure(init_diffstack(&ed->diff, 0x1000) == 0);
    xensure(init_doc(fname, &ed->doc) == 0);
    xensure(init_sel(0x10, &ed->selv) == 0);
    ed->win = malloc(sizeof(struct window));
    ensure(ed->win);

    struct conf_data *dump_on_exit = conf_find(&conf_file, "dump_on_exit");
    if (dump_on_exit) {
        ed->conf_params.dump_on_exit = strcmp(dump_on_exit->val, "1") == 0;
        ilog_enable = ed->conf_params.dump_on_exit;
    }
    struct conf_data *delete_indent = conf_find(&conf_file, "delete_indent");
    if (delete_indent) {
        ed->conf_params.delete_indent = strcmp(delete_indent->val, "1") == 0;
    }
    struct sigaction sa = { .sa_handler = ilog_dump_sig, };
    sigaction(SIGUSR1, &sa, NULL);
    return 0;
}

int
main(int argc, char *argv[]) {
    char *fname = "se.c";
    static struct gl_data gl_id;
    static struct editor ed;

    if (argc - 1 > 0) {
        fname = argv[1];
    }
    xensure(init_editor(&ed, fname) == 0);
    xensure(window_init(&gl_id, argc, argv) == 0);
    xensure(gl_pipeline_init(&ed, &gl_id) == 0);
    render_loop(&ed, &gl_id);
    return 0;
}
