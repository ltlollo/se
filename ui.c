#include "ui.h"

#ifdef USE_OPENGL

static const char *vs_src = "                   \n\
#version 130                                    \n\
in vec4 s_pos;                                  \n\
in vec4 s_color;                                \n\
in vec2 s_uv;                                   \n\
uniform float u_scroll;                         \n\
out vec4 color;                                 \n\
out vec2 uv;                                    \n\
void main() {                                   \n\
    color = s_color;                            \n\
    uv = s_uv;                                  \n\
    gl_Position = vec4(s_pos.x                  \n\
        , s_pos.y + u_scroll                    \n\
        , s_pos.z                               \n\
        , 1.0                                   \n\
    );                                          \n\
}                                               \n\
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

void
gl_window_render(struct editor *ed, struct gl_data *gl_id) {
    struct window *win = ed->win;
    unsigned size = window_area(win);

    gl_buffers_upload_dmg(ed, gl_id);
    glUniform1f(gl_id->scroll
        , 2.0 / ed->win->height * ed->win->scrollback_pos
    );
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6 * size * 2);
    SDL_GL_SwapWindow(gl_id->win);
}

void
gl_window_resize(struct editor *ed
    , struct gl_data *gl_id
    , int win_width_px
    , int win_height_px
    ) {
    glViewport(0, 0, win_width_px, win_height_px);
    resize_display_matrix(ed, win_width_px, win_height_px);

    glBufferData(GL_ARRAY_BUFFER
        , sizeof(struct quad_vertex) * window_area(ed->win) * 2
        , NULL
        , GL_DYNAMIC_DRAW
    );
    gl_buffers_upload(ed, gl_id);
    glVertexAttribPointer(gl_id->pos, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 7
        , 0
    );
    glVertexAttribPointer(gl_id->uv, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 7
        , (void *)(sizeof(float) * 2)
    );
    glVertexAttribPointer(gl_id->col, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 7
        , (void *)(sizeof(float) * 4)
    );
}


