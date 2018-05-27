// This is free and unencumbered software released into the public domain.
// For more information, see LICENSE.

void
key_move_input(struct editor *ed, int key, int mod) {
    struct selection *sel_curr;
    size_t delta;

    switch (key) {
        case SDLK_DOWN:
            alt_cursors_down(ed, mod);
            break;
        case SDLK_UP:
            alt_cursors_up(ed, mod);
            break;
        case SDLK_RIGHT:
            move_cursors_right(ed->selv, mod, ed->doc);
            break;
        case SDLK_LEFT:
            move_cursors_left(ed->selv, mod, ed->doc);
            break;
        case SDLK_PAGEDOWN:
            ed->selv->focus = ed->selv->data + ed->selv->size - 1;
            load_lines(ed, ed->win->height);
            delta = min(ed->doc->loaded_size - ed->selv->focus->line
                , ed->win->height
            );
            sel_curr = ed->selv->data;
            while (sel_curr != ed->selv->data + ed->selv->size) {
                sel_curr->line += delta;
                sel_curr++;
            }
            break;
        case SDLK_PAGEUP:
            ed->selv->focus = ed->selv->data;
            delta = min(ed->selv->focus->line, ed->win->height);
            sel_curr = ed->selv->data;
            while (sel_curr != ed->selv->data + ed->selv->size) {
                sel_curr->line -= delta;
                sel_curr++;
            }
            break;
        default:
            break;
    }
}

void
alt_cursors_down(struct editor *ed, int mod) {
    struct document *doc = ed->doc;
    struct selectarr *selv = ed->selv;
    struct selection *sel_curr;

    if ((mod & KMOD_LSHIFT) == 0
        && selv->focus != selv->data + selv->size - 1) {
        selv->focus = selv->data + selv->size - 1;
        return;
    }
    if ((mod & KMOD_LSHIFT)
        && selv->focus == selv->data + selv->size - 1
    ) {
        doc = load_lines(ed, 1);
        if (doc->loaded_size == selv->focus->line) {
            return;
        }
        selv = reserve_selectarr(&ed->selv, 1);
        memcpy(selv->data + selv->size
            , selv->data + selv->size - 1
            , sizeof(struct selection)
        );
        selv->focus = selv->data + selv->size++;
        selv->focus->line++;
    } else if ((mod & KMOD_LSHIFT)
        && selv->focus == selv->data
    ) {
        memmove(selv->data
            , selv->data + 1
            , sizeof(struct selection) * --selv->size
        );
    } else {
        doc = load_lines(ed, 1);
        if (selv->focus->line >= doc->loaded_size) {
            dbg_assert(selv->focus->line >= doc->loaded_size);
            return;
        }
        sel_curr = selv->data;
        while (sel_curr != selv->data + selv->size) {
            sel_curr->line++;
            sel_curr++;
        }
    }
}

void
alt_cursors_up(struct editor *ed, int mod) {
    struct selectarr *selv = ed->selv;
    struct selection *sel_curr;

    if ((mod & KMOD_LSHIFT) == 0
        && selv->focus != selv->data) {
        selv->focus = selv->data;
        return;
    }
    if ((mod & KMOD_LSHIFT)
        && selv->focus == selv->data
    ) {
        if (selv->focus->line == 0) {
            return;
        }
        selv = reserve_selectarr(&ed->selv, 1);
        memmove(selv->data + 1
            , selv->data
            , sizeof(struct selection) * selv->size++
        );
        selv->focus = selv->data;
        selv->focus->line--;
        selv->focus->glyph_beg = 0;
        selv->focus->glyph_end = 0;
    } else if ((mod & KMOD_LSHIFT)
        && selv->focus == selv->data + selv->size - 1
    ) {
        if (selv->size > 1) {
            selv->focus = selv->data + --selv->size - 1;
        }
    } else {
        selv->focus = selv->data;
        if (selv->focus->line == 0) {
            return;
        }
        sel_curr = selv->data;
        while (sel_curr != selv->data + selv->size) {
            sel_curr->line--;
            sel_curr++;
        }
    }
}

