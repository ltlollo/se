// This is free and unencumbered software released into the public domain.
// For more information, see LICENSE.

struct diffstack *
diffstack_reserve(struct diffstack **ds, size_t size) {
    struct diffstack *res = *ds;
    size_t alloc;

    if (res->curr_checkpoint_end + size + EMPTY_DIFF >= res->alloc) {
        alloc = res->alloc * 2 + size + EMPTY_DIFF;
        ensure(reallocflexarr((void **) ds
            , sizeof(struct diffstack)
            , alloc
            , sizeof(uint8_t)
        ));
        res = *ds;
        res->alloc = alloc;
    }
    return res;
}

struct diffstack *
diffstack_aggregate_begin(struct diffstack **diffptr
    , struct diffaggr_info *info
    ) {
    struct diffstack *diff = diffstack_reserve(diffptr, SIZE_AGGR);

    info->size = 0;
    info->old_checkpoint_beg = diff->curr_checkpoint_beg;
    info->old_checkpoint_end = diff->curr_checkpoint_end;
    diff->curr_checkpoint_beg = diff->curr_checkpoint_end;
    diff->curr_checkpoint_end = diff->curr_checkpoint_beg + SIZE_AGGR;
    info->aggregate_beg = diff->curr_checkpoint_beg;

    return diff;
}

struct diffstack *
diffstack_aggregate_end(struct diffstack **diffptr
    , struct diffaggr_info *info
    ) {
    struct diffstack *diff = *diffptr;

    if (info->size == 0) {
        diff->curr_checkpoint_beg = info->old_checkpoint_beg;
        diff->curr_checkpoint_end = info->old_checkpoint_end;
        return diff;
    }
    diff = diffstack_reserve(diffptr, SIZE_AGGR);
    diff->curr_checkpoint_beg = info->aggregate_beg;
    diff->curr_checkpoint_end = diff->curr_checkpoint_end + SIZE_AGGR;

    diff->data[info->aggregate_beg] = DIFF_AGGREGATE;
    memcpy(diff->data + info->aggregate_beg + 1
        , &info->size
        , sizeof(info->size)
    );
    memcpy(diff->data + diff->curr_checkpoint_end - sizeof(info->aggregate_beg)
        - 1
        , &info->size
        , sizeof(info->size)
    );
    diff->data[diff->curr_checkpoint_end - 1] = DIFF_AGGR_SEQ;

    diff->last_checkpoint_beg = diff->curr_checkpoint_beg;
    diff->last_checkpoint_end = diff->curr_checkpoint_end;

    return diff;
}

void
diffstack_insert_merge(struct diffstack **ds
    , struct line *line
    , size_t size
    , size_t x
    , size_t y
    ) {
    struct diffstack *res = *ds;
    size_t old_x;
    size_t old_y;
    size_t old_size;

    if (size == 0) {
        return;
    }
    res = diffstack_reserve(ds, SIZE_SPLIT);
    ensure(res);

    dbg_assert(x == 0 && y != 0);

    if (res->curr_checkpoint_beg != res->curr_checkpoint_end
        && res->data[res->curr_checkpoint_beg] == DIFF_LINE_MERGE
    ) {
        memcpy(&old_x
            , res->data + res->curr_checkpoint_beg + 1
            , sizeof(size_t)
        );
        memcpy(&old_y
            , res->data + res->curr_checkpoint_beg + 1 + sizeof(size_t)
            , sizeof(size_t)
        );
        memcpy(&old_size
            , res->data + res->curr_checkpoint_beg + 1 + 2 * sizeof(size_t)
            , sizeof(size_t)
        );
        if (old_x != 0 || old_y != y) {
            goto MERGE_DIFFERENT_DIFF_CLASS;
        }
        x = line[-1].size;
        y = y - 1;
        size += old_size;
        memcpy(res->data + res->curr_checkpoint_beg + 1
            , &x
            , sizeof(size_t)
        );
        memcpy(res->data + res->curr_checkpoint_beg + 1 + sizeof(size_t)
            , &y
            , sizeof(size_t)
        );
        memcpy(res->data + res->curr_checkpoint_beg + 1 + 2 * sizeof(size_t)
            , &size
            , sizeof(size_t)
        );
    } else {
MERGE_DIFFERENT_DIFF_CLASS:
        res->curr_checkpoint_beg = res->curr_checkpoint_end;
        res->curr_checkpoint_end = res->curr_checkpoint_beg + SIZE_SPLIT;
        res->data[res->curr_checkpoint_beg] = DIFF_LINE_MERGE;
        memcpy(res->data + res->curr_checkpoint_beg + 1, &x, sizeof(size_t));
        memcpy(res->data + res->curr_checkpoint_beg + 1 + sizeof(size_t)
            , &y
            , sizeof(size_t)
        );
        memcpy(res->data + res->curr_checkpoint_beg + 1 + 2 * sizeof(size_t)
            , &size
            , sizeof(size_t)
        );
        res->data[res->curr_checkpoint_end - 1] = DIFF_SPLIT_SEP;
    }
    res->last_checkpoint_beg = res->curr_checkpoint_beg;
    res->last_checkpoint_end = res->curr_checkpoint_end;
}

