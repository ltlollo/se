#include "ui.h"

#ifdef USE_OPENGL
#include "gl.c"

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
#include "vk.c"

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