void
move_cursors_right(struct selectarr *selv, int mod, struct document *doc) {
    struct selection *sel_curr;
    struct line* line;
    uint8_t *line_curr;
    uint8_t *line_end;
    uint32_t glyph;
    int utf8;
    size_t span;
    size_t i;

    if (mod & KMOD_LCTRL) {
        sel_curr = selv->data;
        while (sel_curr != selv->data + selv->size) {
            dbg_assert(sel_curr->line <= doc->loaded_size);
            span = 0;
            line = doc->lines + sel_curr->line;
            line_curr = begin_line(line, doc);
            line_end = end_line(line, doc);
            utf8 = is_line_utf8(line, doc);
            if (utf8) {
                for (i = 0; i < sel_curr->glyph_end; i++) {
                    line_curr = next_utf8_char(line_curr);
                    if (line_curr == line_end) {
                        break;
                    }
                }
                if (line_curr < line_end && utf8) {
                    glyph = iter_glyph_from_utf8(&line_curr);
                    sel_curr->glyph_end++;
                    span++;
                    while (line_curr < line_end) {
                        if (!is_same_class(iter_glyph_from_utf8(&line_curr)
                                , glyph
                        )) {
                            break;
                        }
                        sel_curr->glyph_end++;
                        span++;
                    }
                }
            } else {
                sel_curr->glyph_end++;
                span++;
            }
            if ((mod & KMOD_LSHIFT) == 0) {
                sel_curr->glyph_beg += span;
            }
            sel_curr++;
        }
    } else {
        sel_curr = selv->data;
        while (sel_curr != selv->data + selv->size) {
            sel_curr->glyph_end++;
            sel_curr++;
        }
        if ((mod & KMOD_LSHIFT) == 0) {
            sel_curr = selv->data;
            while (sel_curr != selv->data + selv->size) {
                sel_curr->glyph_beg++;
                sel_curr++;
            }
        }
    }
}

void
move_cursors_left(struct selectarr *selv, int mod, struct document *doc) {
    struct selection *sel_curr;
    struct line* line;
    uint8_t *line_curr;
    uint8_t *line_beg;
    uint8_t *line_end;
    uint32_t glyph;
    int utf8;
    size_t span;
    size_t i;

    if (mod & KMOD_LCTRL) {
        sel_curr = selv->data;
        while (sel_curr != selv->data + selv->size) {
            dbg_assert(sel_curr->line <= doc->loaded_size);
            if (sel_curr->glyph_end == 0) {
                sel_curr++;
                continue;
            }
            span = 0;
            line = doc->lines + sel_curr->line;
            line_beg = begin_line(line, doc);
            line_end = end_line(line, doc);
            line_curr = line_beg;
            utf8 = is_line_utf8(line, doc);
            if (utf8) {
                for (i = 0; i < sel_curr->glyph_end; i++) {
                    line_curr = next_utf8_char(line_curr);
                    if (line_curr == line_end) {
                        break;
                    }
                }
                if (line_curr > line_beg) {
                    line_curr = prev_utf8_char(line_curr);
                    glyph = glyph_from_utf8(line_curr);
                    sel_curr->glyph_end--;
                    span++;
                    while (line_curr != line_beg) {
                        line_curr = prev_utf8_char(line_curr);;
                        if (!is_same_class(glyph_from_utf8(line_curr)
                                , glyph
                        )) {
                            break;
                        }
                        sel_curr->glyph_end--;
                        span++;
                    }
                }
            } else if (sel_curr->glyph_end) {
                sel_curr->glyph_end--;
                span++;
            }
            if ((mod & KMOD_LSHIFT) == 0) {
                sel_curr->glyph_beg -= min(span, sel_curr->glyph_beg);
            }
            if (sel_curr->glyph_beg > sel_curr->glyph_end) {
                sel_curr->glyph_beg = sel_curr->glyph_end;
            }
            sel_curr++;
        }
    } else {
        if (mod & KMOD_LSHIFT) {
            sel_curr = selv->data;
            while (sel_curr != selv->data + selv->size) {
                if (sel_curr->glyph_end > sel_curr->glyph_beg) {
                    sel_curr->glyph_end--;
                }
                sel_curr++;
            }
        } else {
            sel_curr = selv->data;
            while (sel_curr != selv->data + selv->size) {
                if (sel_curr->glyph_beg) {
                    sel_curr->glyph_beg--;
                    sel_curr->glyph_end--;
                }
                sel_curr++;
            }
        }
    }
}