void
diffstack_insert_split(struct diffstack **ds
    , size_t size
    , size_t x
    , size_t y
    ) {
    struct diffstack *res = *ds;
    size_t old_x;
    size_t old_y;
    size_t old_size;

    if (size == 0) {
        return;
    }
    res = diffstack_reserve(ds, SIZE_SPLIT);
    ensure(res);

    if (res->curr_checkpoint_beg != res->curr_checkpoint_end
        && res->data[res->curr_checkpoint_beg] == DIFF_LINE_SPLIT
        && y != DIFF_ADD_EOD
    ) {
        memcpy(&old_x
            , res->data + res->curr_checkpoint_beg + 1
            , sizeof(size_t)
        );
        memcpy(&old_y
            , res->data + res->curr_checkpoint_beg + 1 + sizeof(size_t)
            , sizeof(size_t)
        );
        memcpy(&old_size
            , res->data + res->curr_checkpoint_beg + 1 + 2 * sizeof(size_t)
            , sizeof(size_t)
        );
        if (x != 0 || old_x != 0 || old_y + old_size != y) {
            goto SPLIT_DIFFERENT_DIFF_CLASS;
        }
        old_size += size;
        memcpy(res->data + res->curr_checkpoint_beg + 1 + 2 * sizeof(size_t)
            , &old_size
            , sizeof(size_t)
        );
    } else {
SPLIT_DIFFERENT_DIFF_CLASS:
        res->curr_checkpoint_beg = res->curr_checkpoint_end;
        res->curr_checkpoint_end = res->curr_checkpoint_beg + SIZE_SPLIT;
        res->data[res->curr_checkpoint_beg] = DIFF_LINE_SPLIT;
        memcpy(res->data + res->curr_checkpoint_beg + 1, &x, sizeof(size_t));
        memcpy(res->data + res->curr_checkpoint_beg + 1 + sizeof(size_t)
            , &y
            , sizeof(size_t)
        );
        memcpy(res->data + res->curr_checkpoint_beg + 1 + 2 * sizeof(size_t)
            , &size
            , sizeof(size_t)
        );
        res->data[res->curr_checkpoint_end - 1] = DIFF_SPLIT_SEP;
    }
    res->last_checkpoint_beg = res->curr_checkpoint_beg;
    res->last_checkpoint_end = res->curr_checkpoint_end;
}