int
gl_window_init(struct gl_data *gl_id, int argc, char **argv) {
    static char window_title[128];
    int err;

    str_intercalate(window_title, sizeof(window_title) - 1, argv, argc, ' '); 

    err = SDL_Init(SDL_INIT_VIDEO);
    if (err != 0) {
        errx(1, "SDL_Init: %s", SDL_GetError());
    }
    gl_id->win = SDL_CreateWindow(window_title
        , SDL_WINDOWPOS_UNDEFINED
        , SDL_WINDOWPOS_UNDEFINED
        , 0
        , 0
        , SDL_WINDOW_OPENGL | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_RESIZABLE
    );
    if (gl_id->win == NULL) {
        errx(1, "SDL_CreateWindow: %s", SDL_GetError());
    }
    gl_id->ctx = SDL_GL_CreateContext(gl_id->win);
    xensure(glewInit() == GLEW_OK);

    glClearColor(0.152, 0.156, 0.13, 1.0);
    return 0;
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
    gl_id->scroll = glGetUniformLocation(gl_id->prog, "u_scroll");
    xensure(glGetError() == GL_NO_ERROR);

    memzero(ed->win, 1, sizeof(struct window));
    gl_window_resize(ed, gl_id, win_width_px, win_height_px);

    glEnableVertexAttribArray(gl_id->pos);
    glEnableVertexAttribArray(gl_id->col);
    glEnableVertexAttribArray(gl_id->uv);
    glUniform1f(gl_id->scroll, 0.0); 

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
gl_buffers_upload(struct editor *ed, struct gl_data *gl_id) {
    size_t size = ed->win->scrollback_size * ed->win->width;
    (void)gl_id;

    glBufferSubData(GL_ARRAY_BUFFER
        , 0
        , sizeof(struct quad_vertex) * size
        , ed->win->glyph_mesh
    );
}

void
gl_buffers_upload_dmg(struct editor *ed, struct gl_data *gl_id) {
    struct window *win = ed->win;
    struct document *doc = ed->doc;
    size_t dmg_beg = win->dmg_scrollback_beg;
    size_t dmg_end = win->dmg_scrollback_end;
    size_t scrollback_beg = doc->line_off;
    size_t scrollback_end = doc->line_off + win->scrollback_size;

    size_t dmg_isect_beg = max(scrollback_beg, dmg_beg) - scrollback_beg;
    size_t dmg_isect_end = min(scrollback_end, dmg_end) - scrollback_beg;
    size_t dmg_isect_size = dmg_isect_end - dmg_isect_beg;

    (void)gl_id;

    glBufferSubData(GL_ARRAY_BUFFER
        , sizeof(struct quad_vertex) * dmg_isect_beg * win->width
        , sizeof(struct quad_vertex) * (dmg_isect_size * win->width)
        , win->glyph_mesh + dmg_isect_beg * win->width
    );
}


void
ui_window_render(struct editor *ed, union uistate *ui) {
    gl_window_render(ed, &ui->gl_id);
}

void
ui_window_resize(struct editor *ed, union uistate *ui, int w, int h) {
    gl_window_resize(ed, &ui->gl_id, w, h);
}

int
ui_window_init(union uistate *ui, int argc, char **argv) {
    return gl_window_init(&ui->gl_id, argc, argv);
}

int
ui_pipeline_init(struct editor *ed, union uistate *ui) {
    return gl_pipeline_init(ed, &ui->gl_id);
}

void
ui_buffers_upload(struct editor *ed, union uistate *ui) {
    gl_buffers_upload(ed, &ui->gl_id);
}

void
ui_buffers_upload_dmg(struct editor *ed, union uistate *ui) {
    gl_buffers_upload_dmg(ed, &ui->gl_id);
}

#else

void
vk_window_render(struct editor *ed, struct vkstate *vks) {
    struct window *win = ed->win;
    uint32_t image_idx;

    VkPipelineStageFlags stages = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    };
    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &vks->image_sem,
        .pWaitDstStageMask = &stages,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &vks->render_sem,
        .commandBufferCount = 1,
    };
    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &vks->render_sem,
        .swapchainCount = 1,
        .pSwapchains = &vks->swapchain,
        .pImageIndices = &image_idx,
    };
    struct ubotype val = { .move = 2.0 / win->height * win->scrollback_pos, };

    int err = vkAcquireNextImageKHR(vks->device, vks->swapchain, ~0ull,
        vks->image_sem, VK_NULL_HANDLE, &image_idx
    );
    switch (err) {
        case VK_ERROR_OUT_OF_DATE_KHR:
            warnx("VK_ERROR_OUT_OF_DATE_KHR");
            vkdelete(vks);
            vkcreate(vks, win->width, win->scrollback_size, win->glyph_mesh);
            err = vkAcquireNextImageKHR(vks->device, vks->swapchain, ~0ull,
                vks->image_sem, VK_NULL_HANDLE, &image_idx
            );
            break;
        case VK_SUBOPTIMAL_KHR:
            warnx("VK_SUBOPTIMAL_KHR");
            break;
        default:
            xvkerr(err);
    }
    vk_update_ubo(vks->device, vks->ubo_mem[image_idx], &val);
    vk_buffers_upload_dmg(ed, vks);

    submit_info.pCommandBuffers = vks->command_buffers + image_idx;
    xvkerr(vkQueueSubmit(vks->graphics_queue, 1, &submit_info,
            VK_NULL_HANDLE
    ));
    vkQueuePresentKHR(vks->graphics_queue, &present_info);
}

void
vk_window_resize(struct editor *ed
    , struct vkstate *vks
    , int win_width_px
    , int win_height_px
) {
    if (win_width_px == 0) {
        win_width_px = 1;
    }
    if (win_height_px == 0) {
        win_height_px = 1;
    }
    resize_display_matrix(ed, win_width_px, win_height_px);

    vks->w = win_width_px, vks->h = win_height_px;
    warnx("SDL_WINDOWEVENT_RESIZED: %dx%d", vks->w, vks->h);

    vkdelete(vks);

    struct window *win = ed->win;
    vkcreate(vks, win->width, win->scrollback_size, win->glyph_mesh);
}

