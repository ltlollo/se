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

    if (mod & GLUT_ACTIVE_CTRL) {
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
            if ((mod & GLUT_ACTIVE_SHIFT) == 0) {
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
        if ((mod & GLUT_ACTIVE_SHIFT) == 0) {
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

    if (mod & GLUT_ACTIVE_CTRL) {
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
            if ((mod & GLUT_ACTIVE_SHIFT) == 0) {
                sel_curr->glyph_beg -= min(span, sel_curr->glyph_beg);
            }
            if (sel_curr->glyph_beg > sel_curr->glyph_end) {
                sel_curr->glyph_beg = sel_curr->glyph_end;
            }
            sel_curr++;
        }
    } else {
        sel_curr = selv->data;
        while (sel_curr != selv->data + selv->size) {
            if (sel_curr->glyph_end) {
                sel_curr->glyph_end--;
            }
            sel_curr++;
        }
        if ((mod & GLUT_ACTIVE_SHIFT) == 0) {
            sel_curr = selv->data;
            while (sel_curr != selv->data + selv->size) {
                if (sel_curr->glyph_beg) {
                    sel_curr->glyph_beg--;
                }
                sel_curr++;
            }
        }
    }
}

void
key_special_input(int key, int mx __unused, int my __unused) {
    int mod = glutGetModifiers();
    struct selection *sel_curr;
    size_t delta;

    switch (key) {
        case GLUT_KEY_DOWN:
            load_lines(1, &doc);
            selv->focus = selv->data + selv->size - 1;
            if (doc->loaded_size == selv->focus->line) {
                break;
            }
            if (mod & GLUT_ACTIVE_SHIFT) {
                selv = reserve_selectarr(&selv, 1);
                memcpy(selv->data + selv->size
                    , selv->data + selv->size - 1
                    , sizeof(struct selection)
                );
                selv->focus = selv->data + selv->size++;
                selv->focus->line++;
            } else {
                sel_curr = selv->data;
                while (sel_curr != selv->data + selv->size) {
                    sel_curr->line++;
                    sel_curr++;
                }
            }
            break;
        case GLUT_KEY_UP:
            if (mod & GLUT_ACTIVE_SHIFT) {
                if (selv->size == 1) {
                    break;
                }
                if (selv->focus == selv->data + selv->size - 1) {
                    selv->focus--;
                }
                selv->size--;
            } else {
                selv->focus = selv->data;
                if (selv->focus->line == 0) {
                    break;
                }
                sel_curr = selv->data;
                while (sel_curr != selv->data + selv->size) {
                    sel_curr->line--;
                    sel_curr++;
                }
            }
            break;
        case GLUT_KEY_RIGHT:
            move_cursors_right(selv, mod, doc);
            break;
        case GLUT_KEY_LEFT:
            move_cursors_left(selv, mod, doc);
            break;
        case GLUT_KEY_PAGE_DOWN:
            selv->focus = selv->data + selv->size - 1;
            load_lines(win.height, &doc);
            delta = min(doc->loaded_size - selv->focus->line, win.height);
            sel_curr = selv->data;
            while (sel_curr != selv->data + selv->size) {
                sel_curr->line += delta;
                sel_curr++;
            }
            break;
        case GLUT_KEY_PAGE_UP:
            selv->focus = selv->data;
            delta = min(selv->focus->line, win.height);
            sel_curr = selv->data;
            while (sel_curr != selv->data + selv->size) {
                sel_curr->line -= delta;
                sel_curr++;
            }
            break;
        default:
            break;
    }
    screen_reposition(&win, &doc, selv->focus->line, selv->focus->glyph_end);
    fill_screen_colors(doc, &win, selv, 0);
    gl_buffers_upload(&win);
    glutPostRedisplay();
}


void
key_input(unsigned char key, int x __unused, int y __unused) {

    switch (key) {
        case 'd':
            diff_line_insert(&diff, 0, doc->lines, doc, "asd", 3);
            break;
        case 's':
            diff_line_remove(&diff, 0, doc->lines, doc, 3);
            break;
        case 'u':
            diffstack_undo(diff, doc);
            break;
        case 'p':
            diff_line_split(&diff, 0, 0, &doc);
            break;
        case 'm':
            diff_line_merge(&diff, 0, 1, &doc);
            break;
        case 'q': case 27:
            exit(0);
        default:
            return;
    }
    fill_screen(&doc, &win, 0);
    gl_buffers_upload(&win);
    glutPostRedisplay();
}