void
diffstack_insert_chars_add(struct diffstack **ds
    , uint8_t *str
    , size_t size
    , size_t x
    , size_t y
    ) {
    struct diffstack *res = *ds;
    uint8_t *seq_new_beg = str;
    uint8_t *seq_new_end = str + size;
    uint8_t *seq_old_beg;
    uint8_t *seq_old_end;
    uint32_t glyph_old;
    uint32_t glyph_new;
    size_t old_x;
    size_t old_y;

    if (size == 0) {
        return;
    }
    res = diffstack_reserve(ds, size);
    ensure(res);

    if (res->curr_checkpoint_beg != res->curr_checkpoint_end
        && res->data[res->curr_checkpoint_beg] == DIFF_CHARS_ADD
    ) {
        seq_old_beg = res->data + res->curr_checkpoint_beg + 2
            + sizeof(size_t) * 2;
        seq_old_end = res->data + res->curr_checkpoint_end - 1;
        memcpy(&old_x, res->data + res->curr_checkpoint_beg + 1
            , sizeof(size_t)
        );
        memcpy(&old_y
            , res->data + res->curr_checkpoint_beg + 1 + sizeof(size_t)
            , sizeof(size_t)
        );
        dbg_assert(seq_old_end > seq_old_beg);
        dbg_assert(seq_old_end - seq_old_beg != 0);
        if (old_y != y || old_x + (seq_old_end - seq_old_beg) != x) {
            goto ADD_DIFFERENT_DIFF_CLASS;
        }
        glyph_old = first_glyph(seq_old_beg, seq_old_end);
        glyph_new = first_glyph(seq_new_beg, seq_new_end);
        if (!is_same_class(glyph_old, glyph_new)) {
            goto ADD_DIFFERENT_DIFF_CLASS;
        }
        dbg_assert(seq_old_end[0] == DIFF_CHAR_SEQ);
        memcpy(seq_old_end, str, size);
        seq_old_end[size] = DIFF_CHAR_SEQ;
        res->curr_checkpoint_end += size;
    } else {
ADD_DIFFERENT_DIFF_CLASS:
        res->curr_checkpoint_beg = res->curr_checkpoint_end;
        res->curr_checkpoint_end = res->curr_checkpoint_beg + EMPTY_DIFF
            + size;
        res->data[res->curr_checkpoint_beg] = DIFF_CHARS_ADD;
        memcpy(res->data + res->curr_checkpoint_beg + 1, &x, sizeof(size_t));
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
        res->data[res->curr_checkpoint_beg + EMPTY_DIFF + size - 1]
            = DIFF_CHAR_SEQ;
    }
    res->last_checkpoint_beg = res->curr_checkpoint_beg;
    res->last_checkpoint_end = res->curr_checkpoint_end;
}

void
diffstack_insert_chars_del(struct diffstack **ds
    , uint8_t *str
    , size_t size
    , size_t x
    , size_t y
    ) {
    struct diffstack *res = *ds;
    size_t x_end = x + size;;
    uint8_t *seq_new_beg = str;
    uint8_t *seq_new_end = str + size;
    uint8_t *seq_old_beg;
    uint8_t *seq_old_end;
    uint32_t glyph_old;
    uint32_t glyph_new;
    size_t old_x;
    size_t old_y;

    if (size == 0) {
        return;
    }
    res = diffstack_reserve(ds, size);
    ensure(res);

    if (res->curr_checkpoint_beg != res->curr_checkpoint_end
        && res->data[res->curr_checkpoint_beg] == DIFF_CHARS_DEL
    ) {
        seq_old_beg = res->data + res->curr_checkpoint_beg + 2
            + sizeof(size_t) * 2;
        seq_old_end = res->data + res->curr_checkpoint_end - 1;
        memcpy(&old_x, res->data + res->curr_checkpoint_beg + 1
            , sizeof(size_t)
        );
        memcpy(&old_y
            , res->data + res->curr_checkpoint_beg + 1 + sizeof(size_t)
            , sizeof(size_t)
        );
        dbg_assert(seq_old_end > seq_old_beg);
        dbg_assert(seq_old_end - seq_old_beg != 0);
        if (old_y != y
            || old_x + seq_old_end < seq_old_beg
            || old_x - (seq_old_end - seq_old_beg) != x_end
        ) {
            goto DEL_DIFFERENT_DIFF_CLASS;
        }
        glyph_old = first_glyph(seq_old_beg, seq_old_end);
        glyph_new = first_glyph(seq_new_beg, seq_new_end);
        if (!is_same_class(glyph_old, glyph_new)) {
            goto DEL_DIFFERENT_DIFF_CLASS;
        }
        dbg_assert(seq_old_end[0] == DIFF_CHAR_SEQ);
        rmemcpy(seq_old_end, str, size);
        seq_old_end[size] = DIFF_CHAR_SEQ;
        res->curr_checkpoint_end += size;
    } else {
DEL_DIFFERENT_DIFF_CLASS:
        res->curr_checkpoint_beg = res->curr_checkpoint_end;
        res->curr_checkpoint_end = res->curr_checkpoint_beg + EMPTY_DIFF
            + size;
        res->data[res->curr_checkpoint_beg] = DIFF_CHARS_DEL;
        memcpy(res->data + res->curr_checkpoint_beg + 1
            , &x_end
            , sizeof(size_t)
        );
        memcpy(res->data + res->curr_checkpoint_beg + 1 + sizeof(size_t)
            , &y
            , sizeof(size_t)
        );
        rmemcpy(res->data + res->curr_checkpoint_beg + 2 + 2 * sizeof(size_t)
            , str
            , size
        );
        res->data[res->curr_checkpoint_beg + 1 + 2 * sizeof(size_t)]
            = DIFF_CHAR_SEQ;
        res->data[res->curr_checkpoint_beg + EMPTY_DIFF + size - 1]
            = DIFF_CHAR_SEQ;
    }
    res->last_checkpoint_beg = res->curr_checkpoint_beg;
    res->last_checkpoint_end = res->curr_checkpoint_end;
}