int
vk_window_init(struct vkstate *vks, int argc, char **argv) {
    static char window_title[128];

    str_intercalate(window_title, sizeof(window_title) - 1, argv, argc, ' '); 

    vks->w = 800;
    vks->h = 600;
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    vks->win = SDL_CreateWindow(window_title, 0, 0, vks->w, vks->h
        , SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN | SDL_WINDOW_MAXIMIZED
        | SDL_WINDOW_RESIZABLE
    );
    vkinit(vks, vks->win, debug_callback);
    return 0;
}

int
vk_pipeline_init(struct editor *ed, struct vkstate *vks) {
    void *font_data = malloc(0x1440000);
    struct mmap_file file;
    struct rfp_file *rfp;

    memzero(ed->win, 1, sizeof(struct window));

    xensure(font_data);
    xensure(load_font("unifont.cfp", &file) == 0);
    rfp = load_cfp_file(&file);
    rfp_decompress(rfp, font_data);

    float empty_quad[sizeof(struct quad_vertex) / sizeof(float)] = {};

    vkmktex(vks, font_data, 0x1200, 0x1200);
    vkcreate(vks, 1, 1, empty_quad);
    vkupdatexcmd(vks, 0x1200, 0x1200);

    free(font_data);
    return 0;
}

void
vk_buffers_upload(struct editor *ed, struct vkstate *vks) {
    size_t size = ed->win->scrollback_size * ed->win->width;

    if (size) {
        void *data;
        vkMapMemory(vks->device, vks->vertex_mem, 0, size, 0, &data);
        memcpy(data, ed->win->glyph_mesh, size);
        vkUnmapMemory(vks->device, vks->vertex_mem);
    }
}

void
vk_buffers_upload_dmg(struct editor *ed, struct vkstate *vks) {
    struct window *win = ed->win;
    struct document *doc = ed->doc;
    size_t dmg_beg = win->dmg_scrollback_beg;
    size_t dmg_end = win->dmg_scrollback_end;
    size_t scrollback_beg = doc->line_off;
    size_t scrollback_end = doc->line_off + win->scrollback_size;

    size_t dmg_isect_beg = max(scrollback_beg, dmg_beg) - scrollback_beg;
    size_t dmg_isect_end = min(scrollback_end, dmg_end) - scrollback_beg;
    size_t dmg_isect_size = dmg_isect_end - dmg_isect_beg;

    if (dmg_isect_size * win->width == 0) {
        return;
    }
    void *data;

    size_t quad_off = dmg_isect_beg * win->width;
    size_t quad_size = dmg_isect_size * win->width;

    vkMapMemory(vks->device
        , vks->vertex_mem
        , sizeof(struct quad_vertex) * quad_off
        , sizeof(struct quad_vertex) * quad_size
        , 0
        , &data
    );
    memcpy(data
        , win->glyph_mesh + quad_off
        , quad_size * sizeof(struct quad_vertex)
    );
    vkUnmapMemory(vks->device, vks->vertex_mem);
}

void
ui_window_render(struct editor *ed, union uistate *ui) {
    vk_window_render(ed, &ui->vks);
}

void
ui_window_resize(struct editor *ed, union uistate *ui, int w, int h) {
    vk_window_resize(ed, &ui->vks, w, h);
}

int
ui_window_init(union uistate *ui, int argc, char **argv) {
    return vk_window_init(&ui->vks, argc, argv);
}

int
ui_pipeline_init(struct editor *ed, union uistate *ui) {
    return vk_pipeline_init(ed, &ui->vks);
}

void
ui_buffers_upload(struct editor *ed, union uistate *ui) {
    vk_buffers_upload(ed, &ui->vks);
}

void
ui_buffers_upload_dmg(struct editor *ed, union uistate *ui) {
    vk_buffers_upload_dmg(ed, &ui->vks);
}
#endif