void
key_insert_chars(struct editor *ed, int key, int mod) {
    struct selection *sel = ed->selv->data;
    struct selection *sel_end = ed->selv->data + ed->selv->size;
    struct document *doc = ed->doc;
    struct diffstack *diff = ed->diff;
    size_t diff_curr_beg = diff->curr_checkpoint_beg;
    size_t diff_curr_end = diff->curr_checkpoint_end;
    size_t aggr_count = 0;
    int add_new_line = 0;
    size_t aggr_beg_pos;
    uint8_t *line_beg;
    uint8_t *line_curr;
    size_t pos;
    struct line *line;
    uint8_t buf[4];
    size_t bufsz;

    if (mod & KMOD_LCTRL) {
        return;
    }
    buf[0] = key & 0x7f;
    bufsz = 1;

    if ((sel_end - 1)->line == ed->doc->loaded_size) {
        add_new_line = 1;
    }
    if (ed->selv->size > 1 || add_new_line) {
        diff = diffstack_reserve(&ed->diff, SIZE_AGGR);
        diff->curr_checkpoint_beg = diff->curr_checkpoint_end;
        diff->curr_checkpoint_end = diff->curr_checkpoint_beg + SIZE_AGGR;
        aggr_beg_pos =  diff->curr_checkpoint_beg;
    }
    if (add_new_line) {
        ensure(ed->doc->loaded_size); // How to handle empty file?
        line = ed->doc->lines + ed->doc->loaded_size - 1;
        diff_line_split(&ed->diff
            , line->size
            , ed->doc->loaded_size - 1
            , &ed->doc
        );
        aggr_count++;
    }
    while (sel != sel_end) {
        line = ed->doc->lines + sel->line;
        line_beg = begin_line(line, doc);
        line_curr = sync_width_or_null(line, sel->glyph_end, doc);
        if (line_curr) {
            pos = line_curr - line_beg;
            diff_line_insert(&ed->diff
                , pos
                , line
                , ed->doc
                , buf
                , bufsz
            );
            sel->glyph_beg++;
            sel->glyph_end++;
            aggr_count++;
        }
        sel++;
    }
    if (aggr_count == 0) {
        diff->curr_checkpoint_beg = diff_curr_beg;
        diff->curr_checkpoint_end = diff_curr_end;
        return;
    }
    if (ed->selv->size > 1) {
        diff = diffstack_reserve(&ed->diff, SIZE_AGGR);
        diff->curr_checkpoint_beg = aggr_beg_pos;
        diff->curr_checkpoint_end = diff->curr_checkpoint_end + SIZE_AGGR;
        diff->last_checkpoint_beg = diff->curr_checkpoint_beg;
        diff->last_checkpoint_end = diff->curr_checkpoint_end;
        diff->data[aggr_beg_pos] = DIFF_AGGREGATE;
        memcpy(diff->data + aggr_beg_pos + 1, &aggr_count, sizeof(aggr_count));
        memcpy(diff->data + diff->curr_checkpoint_end - sizeof(aggr_count) - 1
            , &aggr_count
            , sizeof(aggr_count)
        );
        diff->data[diff->curr_checkpoint_end - 1] = DIFF_AGGR_SEQ;
    }

    fill_screen_glyphs(ed, 0);
};

void
key_backspace(struct editor *ed) {
    struct selection *sel = ed->selv->data;
    struct selection *sel_end = ed->selv->data + ed->selv->size;
    struct document *doc = ed->doc;
    struct diffstack *diff = ed->diff;
    size_t diff_curr_beg = diff->curr_checkpoint_beg;
    size_t diff_curr_end = diff->curr_checkpoint_end;
    size_t aggr_count = 0;
    struct line *line;
    uint8_t *line_beg;
    uint8_t *line_end;
    uint8_t *line_curr;
    size_t pos;
    size_t delta;
    size_t aggr_beg_pos;

    if (ed->selv->size > 1) {
        diff = diffstack_reserve(&ed->diff, SIZE_AGGR);
        diff->curr_checkpoint_beg = diff->curr_checkpoint_end;
        diff->curr_checkpoint_end = diff->curr_checkpoint_beg + SIZE_AGGR;
        aggr_beg_pos =  diff->curr_checkpoint_beg;
    }
    while (sel != sel_end) {
        line = ed->doc->lines + sel->line;
        if (sel->glyph_end) {
            line_beg = begin_line(line, doc);
            line_end = end_line(line, doc);
            line_curr = sync_width_or_null(line, sel->glyph_end - 1, doc);
            if (line_curr && line_curr < line_end) {
                pos = line_curr - line_beg;
                if (is_line_utf8(line, doc)) {
                    delta = next_utf8_char(line_curr) - line_curr;
                } else {
                    delta = 1;
                }
                diff_line_remove(&ed->diff, pos, line, doc, delta);
                aggr_count++;
            }
            sel->glyph_beg--;
            sel->glyph_end--;
        }
        sel++;
    }
    if (aggr_count == 0) {
        diff->curr_checkpoint_beg = diff_curr_beg;
        diff->curr_checkpoint_end = diff_curr_end;
        return;
    }
    if (ed->selv->size > 1) {
        diff = diffstack_reserve(&ed->diff, SIZE_AGGR);
        diff->curr_checkpoint_beg = aggr_beg_pos;
        diff->curr_checkpoint_end = diff->curr_checkpoint_end + SIZE_AGGR;
        diff->last_checkpoint_beg = diff->curr_checkpoint_beg;
        diff->last_checkpoint_end = diff->curr_checkpoint_end;
        diff->data[aggr_beg_pos] = DIFF_AGGREGATE;
        memcpy(diff->data + aggr_beg_pos + 1, &aggr_count, sizeof(aggr_count));
        memcpy(diff->data + diff->curr_checkpoint_end - sizeof(aggr_count) - 1
            , &aggr_count
            , sizeof(aggr_count)
        );
        diff->data[diff->curr_checkpoint_end - 1] = DIFF_AGGR_SEQ;
    }

    fill_screen_glyphs(ed, 0);
}



