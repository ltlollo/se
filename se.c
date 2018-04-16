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

#include "util.h"
#include "umap.h"
#include "fio.h"

#define D_GLYPH     (1.0f / 256)
#define H_GLYPH_PX  (12)
#define V_GLYPH_PX  (16)
#define TAB_SIZE    (8)

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
    struct extern_line *must_leak;
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
    DIFF_CHARS_ADD  = 0,
    DIFF_CHARS_DEL  = 1,
    DIFF_LINE_SPLIT = 2,
    DIFF_LINE_MERGE = 3,
    DIFF_AGGREGATE  = 4,
};

enum DIFF_DELIM {
    DIFF_CHAR_SEQ = '\n',
    DIFF_AGGR_SEQ = '\0',
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

void
diffstack_insert_chars_add(struct diffstack **ds
    , uint8_t *str
    , size_t size
    , size_t x
    , size_t y
    ) {
    size_t empty_diff = 3 + sizeof(size_t) * 2;
    struct diffstack *res = *ds;
    uint8_t *seq_new_beg = str;
    uint8_t *seq_new_end = str + size;
    uint8_t *seq_old_beg;
    uint8_t *seq_old_end;
    size_t alloc;
    uint32_t glyph_old;
    uint32_t glyph_new;
    size_t old_x;
    size_t old_y;

    if (size == 0) {
        return;
    }
    if (res->curr_checkpoint_end + size + empty_diff >= res->alloc) {
        alloc = res->alloc * 2 + size + empty_diff;
        ensure(reallocflexarr((void **) ds
            , sizeof(struct diffstack)
            , sizeof(uint8_t)
            , alloc
        ));
        res = *ds;
        res->alloc = alloc;
    }
    if (res->curr_checkpoint_beg != res->curr_checkpoint_end
        && res->data[res->curr_checkpoint_beg] == DIFF_CHARS_ADD
    ) {
        seq_old_beg = res->data + res->curr_checkpoint_beg + 2
            + sizeof(size_t) * 2;
        seq_old_end = res->data + res->curr_checkpoint_end - 1;
        memcpy(&old_x, res->data + res->curr_checkpoint_beg + 1
            , sizeof(size_t)
        );
        memcpy(&old_y, res->data + res->curr_checkpoint_beg + 1
            + sizeof(size_t)
            , sizeof(size_t)
        );
        dbg_assert(seq_old_end > seq_old_beg);
        dbg_assert(seq_old_end - seq_old_beg != 0);
        if (old_y != y || old_x + (seq_old_end - seq_old_beg) != x) {
            goto DIFFERENT_DIFF_CLASS;
        }
        glyph_old = first_glyph(seq_old_beg, seq_old_end);
        glyph_new = first_glyph(seq_new_beg, seq_new_end);

        if (!is_same_class(glyph_old, glyph_new)) {
            goto DIFFERENT_DIFF_CLASS;    
        }
        dbg_assert(seq_old_end[0] == DIFF_CHAR_SEQ);
        memcpy(seq_old_end, str, size);
        seq_old_end[size] = DIFF_CHAR_SEQ;
        res->curr_checkpoint_end += size;
    } else {
DIFFERENT_DIFF_CLASS:
        res->curr_checkpoint_beg = res->curr_checkpoint_end;
        res->curr_checkpoint_end = res->curr_checkpoint_beg + empty_diff
            + size;
        res->data[res->curr_checkpoint_beg] = DIFF_CHARS_ADD;
        memcpy(res->data + res->curr_checkpoint_beg + 1
            , &x
            , sizeof(size_t)
        );
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
        res->data[res->curr_checkpoint_beg + empty_diff + size - 1]
            = DIFF_CHAR_SEQ;
    }
    res->last_checkpoint_beg = res->curr_checkpoint_beg;
    res->last_checkpoint_end = res->curr_checkpoint_end;
}

int
init_diffstack(struct diffstack **ds, size_t alloc) {
    struct diffstack *res;

    ensure(reallocflexarr((void **) ds
        , sizeof(struct diffstack)
        , sizeof(uint8_t)
        , alloc
    ));
    res = *ds;
    memset(res, 0, sizeof(struct diffstack));
    res->alloc = alloc;
    return 0;
}

void
render(void) {
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
resize(int win_width_px, int win_height_px) {
    glViewport(0, 0, win_width_px, win_height_px);
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

static const char *fs_src  = "                                  \n\
#version 130                                                    \n\
in vec4 color;                                                  \n\
in vec2 uv;                                                     \n\
out vec4 fColor;                                                \n\
uniform sampler2D texture;                                      \n\
void main() {                                                   \n\
    vec4 bg = vec4(0.152, 0.156, 0.13, 1.0);                    \n\
    fColor = texture2D(texture, uv);                            \n\
    if (color.r < 0.5) {                                        \n\
        vec4 c = vec4(color.r * 4, color.g, color.b, color.a);  \n\
        fColor = (1.0 - fColor.r) * c + bg;                     \n\
    } else {                                                    \n\
        fColor = fColor.r * color;                              \n\
    }                                                           \n\
}                                                               \n\
";

void
key_input(unsigned char key, int x __unused, int y __unused) {
    size_t size = win.height * win.width;
    struct color *vertex_color;
    size_t i;

    switch (key) {
        case 'd':
            line_insert(0, doc->lines, doc, &win, "asd", 3);
            fill_screen(&doc, &win);
            gl_buffers_upload(&win);
            break;
        case 'q': case 27:
            exit(0);
        case 'c':
            vertex_color = win.font_color->vertex_color;
            for (i = 0; i < 6 * size; i++) {
                vertex_color[i].r = 1.0f - vertex_color[i].r;
                vertex_color[i].g = 1.0f - vertex_color[i].g;
                vertex_color[i].b = 1.0f - vertex_color[i].b;
            }
            glBufferSubData(GL_ARRAY_BUFFER
                , sizeof(struct quad_coord) * 2 * size
                , sizeof(struct quad_color) * size
                , win.font_color
            );
            break;
        default:
            return;
    }
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
    static char info[2048];
    int len = 2048;
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
    glutReshapeFunc(resize);
    glutDisplayFunc(render);
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

void
colorful_test(struct window *win) {
    float r, g, b;
    unsigned i, j;

    for (i = 0; i < win->scrollback_size; i++) {
        for (j = 0; j < win->width; j++) {
            r = sin(1 * i * M_PI / win->scrollback_size);
            g = sin(2 * i * M_PI / win->scrollback_size + M_PI);
            b = cos(2 * j * M_PI / win->width);
            set_quad_color(win->font_color + i * win->width + j
                , (r * r * 0.95 + 0.15) * .25
                , (g * g * 0.95 + 0.15) * 1.0
                , (b * b * 0.95 + 0.15) * 1.0
            );
        }
    }
}

int
gl_pipeline_init() {
    unsigned win_width_px = 0;
    unsigned win_height_px = 0;
    struct mmap_file file;
    struct rfp_file *rfp;

    xensure(load_file("unifont.rfp", &file) == 0);
    rfp = load_rfp_file(&file);
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
        , rfp->data
    );
    unload_file(&file);

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


void
resize_extern_line(struct line *line, size_t size, struct document *doc) {
    size_t alloc = line->alloc;
    uint8_t *doc_beg = doc->file.data;
    uint8_t *doc_end = doc->file.data + doc->file.size;
    uint8_t *str_beg;
    uint8_t *str_end;

    dbg_assert(line->intern_line < doc_beg || line->intern_line > doc_end);
    dbg_assert(line->ptr == NULL || line->alloc != 0);

    if (size < line->size) {
        line->size = size;
        return;
    }
    if (size >= line->alloc) {
        alloc += alloc / 2 + size;
        ensure(reallocflexarr((void **)&line->extern_line
            , sizeof(*line->extern_line)
            , sizeof(*str_beg)
            , alloc
        ) != NULL);

        if (line->intern_line == doc_end) {
            doc->must_leak = line->extern_line;
            line->extern_line = NULL;
            ensure(reallocflexarr((void **)&line->extern_line
                , sizeof(*line->extern_line)
                , sizeof(*str_beg)
                , alloc
            ) != NULL);
            memcpy(line->extern_line
                , doc->must_leak
                , sizeof(struct extern_line) + line->size * sizeof(*str_beg)
            );
        }
        line->alloc = alloc;
    }
    str_beg = line->extern_line->data + line->size;
    str_end = line->extern_line->data + size;

    if (RUNTIME_INSTR) {
        memzero(str_beg, str_end - str_beg, sizeof(*str_beg));
    }
    line->size = size;
}

void
init_extern_line(struct line *line
    , uint8_t *str
    , size_t size
    , enum UTF8_STATUS utf8_status
    , struct document *doc
    ) {
    ensure(line->ptr == NULL);
    resize_extern_line(line, size, doc);
    line->extern_line->utf8_status = utf8_status;
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
        ensure(reallocflexarr((void **)doc, sizeof(struct document), res->alloc
                , sizeof(*res->lines)
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
        res = reallocflexarr((void **)doc, sizeof(struct document), alloc
            , sizeof(*res->lines)
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
    struct mmap_file file;
    struct document *res;
    struct line_metadata *fst_line;
    size_t alloc;

    ensure(load_file(fname, &file) == 0);

    alloc = file.size / 32 + 1;
    res = reallocflexarr((void **)doc, sizeof(struct document), alloc
            , sizeof(*res->lines)
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

int
is_tok(uint32_t c) {
    if (c >= 'a' && c <= 'z') {
        return 1;
    }
    if (c >= 'A' && c <= 'Z') {
        return 1;
    }
    if (c >= '0' && c <= '9') {
        return 1;
    }
    if (c == '_') {
        return 1;
    }
    return 0;
}

int
is_digit(uint32_t c) {
    if (c >= '0' && c <= '9') {
        return 1;
    }
    return 0;
}

int
match(uint8_t *beg, uint8_t *curr, uint8_t *end, char *str, intptr_t size) {
    if (end - curr < size) {
        return 0;
    }
    if (curr - 1 - beg > 0 && is_tok(curr[-1])) {
        return 0;
    }
    if (end - curr + size > 0 && is_tok(curr[size])) {
        return 0;
    }
    return memcmp(curr, str, size) == 0;
}

struct colored_world {
    int color_pos;
    unsigned size;
};

#define ret_match(beg, str, end, color, res, cxstr)         \
    if (match(beg, str, end, cxstr, sizeof(cxstr) - 1)) {   \
        res.color_pos = color;                              \
        res.size = sizeof(cxstr) - 1;                       \
        return res;                                         \
    }

struct colored_world
match_word(uint8_t *beg, uint8_t *curr, uint8_t *end) {
    struct colored_world res = {0, 1};
    uint8_t *tmp;

    if (*curr <= 0x20 || *curr > 0x7e) {
        return res;
    }
    ret_match(beg, curr, end, 1, res, "#include");
    ret_match(beg, curr, end, 1, res, "#define");
    ret_match(beg, curr, end, 2, res, "if");
    ret_match(beg, curr, end, 2, res, "else");
    ret_match(beg, curr, end, 2, res, "do");
    ret_match(beg, curr, end, 2, res, "for");
    ret_match(beg, curr, end, 2, res, "while");
    ret_match(beg, curr, end, 2, res, "switch");
    ret_match(beg, curr, end, 2, res, "case");
    ret_match(beg, curr, end, 2, res, "break");
    ret_match(beg, curr, end, 2, res, "continue");
    ret_match(beg, curr, end, 2, res, "goto");
    ret_match(beg, curr, end, 2, res, "return");
    ret_match(beg, curr, end, 2, res, "sizeof");
    ret_match(beg, curr, end, 2, res, "return");
    ret_match(beg, curr, end, 3, res, "static");
    ret_match(beg, curr, end, 3, res, "extern");
    ret_match(beg, curr, end, 3, res, "struct");
    ret_match(beg, curr, end, 3, res, "enum");
    ret_match(beg, curr, end, 3, res, "union");
    ret_match(beg, curr, end, 3, res, "const");
    ret_match(beg, curr, end, 3, res, "volatile");
    ret_match(beg, curr, end, 3, res, "char");
    ret_match(beg, curr, end, 3, res, "unsigned");
    ret_match(beg, curr, end, 3, res, "signed");
    ret_match(beg, curr, end, 3, res, "long");
    ret_match(beg, curr, end, 3, res, "int");
    ret_match(beg, curr, end, 3, res, "void");
    ret_match(beg, curr, end, 3, res, "short");
    ret_match(beg, curr, end, 3, res, "uint8_t");
    ret_match(beg, curr, end, 3, res, "uint16_t");
    ret_match(beg, curr, end, 3, res, "uint32_t");
    ret_match(beg, curr, end, 3, res, "uint64_t");
    ret_match(beg, curr, end, 3, res, "size_t");
    ret_match(beg, curr, end, 3, res, "GLuint");
    ret_match(beg, curr, end, 3, res, "GLfloat");

    if (match(beg, curr, end, "//", 2)) {
        res.size = end - curr;
        res.color_pos = 4;
    } else if (*curr == '"') {
        end = memchr(curr + 1, '"', end - curr - 1);
        if (end) {
            res.size = end - curr + 1;
            res.color_pos = 5;
        }
    } else if (*curr == '<') {
        for (tmp = curr + 1; tmp < end; tmp++) {
            if (is_tok(*tmp) || *tmp == '/' || *tmp == '.') {
                continue;
            }
            if (*tmp == '>') {
                res.size = tmp + 1 - curr;
                res.color_pos = 5;
                break;
            }
            break;
        }
    }
    return res;
}

static struct color colors_table[] = {
    {0.85, 1.00, 1.00},
    {1.00, 0.00, 0.37},
    {1.00, 0.84, 0.37},
    {0.52, 0.68, 0.37},
    {0.52, 0.37, 0.52},
    {0.68, 0.68, 0.52},
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
    struct colored_world cw = { 0, 0 };
    uint32_t glyph;

    if (utf8_status == UTF8_CLEAN) {
        while (line_curr != line_end) {
            if (cw.size == 0) {
                cw = match_word(line_beg, line_curr, line_end);
            }
            cw.size--;
            set_quad_color(win->font_color + i * win->width + j
                , colors_table[cw.color_pos].r * 0.25
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
        }
    } else {
        while (line_curr != line_end) {
            set_quad_color(win->font_color + i * win->width + j
                , colors_table[0].r * 0.25
                , colors_table[0].g
                , colors_table[0].b
            );
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
        set_quad_color(win->font_color + i * win->width + j
            , colors_table[0].r * 0.25
            , colors_table[0].g
            , colors_table[0].b
        );
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

void
convert_line_external(struct line *line, struct document *doc) {
    struct extern_line* el = NULL;

    if (!is_line_internal(line, doc)) {
        return;
    }
    ensure(reallocflexarr((void **)&el
            , sizeof(*el)
            , line->size
            , sizeof(*el->data)
    ));
    el->utf8_status = UTF8_CLEAN;
    memcpy(el->data, line->intern_line, line->size * sizeof(*el->data));
    line->alloc = line->size;
    line->extern_line = el;
}


void
line_insert(size_t pos
    , struct line_metadata *lm
    , struct document *doc
    , struct window *win
    , uint8_t *data
    , size_t size
    ) {
    struct line *line = &lm->line;
    uint8_t *data_curr = data;
    uint8_t *data_end = data + size;
    size_t alloc;
    struct extern_line *el;
    uint8_t *el_curr;
    uint8_t *el_end;

    convert_line_external(line, doc);
    el = line->extern_line;

    if (line->size + size > line->alloc) {
        alloc = line->size + line->size / 2 + size;
        ensure(reallocflexarr((void **)&el
                , sizeof(*el)
                , alloc
                , sizeof(*el->data)
        ));
        line->alloc = alloc;
        line->extern_line = el;
    }
    el_curr = el->data + pos;
    el_end = el->data + line->size;

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
    recompute_win_lines_metadata(lm, doc, win);
    diffstack_insert_chars_add(&diff, data, size, pos, lm - doc->lines);
}

int
main(int argc, char *argv[]) {
    char *fname = "se.c";

    if (argc - 1 > 0) {
        fname = argv[1];
    }
    ensure(init_diffstack(&diff, 0x1000) == 0);
    ensure(init_doc(fname, &doc) == 0);

    xensure(win_init(argc, argv) == 0);
    xensure(gl_pipeline_init() == 0);

    glutMainLoop();
    return 0;
}
