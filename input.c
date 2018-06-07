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
add_to_cursor(struct editor *ed
    , struct selection *sel
    , struct line *line
    , uint8_t *buf
    , size_t bufsz
    , struct diffaggr_info *aggr_info
    ) {
    uint8_t *line_beg;
    uint8_t *line_curr;
    size_t pos;

    line = ed->doc->lines + sel->line;
    line_beg = begin_line(line, ed->doc);
    line_curr = sync_width_or_null(line, sel->glyph_end, ed->doc);
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
        aggr_info->size++;
    }
}

void
key_insert_chars(struct editor *ed, int key, int mod) {
    // beware to not cache ed members, or reload them when necessay
    struct selection *sel = ed->selv->data;
    struct selection *sel_end = ed->selv->data + ed->selv->size;
    struct selection *sel_fin = sel_end - 1;
    struct diffaggr_info aggr_info;
    struct line *line;
    uint8_t buf[4];
    size_t bufsz;

    if (mod & KMOD_LCTRL) {
        return;
    }
    buf[0] = key & 0x7f;
    bufsz = 1;

    if (ed->selv->size == 1) {
        if (sel->line != ed->doc->loaded_size
            && sel->glyph_beg == sel->glyph_end
        ) {
            line = ed->doc->lines + sel->line;
            add_to_cursor(ed, sel, line, buf, bufsz, &aggr_info);
        } else {
            diffstack_aggregate_begin(&ed->diff, &aggr_info);
            if (sel->line == sel->glyph_end) {
                diff_line_split(&ed->diff, 0, DIFF_ADD_EOD, &ed->doc);
                aggr_info.size++;
            } else if (sel->glyph_beg != sel->glyph_end) {
                delete_selection(ed, sel, &aggr_info);
            }
            line = ed->doc->lines + sel->line;
            add_to_cursor(ed, sel, line, buf, bufsz, &aggr_info);
            diffstack_aggregate_end(&ed->diff, &aggr_info);
        }
    } else {
        diffstack_aggregate_begin(&ed->diff, &aggr_info);
        if (sel_fin->line == ed->doc->loaded_size) {
            diff_line_split(&ed->diff, 0, DIFF_ADD_EOD, &ed->doc);
            aggr_info.size++;
        }
        for (sel = ed->selv->data; sel != sel_end; sel++) {
            if (sel->glyph_beg != sel->glyph_end) {
                delete_selection(ed, sel, &aggr_info);
            }
        }
        for (sel = ed->selv->data; sel != sel_end; sel++) {
            line = ed->doc->lines + sel->line;
            add_to_cursor(ed, sel, line, buf, bufsz, &aggr_info);
        }
        diffstack_aggregate_end(&ed->diff, &aggr_info);
    }
    fill_screen_glyphs(ed, 0);
};

void
delete_selection(struct editor *ed
    , struct selection *sel
    , struct diffaggr_info *aggr_info
    ) {
    struct document *doc = ed->doc;
    struct line *line = ed->doc->lines + sel->line;
    uint8_t *line_beg;
    uint8_t *line_end;
    uint8_t *line_curr;
    uint8_t *line_del_beg;
    size_t pos;
    size_t delta;
    size_t i;

    dbg_assert(line < doc->lines + doc->loaded_size);
    dbg_assert(sel->glyph_beg != sel->glyph_end);

    line_beg = begin_line(line, doc);
    line_end = end_line(line, doc);
    line_curr = sync_width_or_null(line, sel->glyph_beg, doc);
    line_del_beg = line_curr;
    if (line_curr && line_curr < line_end) {
        pos = line_curr - line_beg;
        if (!is_line_utf8(line, doc)) {
            delta = min(line_end
                , line_curr + (sel->glyph_end - sel->glyph_beg)
            ) - line_curr;
        } else {
            delta = 0;
            i = sel->glyph_beg;
            while (line_curr != line_end && i != sel->glyph_end) {
                line_curr  = next_utf8_char(line_curr);
                i++;
            }
            dbg_assert(i <= sel->glyph_end);
            delta = line_curr - line_del_beg;
        }
        diff_line_remove(&ed->diff, pos, line, doc, delta);
        sel->glyph_end = sel->glyph_beg;
        aggr_info->size++;
    }
}

void
delete_cursor(struct editor *ed
    , struct selection *sel
    , struct diffaggr_info *aggr_info
    ) {
    struct document *doc = ed->doc;
    struct line *line = ed->doc->lines + sel->line;
    uint8_t *line_beg;
    uint8_t *line_end;
    uint8_t *line_curr;
    size_t pos;
    size_t delta;

    dbg_assert(sel->glyph_end);
    dbg_assert(line < doc->lines + doc->loaded_size);

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
        sel->glyph_beg--;
        sel->glyph_end--;
        aggr_info->size++;
    }
}

void
key_backspace(struct editor *ed) {
    struct selection *sel = ed->selv->data;
    struct selection *sel_end = ed->selv->data + ed->selv->size;
    struct diffaggr_info aggr_info = { .size = 0 };

    if (ed->selv->size == 1) {
        if (sel->glyph_beg != sel->glyph_end) {
            delete_selection(ed, sel, &aggr_info);
        } else if (sel->glyph_end) {
            delete_cursor(ed, sel, &aggr_info);
        }
    } else {
        diffstack_aggregate_begin(&ed->diff, &aggr_info);
        for (sel = ed->selv->data; sel != sel_end; sel++) {
            if (sel->glyph_beg != sel->glyph_end) {
                delete_selection(ed, sel, &aggr_info);
            }
        }
        if (aggr_info.size == 0) {
            for (sel = ed->selv->data; sel != sel_end; sel++) {
                if (sel->glyph_end) {
                    delete_cursor(ed, sel, &aggr_info);
                }
            }
        }
        diffstack_aggregate_end(&ed->diff, &aggr_info);
    }
    if (aggr_info.size) {
        fill_screen_glyphs(ed, 0);
    }
}