void
diffstack_undo_line_merge(uint8_t *diff_beg
    , uint8_t *diff_end
    , struct document *doc
    ) {
    size_t x;
    size_t y;
    size_t n;
    struct line *fst;
    struct line *snd;

    dbg_assert(diff_end == diff_beg + SIZE_SPLIT);
    dbg_assert(*diff_beg == DIFF_LINE_MERGE);

    memcpy(&x, diff_beg + 1, sizeof(size_t));
    memcpy(&y, diff_beg + 1 + sizeof(size_t), sizeof(size_t));
    memcpy(&n, diff_beg + 1 + 2 * sizeof(size_t), sizeof(size_t));

    ensure(n != 0);

    fst = doc->lines + y;
    snd = fst + n;

    dbg_assert(snd < doc->lines + doc->loaded_size);
    dbg_assert(doc->loaded_size + 1 + n <= doc->alloc);

    insert_n_line(x, y, n, &doc);
}

void
diffstack_undo_line_split(uint8_t *diff_beg
    , uint8_t *diff_end
    , struct document *doc
    ) {
    size_t x;
    size_t y;
    size_t n;
    struct line *fst;
    struct line *snd;
    struct line *curr;
    struct line *end;

    dbg_assert(diff_end == diff_beg + SIZE_SPLIT);
    dbg_assert(*diff_beg == DIFF_LINE_SPLIT);

    memcpy(&x, diff_beg + 1, sizeof(size_t));
    memcpy(&y, diff_beg + 1 + sizeof(size_t), sizeof(size_t));
    memcpy(&n, diff_beg + 1 + 2 * sizeof(size_t), sizeof(size_t));

    ensure(n != 0);

    if (y == DIFF_ADD_EOD) {
        fst = doc->lines + doc->loaded_size - n;
        end = doc->lines + doc->loaded_size;
        for (curr = fst; curr != end; curr++) {
            free_line(curr, doc);
        }
        memcpy(fst, end, sizeof(struct line));
        doc->loaded_size -= n;
    } else {
        fst = doc->lines + y;
        snd = fst + n;

        merge_lines(fst, snd, doc);

        for (curr = fst + 1; curr != snd + 1; curr++) {
            free_line(curr, doc);
        }
        memcpy(fst + 1, snd + 1, (doc->loaded_size + 1 - n) * sizeof(*fst));
        doc->loaded_size -= n;
    }
}

void
diffstack_undo_chars_del(uint8_t *diff_beg
    , uint8_t *diff_end
    , struct document *doc
    ) {
    size_t x;
    size_t y;
    uint8_t *seq_beg;
    uint8_t *seq_end;
    size_t seq_size;
    struct line *line;
    uint8_t *restore_seq_beg;
    uint8_t *restore_seq_end;

    dbg_assert(diff_end > diff_beg + EMPTY_DIFF);
    dbg_assert(*diff_beg == DIFF_CHARS_DEL);

    memcpy(&x, diff_beg + 1, sizeof(size_t));
    memcpy(&y, diff_beg + 1 + sizeof(size_t), sizeof(size_t));

    seq_beg = diff_beg + 2 + 2 * sizeof(size_t);
    seq_end = diff_end - 1;
    seq_size = seq_end - seq_beg;

    dbg_assert(seq_end > seq_beg);

    line = doc->lines + y;
    ensure(!is_line_internal(line, doc));

    reserve_extern_line(line, seq_size, doc);

    restore_seq_end = line->extern_line->data + x;
    restore_seq_beg = restore_seq_end - seq_size;

    dbg_assert(restore_seq_beg >= begin_line(line, doc));
    dbg_assert(restore_seq_end <= end_line(line, doc));

    memmove(restore_seq_end, restore_seq_beg, line->size);
    rmemcpy(restore_seq_beg, seq_beg, seq_size);

    line->size += seq_size;
}

