// This is free and unencumbered software released into the public domain.
// For more information, see LICENSE.

int gl_check_program(GLuint, int);
GLuint gl_compile_program(const char *, const char *);
void gl_window_render(struct editor *, struct gl_data *);
void gl_window_resize(struct editor *, struct gl_data *, int, int);
int gl_window_init(struct gl_data *, int, char **);
int gl_pipeline_init(struct editor *, struct gl_data *);
void gl_buffers_upload(struct editor *, struct gl_data *);
void gl_buffers_upload_dmg(struct editor *, struct gl_data *);
