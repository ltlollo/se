// This is free and unencumbered software released into the public domain.
// For more information, see LICENSE.

uint32_t first_glyph(uint8_t *, uint8_t *);
int is_same_class(uint32_t, uint32_t);
void resize_display_matrix(struct editor *, int, int);
struct selectarr * reserve_selectarr(struct selectarr **, size_t);
void screen_reposition(struct editor *);
char * str_intercalate(char *, size_t, char **, size_t, char);
void set_quad_coord(struct quad_vertex *, float, float, float, float);
void set_quad_vertex_uv(struct quad_vertex *, float, float, float, float);
void set_quad_vertex_col(struct quad_vertex *, struct color *);
void fill_window_mesh(struct window *, unsigned, unsigned);
struct window* gen_display_matrix(struct window **, unsigned, unsigned);
int load_font(const char *fname, struct mmap_file *);
void unload_font(struct mmap_file *file);
void free_line(struct line *, struct document *);
struct extern_line * reserve_line(struct line *, size_t, struct document *);
struct extern_line * reserve_extern_line(struct line *, size_t, struct document *);
struct extern_line * reserve_intern_line(struct line *, size_t, struct document *);
void merge_lines(struct line *, struct line *, struct document *);
void init_extern_line(struct line *, uint8_t *, size_t, enum UTF8_STATUS, struct document *);
uint8_t * load_line(struct line *, uint8_t *, uint8_t *, struct document *);
uint8_t * next_utf8_or_null(uint8_t *, uint8_t *);
int is_fully_loaded(struct document *);
int is_line_internal(struct line *, struct document *);
int is_line_utf8(struct line *, struct document *);
struct document * load_lines(struct editor *, size_t);
uint32_t glyph_from_utf8(uint8_t *);
uint32_t iter_glyph_from_utf8(uint8_t **);
struct document * resize_document_by(size_t, struct document **);
int init_sel(size_t, struct selectarr **);
int init_doc(const char *, struct document **);
void fill_glyph_narrow(unsigned, unsigned, uint32_t, struct window *);
void fill_glyph_wide(unsigned, unsigned, uint32_t, struct window *);
void fill_glyph(unsigned, unsigned, uint32_t, struct window *);
void fill_line_glyphs(unsigned, int, uint8_t *, uint8_t *, unsigned, struct window *);
void fill_line_colors(unsigned, int, uint8_t *, uint8_t *, unsigned, struct window *);
uint8_t * next_utf8_char(uint8_t *);
uint8_t * prev_utf8_revchar(uint8_t *);
uint8_t * prev_utf8_char(uint8_t *);
size_t glyphs_in_utf8_span(uint8_t *, uint8_t *);
size_t glyphs_in_utf8_revspan(uint8_t *, uint8_t *);
size_t glyphs_in_line_width(struct line *, struct document *,  size_t);
size_t glyphs_in_line(struct line *, struct document *);
uint8_t * sync_utf8_width_or_null(uint8_t *, uint8_t *, size_t);
uint8_t * sync_width_or_null(struct line *, size_t, struct document *);
void fill_screen(struct editor *);
void fill_screen_glyphs(struct editor *);
void fill_screen_colors(struct editor *);
uint8_t * begin_line(struct line *, struct document *);
uint8_t * end_line(struct line *, struct document *);
size_t window_area(struct window *);
void move_scrollback_up(struct editor *);
void move_scrollback_down(struct editor *);
void move_scrollback(struct editor *, enum MV_VERT_DIRECTION, size_t);
struct extern_line * convert_line_external(struct line *, struct document *);
struct document * insert_empty_lines(size_t, size_t, struct document **);
void copy_extern_line(struct line *, uint8_t *, size_t, struct document *);
void insert_n_line(size_t, size_t, size_t, struct document **);
void remove_n_line(size_t, size_t, struct document *);
void win_dmg_from_lineno(struct window *, size_t);
void win_dmg_calc(struct window *, struct selectarr *);
void save_file(struct editor *);
void handle_event(struct editor *, SDL_Event *, union uistate *);
void render_loop(struct editor *, union uistate *);
void cursors_reposition(struct selectarr *, struct document *);
int init_editor(struct editor *, const char *);
int fmemcmp(char *restrict, char *restrict, size_t);
int is_alpha(uint32_t);
int is_char(uint32_t);
int is_digit(uint32_t);
int is_alnum(uint32_t);
int is_tok(uint32_t);
int is_space(uint32_t);
int is_hex(uint32_t);
int keyword_match(uint8_t *, uint8_t *, char *, intptr_t);
uint8_t * number_match(uint8_t *, uint8_t *);
struct colored_span color_span(uint8_t *, uint8_t *, uint8_t *);
struct diffstack * diffstack_reserve(struct diffstack **, size_t);
struct diffstack * diffstack_aggregate_begin(struct diffstack **, struct diffaggr_info *);
struct diffstack * diffstack_aggregate_end(struct diffstack **, struct diffaggr_info *);
void diffstack_insert_merge(struct diffstack **, struct line *, size_t, size_t, size_t, struct diffaggr_info *);
void diffstack_insert_split(struct diffstack **, size_t, size_t, size_t, struct diffaggr_info *);
void diffstack_insert_chars_add(struct diffstack **, uint8_t *, size_t, size_t, size_t, struct diffaggr_info *);
void diffstack_insert_chars_del(struct diffstack **, uint8_t *, size_t, size_t, size_t, struct diffaggr_info *);
void diffstack_undo_line_merge(uint8_t *, uint8_t *, struct document *);
void diffstack_undo_line_split(uint8_t *, uint8_t *, struct document *);
void diffchars_unpack(char *, size_t *, size_t *);
void diffchars_pack(char *, size_t, size_t);
void diffsplit_unpack(char *, size_t *, size_t *, size_t *);
void diffsplit_pack(char *, size_t, size_t, size_t);
void diffstack_undo_chars_del(uint8_t *, uint8_t *, struct document *);
void diffstack_undo_chars_add(uint8_t *, uint8_t *, struct document *);
void diffstack_redo_chars_add(uint8_t *, uint8_t *, struct document *);
void diffstack_redo_chars_del(uint8_t *, uint8_t *, struct document *);
void diffstack_redo_line_split(uint8_t *, uint8_t *, struct document *);
void diffstack_redo_line_merge(uint8_t *, uint8_t *, struct document *);
void diffstack_redo_aggregate(uint8_t *, uint8_t *, struct document *);
uint8_t * diffstack_curr_mvback(uint8_t *);
uint8_t * diffstack_curr_mvforw(uint8_t *);
uint8_t * diffaggr_first_simple(uint8_t *);
uint8_t * diffaggr_last_simple(uint8_t *);
int reposition_cursor_undo(struct editor *, uint8_t *, uint8_t *);
void show_revseq(char *, char *);
void show_seq(char *, char *);
void diff_show(uint8_t *, uint8_t *, size_t);
void diffstack_show(const char *, struct diffstack *);
int reposition_cursor_redo(struct editor *, uint8_t *, uint8_t *);
int diffstack_undo(struct editor *);
void diffstack_undo_aggregate(uint8_t *, uint8_t *, struct document *);
int diffstack_redo(struct editor *);
int init_diffstack(struct diffstack **, size_t);
void diff_line_split(struct diffstack **, size_t, size_t, struct document **, struct diffaggr_info *);
void diff_line_insert(struct diffstack **, size_t, struct line *, struct document *, uint8_t *, size_t, struct diffaggr_info *);
void diff_line_remove(struct diffstack **, size_t, struct line *, struct document *, size_t, struct diffaggr_info *);
void diff_line_merge(struct diffstack **, size_t, size_t, struct document **, struct diffaggr_info *);
void key_move_input(struct editor *, int, int);
void alt_cursors_down(struct editor *, int);
void alt_cursors_up(struct editor *, int);
void move_cursors_right(struct selectarr *, int, struct document *);
void move_cursors_left(struct selectarr *, int, struct document *);
void add_to_cursor(struct editor *, struct selection *, uint8_t *, size_t, struct diffaggr_info *);
void key_insert_chars(struct editor *, int, int);
void delete_selection(struct editor *, struct selection *, struct diffaggr_info *);
void delete_cursor(struct editor *, struct selection *, struct diffaggr_info *);
int is_lines_merge(struct selectarr *);
void delete_nl(struct editor *, struct selection *, struct diffaggr_info *);
void delete_nls(struct editor *, struct diffaggr_info *);
void key_backspace(struct editor *);
void insert_nl(struct editor *, struct selection *, struct diffaggr_info *);
void key_return(struct editor *);
void ui_window_render(struct editor *, union uistate *);
void ui_window_resize(struct editor *, union uistate *, int, int);
int ui_window_init(union uistate *, int, char **);
int ui_pipeline_init(struct editor *, union uistate *);
void ui_buffers_upload(struct editor *, union uistate *);
void ui_buffers_upload_dmg(struct editor *, union uistate *);
void ui_window_render(struct editor *, union uistate *);
void ui_window_resize(struct editor *, union uistate *, int, int);
int ui_window_init(union uistate *, int, char **);
int ui_pipeline_init(struct editor *, union uistate *);
void ui_buffers_upload(struct editor *, union uistate *);
void ui_buffers_upload_dmg(struct editor *, union uistate *);
int conf_add(struct conf_file *, char key[static KEY_LEN], char val[static VAL_LEN]);
struct conf_data * conf_find(struct conf_file *, char *);
int conf_fill(struct conf_file *);
void init_conf_file(struct conf_file *);