void
diffstack_undo_chars_add(uint8_t *diff_beg
    , uint8_t *diff_end
    , struct document *doc
    ) {
    size_t x;
    size_t y;
    uint8_t *seq_beg;
    uint8_t *seq_end;
    size_t seq_size;
    struct line *line;
    uint8_t *restore_seq_beg;
    uint8_t *restore_seq_end;
    uint8_t *line_end;

    dbg_assert(diff_end > diff_beg + EMPTY_DIFF);
    dbg_assert(*diff_beg == DIFF_CHARS_ADD);

    memcpy(&x, diff_beg + 1, sizeof(size_t));
    memcpy(&y, diff_beg + 1 + sizeof(size_t), sizeof(size_t));

    seq_beg = diff_beg + 2 + 2 * sizeof(size_t);
    seq_end = diff_end - 1;
    seq_size = seq_end - seq_beg;

    dbg_assert(seq_end > seq_beg);

    line = doc->lines + y;
    ensure(!is_line_internal(line, doc));

    restore_seq_beg = line->extern_line->data + x;
    restore_seq_end = restore_seq_beg + seq_size;
    line_end = line->extern_line->data + line->size;

    dbg_assert(restore_seq_beg <= line_end);
    dbg_assert(restore_seq_end <= line_end);

    memmove(restore_seq_beg, restore_seq_end, line_end - restore_seq_end);

    dbg_assert(line->size >= seq_size);
    line->size -= seq_size;
}

uint8_t *
diffstack_curr_mvback(uint8_t *curr_end) {
    uint8_t *seq_curr;
    size_t aggr_size;
    size_t i;

    switch (curr_end[-1]) {
        case DIFF_SPLIT_SEP:
            return curr_end - SIZE_SPLIT;
        case DIFF_CHAR_SEQ:
            seq_curr = curr_end - 2;
            while (*seq_curr != DIFF_CHAR_SEQ) {
                seq_curr--;
            }
            return seq_curr - (sizeof(size_t) * 2 + 1);
        case DIFF_AGGR_SEQ:
            curr_end -= SIZE_AGGR;
            memcpy(&aggr_size, curr_end, sizeof(aggr_size));
            for (i = 0; i < aggr_size; i++) {
                curr_end = diffstack_curr_mvback(curr_end);
                ensure(curr_end);
            }
            return curr_end - SIZE_AGGR;
        default:
            dbg_assert(0);
            return NULL;
    }
}

int
diffstack_undo(struct editor *ed) {
    struct diffstack *ds = ed->diff;
    struct document *doc = ed->doc;
    uint8_t *diff_beg = ds->data + ds->curr_checkpoint_beg;
    uint8_t *diff_end = ds->data + ds->curr_checkpoint_end;

    if (diff_beg == diff_end) {
        return -1;
    }
    switch (*diff_beg) {
        case DIFF_CHARS_ADD:
            diffstack_undo_chars_add(diff_beg, diff_end, doc);
            break;
        case DIFF_CHARS_DEL:
            diffstack_undo_chars_del(diff_beg, diff_end, doc);
            break;
        case DIFF_LINE_SPLIT:
            diffstack_undo_line_split(diff_beg, diff_end, doc);
            break;
        case DIFF_LINE_MERGE:
            diffstack_undo_line_merge(diff_beg, diff_end, doc);
            break;
        case DIFF_AGGREGATE:
            diffstack_undo_aggregate(diff_beg, diff_end, doc);
            break;
        default:
            ensure(0);
    }
    if (ds->curr_checkpoint_beg != 0) {
        diff_end = diff_beg;
        diff_beg = diffstack_curr_mvback(diff_end);
        ensure(diff_beg >= ds->data);
        ds->curr_checkpoint_beg = diff_beg - ds->data;
        ds->curr_checkpoint_end = diff_end - ds->data;
    } else {
        ds->curr_checkpoint_end = 0;
    }
    fill_screen_glyphs(ed, 0);
    return 0;
}

