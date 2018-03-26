static void * reallocarr(void **, size_t, size_t);
static void * reallocflexarr(void **, size_t, size_t, size_t);
static void memzero(void *, size_t, size_t);
int load_file(const char *, struct mmap_file *);
void unload_file(struct mmap_file *);
int load_bitmap(const char *, struct bitmap_data *);
int convert_bmp_window_to_rfp(struct bitmap_data *, unsigned, unsigned, const char *);
struct rfp_file * load_rfp_file(struct mmap_file *);
void render();
void invalidate_win_line_metadata(struct document *);
void resize_display_matrix(int, int);
void resize(int, int);
void key_input(unsigned char, int, int);
void key_special_input(int, int, int);
int gl_check_program(GLuint, int);
GLuint gl_compile_program(const char *, const char *);
char * str_intercalate(char *, size_t, char **, size_t, char);
int win_init(int, char *argv[]);
void set_quad_coord(struct quad_coord *, float, float, float, float);
void set_quad_color(struct quad_color *, float, float, float);
void fill_window_mesh(struct window *, unsigned, unsigned);
void gen_display_matrix(struct window *, unsigned, unsigned);
void colorful_test(struct window *);
int gl_pipeline_init();
void free_line(struct document *, struct line *);
void resize_extern_line(struct line *, size_t, struct document *);
void init_extern_line(struct line *, uint8_t *, size_t, enum UTF8_STATUS, struct document *);
uint8_t * load_line(struct line_metadata *, uint8_t *, uint8_t *, unsigned);
int is_fully_loaded(struct document *);
int is_line_internal(struct line *, struct document *);
int load_lines(size_t, struct document **);
uint32_t glyph_from_utf8(uint8_t **);
int resize_document_by(size_t, struct document **);
int init_doc(const char *, struct document **);
void fill_glyph_narrow(unsigned, unsigned, uint32_t, struct window *);
void fill_glyph_wide(unsigned, unsigned, uint32_t, struct window *);
unsigned fill_glyph(unsigned, unsigned, uint32_t, struct window *);
unsigned fill_line(unsigned, enum UTF8_STATUS, uint8_t *, uint8_t *, struct window *);
size_t glyphs_in_utf8_line(uint8_t *, uint8_t *, struct window *);
void recompute_win_lines_metadata(struct line_metadata *, struct document *, struct window *);
void fill_screen(struct document **, struct window *);
void move_scrollback(struct window *, enum MV_VERT_DIRECTION, struct document *);