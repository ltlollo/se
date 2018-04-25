uint32_t first_glyph(uint8_t *, uint8_t *);
int always(uint32_t glyph);
int is_same_class(uint32_t, uint32_t);
struct diffstack * diffstack_reserve(struct diffstack **, size_t);
void diffstack_insert_merge(struct diffstack **, struct line_metadata *, size_t, size_t, size_t);
void diffstack_insert_split(struct diffstack **, size_t, size_t, size_t);
void diffstack_insert_chars_add(struct diffstack **, uint8_t *, size_t, size_t, size_t);
void diffstack_insert_chars_del(struct diffstack **, uint8_t *, size_t, size_t, size_t);
void diffstack_undo_line_merge(uint8_t *, uint8_t *, struct document *);
void diffstack_undo_line_split(uint8_t *, uint8_t *, struct document *);
void diffstack_undo_chars_del(uint8_t *, uint8_t *, struct document *);
void diffstack_undo_chars_add(uint8_t *, uint8_t *, struct document *);
uint8_t * diffstack_curr_mvback(uint8_t *);
int diffstack_undo(struct diffstack *, struct document *);
int init_diffstack(struct diffstack **, size_t);
void window_render();
void invalidate_win_line_metadata(struct document *);
void resize_display_matrix(int, int);
void window_resize(int, int);
void key_input(unsigned char, int x, int y);
void key_special_input(int, int x, int y);
int gl_check_program(GLuint, int);
GLuint gl_compile_program(const char *, const char *);
char * str_intercalate(char *, size_t, char **, size_t, char);
int win_init(int, char *argv[]);
void set_quad_coord(struct quad_coord *, float, float, float, float);
void set_quad_color(struct quad_color *, float, float, float);
void fill_window_mesh(struct window *, unsigned, unsigned);
void gen_display_matrix(struct window *, unsigned, unsigned);
int load_font(const char *fname, struct mmap_file *);
void unload_font(struct mmap_file *file);
int gl_pipeline_init();
void free_line(struct document *, struct line *);
struct extern_line * reserve_line(struct line *, size_t, struct document *);
struct extern_line * reserve_extern_line(struct line *, size_t, struct document *);
struct extern_line * reserve_intern_line(struct line *, size_t, struct document *);
void merge_lines(struct line_metadata *, struct line_metadata *, struct document *);
void init_extern_line(struct line *, uint8_t *, size_t, enum UTF8_STATUS, struct document *);
uint8_t * load_line(struct line_metadata *, uint8_t *, uint8_t *, unsigned);
uint8_t * next_utf8_or_null(uint8_t *, uint8_t *);
int is_fully_loaded(struct document *);
int is_line_internal(struct line *, struct document *);
int is_line_utf8(struct line *, struct document *);
int load_lines(size_t, struct document **);
uint32_t glyph_from_utf8(uint8_t **);
struct document * resize_document_by(size_t, struct document **);
int init_sel(size_t, struct selectarr **);
int init_doc(const char *, struct document **);
void fill_glyph_narrow(unsigned, unsigned, uint32_t, struct window *);
void fill_glyph_wide(unsigned, unsigned, uint32_t, struct window *);
unsigned fill_glyph(unsigned, unsigned, uint32_t, struct window *);
unsigned fill_line(unsigned, enum UTF8_STATUS, uint8_t *, uint8_t *, size_t, struct window *, struct selection *, struct selection *);
uint8_t * next_utf8_char(uint8_t *);
size_t glyphs_in_utf8_line(uint8_t *, uint8_t *, struct window *);
uint8_t * offsetof_utf8_width(uint8_t *, uint8_t *, struct window *, size_t);
uint8_t * offsetof_width(struct line_metadata *, struct window *, struct document *, size_t);
void recompute_win_lines_metadata(struct line_metadata *, struct document *, struct window *);
void fill_screen(struct document **, struct window *);
uint8_t * begin_line_metadata(struct line_metadata *, struct document *);
uint8_t * end_line_metadata(struct line_metadata *, struct document *);
void move_scrollback_up(struct window *, struct document **);
void move_scrollback_down(struct window *, struct document **);
void gl_buffers_upload(struct window *);
void move_scrollback(struct window *, enum MV_VERT_DIRECTION, size_t, struct document **);
struct extern_line * convert_line_external(struct line *, struct document *);
void diff_line_merge(struct diffstack **, size_t, size_t, struct document **);
struct document * insert_empty_lines(size_t, size_t, struct document **);
void copy_extern_line(struct line *, uint8_t *, size_t, struct document *);
void insert_n_line(size_t, size_t, size_t, struct document **);
void diff_line_split(struct diffstack **, size_t, size_t, struct document **);
void diff_line_insert(struct diffstack **, size_t, struct line_metadata *, struct document *, uint8_t *, size_t);
void diff_line_remove(struct diffstack **, size_t, struct line_metadata *, struct document *, size_t);