void
diffstack_undo_aggregate(uint8_t *diff_aggr_beg
    , uint8_t *diff_aggr_end
    , struct document *doc
    ) {
    uint8_t *diff_beg = diff_aggr_beg + SIZE_AGGR;
    uint8_t *diff_end = diff_aggr_end - SIZE_AGGR;
    uint8_t *diff_curr;
    size_t aggr_size;
    size_t i;

    dbg_assert(diff_aggr_beg[0] == DIFF_AGGREGATE);
    memcpy(&aggr_size, diff_aggr_beg + 1, sizeof(aggr_size));

    for (i = 0; i < aggr_size; i++) {
        diff_curr = diffstack_curr_mvback(diff_end);
        ensure(diff_curr >= diff_beg);
        switch (*diff_curr) {
            case DIFF_CHARS_ADD:
                diffstack_undo_chars_add(diff_curr, diff_end, doc);
                break;
            case DIFF_CHARS_DEL:
                diffstack_undo_chars_del(diff_curr, diff_end, doc);
                break;
            case DIFF_LINE_SPLIT:
                diffstack_undo_line_split(diff_curr, diff_end, doc);
                break;
            case DIFF_LINE_MERGE:
                diffstack_undo_line_merge(diff_curr, diff_end, doc);
                break;
            case DIFF_AGGREGATE:
                diffstack_undo_aggregate(diff_curr, diff_end, doc);
                break;
            default:
                ensure(0);
        }
        diff_end = diff_curr;
    }
    dbg_assert(diff_curr == diff_beg);
}

int
init_diffstack(struct diffstack **ds, size_t alloc) {
    struct diffstack *res;

    ensure(reallocflexarr((void **) ds
        , sizeof(struct diffstack)
        , alloc
        , sizeof(uint8_t)
    ));
    res = *ds;
    memset(res, 0, sizeof(struct diffstack));
    res->alloc = alloc;
    return 0;
}

void
diff_line_split(struct diffstack **ds
    , size_t x
    , size_t y
    , struct document **doc
    ) {
    insert_n_line(x, y, 1, doc);
    diffstack_insert_split(ds, 1, x, y);
}

void
diff_line_insert(struct diffstack **ds
    , size_t pos
    , struct line *line
    , struct document *doc
    , uint8_t *data
    , size_t size
    ) {
    uint8_t *data_curr = data;
    uint8_t *data_end = data + size;
    struct extern_line *el = reserve_line(line, size, doc);
    uint8_t *el_curr = el->data + pos;
    uint8_t *el_end = el->data + line->size;

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
    memmove(el_curr + size, el_curr, (el_end - el_curr) * sizeof(*el_curr));
    memcpy(el_curr, data, size);
    line->size += size;
    diffstack_insert_chars_add(ds, data, size, pos, line - doc->lines);
}

void
diff_line_remove(struct diffstack **ds
    , size_t pos
    , struct line *line
    , struct document *doc
    , size_t size
    ) {
    struct extern_line *el = convert_line_external(line, doc);
    uint8_t *src_beg = el->data + pos + size;
    uint8_t *src_end = el->data + line->size;
    uint8_t *dst_beg = el->data + pos;
    
    ensure(line->size >= size);
    ensure(pos <= line->size && pos + size <= line->size);

    if (next_utf8_or_null(dst_beg, src_end) == NULL) {
        el->utf8_status = UTF8_DIRTY;
    }
    if (next_utf8_or_null(src_beg, src_end) == NULL) {
        el->utf8_status = UTF8_DIRTY;
    }
    diffstack_insert_chars_del(ds, dst_beg, size, pos, line - doc->lines);
    memmove(dst_beg, src_beg, src_end - src_beg);
    line->size -= size;
}

void
diff_line_merge(struct diffstack **ds
    , size_t x
    , size_t y
    , struct document **doc
    ) {
    struct document *res = *doc;
    struct line *from = res->lines + y;
    struct line *into = res->lines + y - 1;
    struct line *trampoline_end = res->lines + res->loaded_size + 1;

    ensure(x == 0 && y > 0);

    dbg_assert(from < res->lines + res->loaded_size);

    diffstack_insert_merge(ds, from, 1, x, y);
    merge_lines(into, from, res);

    free_line(from, res);

    memmove(from, from + 1, (from + 1 - trampoline_end) * sizeof(struct line));
    res->loaded_size--;
}

