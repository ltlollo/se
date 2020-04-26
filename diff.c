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
    diff->data[info->aggregate_beg] = DIFF_AGGREGATE;

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

    ensure(diff->data[info->aggregate_beg] == DIFF_AGGREGATE);

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
    , struct diffaggr_info *info
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
    x = line[-1].size;
    y = y - 1;

    if (res->curr_checkpoint_beg != res->curr_checkpoint_end
        && res->data[res->curr_checkpoint_beg] == DIFF_LINE_MERGE
    ) {
        diffsplit_unpack(res->data + res->curr_checkpoint_beg
            , &old_x
            , &old_y
            , &old_size
        );
        if ((old_x != 0 || old_y != y + size) || 1) {
            goto MERGE_DIFFERENT_DIFF_CLASS;
        }
        old_size += size;
        diffsplit_pack(res->data + res->curr_checkpoint_beg, x, y, old_size);
    } else {
MERGE_DIFFERENT_DIFF_CLASS:
        res->curr_checkpoint_beg = res->curr_checkpoint_end;
        res->curr_checkpoint_end = res->curr_checkpoint_beg + SIZE_SPLIT;
        res->data[res->curr_checkpoint_beg] = DIFF_LINE_MERGE;
        diffsplit_pack(res->data + res->curr_checkpoint_beg, x, y, size);
        res->data[res->curr_checkpoint_end - 1] = DIFF_SPLIT_SEP;
        info->size++;
    }
    res->last_checkpoint_beg = res->curr_checkpoint_beg;
    res->last_checkpoint_end = res->curr_checkpoint_end;
}

void
diffstack_insert_split(struct diffstack **ds
    , size_t size
    , size_t x
    , size_t y
    , struct diffaggr_info *info
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
        diffsplit_unpack(res->data + res->curr_checkpoint_beg
            , &old_x
            , &old_y
            , &old_size
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
        diffsplit_pack(res->data + res->curr_checkpoint_beg, x, y, size);
        res->data[res->curr_checkpoint_end - 1] = DIFF_SPLIT_SEP;
        info->size++;
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
    , struct diffaggr_info *info
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
        seq_old_beg = res->data + res->curr_checkpoint_beg + DIFF_CHARS_OFF;
        seq_old_end = res->data + res->curr_checkpoint_end - 1;
        diffchars_unpack(res->data + res->curr_checkpoint_beg, &old_x, &old_y);
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
        diffchars_pack(res->data + res->curr_checkpoint_beg, x, y);
        memcpy(res->data + res->curr_checkpoint_beg + DIFF_CHARS_OFF
            , str
            , size
        );
        res->data[res->curr_checkpoint_beg + 1 + 2 * sizeof(size_t)]
            = DIFF_CHAR_SEQ;
        res->data[res->curr_checkpoint_beg + EMPTY_DIFF + size - 1]
            = DIFF_CHAR_SEQ;
        info->size++;
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
    , struct diffaggr_info *info
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
        seq_old_beg = res->data + res->curr_checkpoint_beg + DIFF_CHARS_OFF;
        seq_old_end = res->data + res->curr_checkpoint_end - 1;
        diffchars_unpack(res->data + res->curr_checkpoint_beg, &old_x, &old_y);
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
        diffchars_pack(res->data + res->curr_checkpoint_beg, x_end, y);
        rmemcpy(res->data + res->curr_checkpoint_beg + DIFF_CHARS_OFF
            , str
            , size
        );
        res->data[res->curr_checkpoint_beg + 1 + 2 * sizeof(size_t)]
            = DIFF_CHAR_SEQ;
        res->data[res->curr_checkpoint_beg + EMPTY_DIFF + size - 1]
            = DIFF_CHAR_SEQ;
        info->size++;
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

    diffsplit_unpack(diff_beg, &x, &y, &n);
    ensure(n != 0);

    fst = doc->lines + y;
    snd = fst + n;

    dbg_assert(snd <= doc->lines + doc->loaded_size);
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

    diffsplit_unpack(diff_beg, &x, &y, &n);
    ensure(n != 0);

    if (y == DIFF_ADD_EOD) {
        // TODO WAAAT? is DIFF_ADD_EOD a thing?
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
        remove_n_line(y + 1, n, doc);
    }
}

void
diffchars_unpack(char *diff_beg, size_t *x, size_t *y) {
    memcpy(x, diff_beg + 1 + 0 * sizeof(size_t), sizeof(size_t));
    memcpy(y, diff_beg + 1 + 1 * sizeof(size_t), sizeof(size_t));
}

void
diffchars_pack(char *diff_beg, size_t x, size_t y) {
    memcpy(diff_beg + 1 + 0 * sizeof(size_t), &x, sizeof(size_t));
    memcpy(diff_beg + 1 + 1 * sizeof(size_t), &y, sizeof(size_t));
}

void
diffsplit_unpack(char *diff_beg, size_t *x, size_t *y, size_t *n) {
    memcpy(x, diff_beg + 1 + 0 * sizeof(size_t), sizeof(size_t));
    memcpy(y, diff_beg + 1 + 1 * sizeof(size_t), sizeof(size_t));
    memcpy(n, diff_beg + 1 + 2 * sizeof(size_t), sizeof(size_t));
}

void
diffsplit_pack(char *diff_beg, size_t x, size_t y, size_t n) {
    memcpy(diff_beg + 1 + 0 * sizeof(size_t), &x, sizeof(size_t));
    memcpy(diff_beg + 1 + 1 * sizeof(size_t), &y, sizeof(size_t));
    memcpy(diff_beg + 1 + 2 * sizeof(size_t), &n, sizeof(size_t));
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

    diffchars_unpack(diff_beg, &x, &y);
    seq_beg = diff_beg + DIFF_CHARS_OFF;
    seq_end = diff_end - 1;
    seq_size = seq_end - seq_beg;

    dbg_assert(seq_end > seq_beg);

    line = doc->lines + y;
    ensure(!is_line_internal(line, doc));

    reserve_extern_line(line, seq_size, doc);

    restore_seq_end = line->extern_line->data + x;
    restore_seq_beg = restore_seq_end - seq_size;

    dbg_assert(restore_seq_beg >= begin_line(line, doc));
    dbg_assert(restore_seq_end <= begin_line(line, doc) + line->alloc);

    memmove(restore_seq_end, restore_seq_beg, line->size - x + seq_size);
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

    diffchars_unpack(diff_beg, &x, &y);
    seq_beg = diff_beg + DIFF_CHARS_OFF;
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


void
diffstack_redo_chars_add(uint8_t *diff_beg
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
    dbg_assert(*diff_beg == DIFF_CHARS_ADD);

    diffchars_unpack(diff_beg, &x, &y);
    seq_beg = diff_beg + DIFF_CHARS_OFF;
    seq_end = diff_end - 1;
    seq_size = seq_end - seq_beg;

    dbg_assert(seq_end > seq_beg);

    line = doc->lines + y;
    ensure(!is_line_internal(line, doc));

    reserve_extern_line(line, seq_size, doc);

    restore_seq_beg = line->extern_line->data + x;
    restore_seq_end = restore_seq_beg + seq_size;

    dbg_assert(restore_seq_end >= begin_line(line, doc));
    dbg_assert(restore_seq_beg <= end_line(line, doc));

    memmove(restore_seq_end, restore_seq_beg, line->size - x);
    memcpy(restore_seq_beg, seq_beg, seq_size);

    line->size += seq_size;
}

void
diffstack_redo_chars_del(uint8_t *diff_beg
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
    dbg_assert(*diff_beg == DIFF_CHARS_DEL);

    diffchars_unpack(diff_beg, &x, &y);
    seq_beg = diff_beg + DIFF_CHARS_OFF;
    seq_end = diff_end - 1;
    seq_size = seq_end - seq_beg;

    dbg_assert(seq_end > seq_beg);

    line = doc->lines + y;
    ensure(!is_line_internal(line, doc));

    restore_seq_end = line->extern_line->data + x;
    restore_seq_beg = restore_seq_end - seq_size;
    line_end = line->extern_line->data + line->size;

    dbg_assert(restore_seq_beg <= line_end);
    dbg_assert(restore_seq_end <= line_end);

    memmove(restore_seq_beg, restore_seq_end, line_end - restore_seq_end);

    dbg_assert(line->size >= seq_size);
    line->size -= seq_size;
}

void
diffstack_redo_line_split(uint8_t *diff_beg
    , uint8_t *diff_end
    , struct document *doc
    ) {
    size_t x;
    size_t y;
    size_t n;
    struct line *fst;
    struct line *snd;

    dbg_assert(diff_end == diff_beg + SIZE_SPLIT);
    dbg_assert(*diff_beg == DIFF_LINE_SPLIT);

    diffsplit_unpack(diff_beg, &x, &y, &n);
    ensure(n != 0);

    fst = doc->lines + y;
    snd = fst + n;

    dbg_assert(snd <= doc->lines + doc->loaded_size);
    dbg_assert(doc->loaded_size + 1 + n <= doc->alloc);

    insert_n_line(x, y, n, &doc);
}

void
diffstack_redo_line_merge(uint8_t *diff_beg
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
    dbg_assert(*diff_beg == DIFF_LINE_MERGE);

    diffsplit_unpack(diff_beg, &x, &y, &n);
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
        remove_n_line(y + 1, n, doc);
    }
}

void
diffstack_redo_aggregate(uint8_t *diff_aggr_beg
    , uint8_t *diff_aggr_end
    , struct document *doc
    ) {
    uint8_t *diff_beg = diff_aggr_beg + SIZE_AGGR;
    uint8_t *diff_end = diff_aggr_end - SIZE_AGGR;
    uint8_t *diff_curr;
    uint8_t *diff_curr_end;
    size_t aggr_size;
    size_t i;

    dbg_assert(diff_aggr_beg[0] == DIFF_AGGREGATE);
    memcpy(&aggr_size, diff_aggr_beg + 1, sizeof(aggr_size));

    diff_curr = diff_beg;
    diff_curr_end = diff_curr;

    for (i = 0; i < aggr_size; i++) {
        diff_curr_end  = diffstack_curr_mvforw(diff_curr);
        ensure(diff_curr <= diff_curr_end);
        ensure(diff_curr_end <= diff_end);
        switch (*diff_curr) {
            case DIFF_CHARS_ADD:
                diffstack_redo_chars_add(diff_curr, diff_curr_end, doc);
                break;
            case DIFF_CHARS_DEL:
                diffstack_redo_chars_del(diff_curr, diff_curr_end, doc);
                break;
            case DIFF_LINE_SPLIT:
                diffstack_redo_line_split(diff_curr, diff_curr_end, doc);
                break;
            case DIFF_LINE_MERGE:
                diffstack_redo_line_merge(diff_curr, diff_curr_end, doc);
                break;
            case DIFF_AGGREGATE:
                diffstack_redo_aggregate(diff_curr, diff_curr_end, doc);
                break;
            default:
                ensure(0);
        }
        diff_curr = diff_curr_end;
    }
    dbg_assert(diff_curr_end == diff_end);
}

uint8_t *
diffstack_curr_mvback(uint8_t *curr_end) {
    uint8_t *seq_curr;
    size_t aggr_size;
    size_t i;

    switch (curr_end[-1]) {
        case DIFF_CHAR_SEQ:
            seq_curr = curr_end - 2;
            while (*seq_curr != DIFF_CHAR_SEQ) {
                seq_curr--;
            }
            return seq_curr - (sizeof(size_t) * 2 + 1);
        case DIFF_SPLIT_SEP:
            return curr_end - SIZE_SPLIT;
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

uint8_t *
diffstack_curr_mvforw(uint8_t *curr_end) {
    uint8_t *seq_curr;
    size_t aggr_size;
    size_t i;

    switch (*curr_end) {
        case DIFF_CHARS_ADD:    case DIFF_CHARS_DEL:
            seq_curr = curr_end + EMPTY_DIFF - 1;
            while (*seq_curr != DIFF_CHAR_SEQ) {
                seq_curr++;
            }
            return seq_curr + 1;
        case DIFF_LINE_SPLIT:   case DIFF_LINE_MERGE:
            return curr_end + SIZE_SPLIT;
        case DIFF_AGGREGATE:
            memcpy(&aggr_size, curr_end + 1, sizeof(aggr_size));
            curr_end += SIZE_AGGR;
            for (i = 0; i < aggr_size; i++) {
                curr_end = diffstack_curr_mvforw(curr_end);
                ensure(curr_end);
            }
            return curr_end + SIZE_AGGR;
        default:
            dbg_assert(0);
            return NULL;
    }
}

uint8_t *
diffaggr_first_simple(uint8_t *diff_beg) {
    size_t aggr_size;

    ensure(*diff_beg == DIFF_AGGREGATE);
    memcpy(&aggr_size, diff_beg + 1, sizeof(aggr_size));

    diff_beg = diff_beg + SIZE_AGGR;
    for (size_t i = 0; i < aggr_size; i++) {
        switch (*diff_beg) {
            case DIFF_CHARS_ADD: case DIFF_CHARS_DEL: case DIFF_LINE_SPLIT:
            case DIFF_LINE_MERGE:
                return diff_beg;
            case DIFF_AGGREGATE:
                return diffaggr_first_simple(diff_beg);
            default:
                ensure(0);
        }
    }
    return NULL;
}

uint8_t *
diffaggr_last_simple(uint8_t *diff_end) {
    size_t aggr_size;

    ensure(diff_end[-1] == DIFF_AGGR_SEQ);
    diff_end -= SIZE_AGGR;
    memcpy(&aggr_size, diff_end, sizeof(aggr_size));

    for (size_t i = 0; i < aggr_size; i++) {
        switch (diff_end[-1]) {
            case DIFF_CHAR_SEQ: case DIFF_SPLIT_SEP:
                return diffstack_curr_mvback(diff_end);
            case DIFF_AGGR_SEQ:
                return diffaggr_last_simple(diff_end);
            default:
                ensure(0);
        }
    }
    return NULL;
}

int
reposition_cursor_undo(struct editor *ed
    , uint8_t *diff_beg
    , uint8_t *diff_end
) {
    struct selectarr *selv = ed->selv;
    struct line *line;
    uint8_t *seq_beg = diff_beg + DIFF_CHARS_OFF;
    uint8_t *seq_end = diff_end - 1;
    size_t diff_size = seq_end - seq_beg;
    size_t x;
    size_t y;

    if (diff_beg == diff_end) {
        return -1;
    }
    if (*diff_beg == DIFF_AGGREGATE) {
        diff_beg = diffaggr_first_simple(diff_beg);
        if (diff_beg == NULL) {
            return -1;
        } else {
            diff_end = diffstack_curr_mvforw(diff_beg);
            seq_beg = diff_beg + DIFF_CHARS_OFF;
            seq_end = diff_end - 1;
            diff_size = seq_end - seq_beg;
        }
    }
    switch (*diff_beg) {
        case DIFF_CHARS_DEL:
            diffchars_unpack(diff_beg, &x, &y);
            line = ed->doc->lines + y;
            ensure(y < ed->doc->loaded_size);
            x = glyphs_in_line_width(line, ed->doc, x - diff_size);
            if (is_line_utf8(line, ed->doc)) {
                x = x + glyphs_in_utf8_revspan(seq_beg, seq_end);
            } else {
                x = x + diff_size;
            }
            selv->size = 1;
            selv->focus = selv->data;
            selv->focus->line = y;
            selv->focus->glyph_beg = x;
            selv->focus->glyph_end = x;
            break;
        case DIFF_CHARS_ADD:
        case DIFF_LINE_SPLIT:
            diffchars_unpack(diff_beg, &x, &y);
            line = ed->doc->lines + y;
            ensure(y < ed->doc->loaded_size);
            x = glyphs_in_line_width(line, ed->doc, x);
            selv->size = 1;
            selv->focus = selv->data;
            selv->focus->line = y;
            selv->focus->glyph_beg = x;
            selv->focus->glyph_end = x;
            break;
        case DIFF_LINE_MERGE:
            diffchars_unpack(diff_beg, &x, &y);
            selv->size = 1;
            selv->focus = selv->data;
            selv->focus->line = y + 1;
            selv->focus->glyph_beg = 0;
            selv->focus->glyph_end = 0;
            break;
        default:
            ensure(0);
            break;
    }
    return 0;
}

void
show_revseq(char *seq_beg, char *seq_end) {
    size_t delta = seq_end - seq_beg;
    uint8_t seq[4];
    size_t i;
    size_t j;

    for (i = 0; i < delta / 4; i++) {
        for (j = 0; j < 4; j++) {
            seq[j] = seq_end[-(4 * i + j + 1)];
        }
        fwrite(seq, 1, 4, stderr);
    }
    for (j = 0; j < delta % 4; j++) {
        seq[j] = seq_end[-(4 * i + j + 1)];
    }
    fwrite(seq, 1, delta % 4, stderr);
}

void
show_seq(char *seq_beg, char *seq_end) {
    fwrite(seq_beg, 1, seq_end - seq_beg, stderr);
}

void
diff_show(uint8_t *diff_beg, uint8_t *diff_end, size_t indent) {
    uint8_t *seq_beg = diff_beg + DIFF_CHARS_OFF;
    uint8_t *seq_end = diff_end - 1;
    size_t aggr_size;
    size_t x;
    size_t y;
    size_t n;

    if (indent) {
        fprintf(stderr, "%*s", (int)indent, "  ");
    }
    switch (*diff_beg) {
        case DIFF_CHARS_ADD:
            diffchars_unpack(diff_beg, &x, &y);
            fprintf(stderr, "add { x: %5.lu, y: %5.lu, p: ", x, y);
            show_seq(seq_beg, seq_end);
            fputs("}\n", stderr);
            break;
        case DIFF_CHARS_DEL:
            diffchars_unpack(diff_beg, &x, &y);
            fprintf(stderr, "add { x: %5.lu, y: %5.lu, p: ", x, y);
            show_revseq(seq_beg, seq_end);
            fputs("}\n", stderr);
            break;
        case DIFF_LINE_SPLIT:
            diffsplit_unpack(diff_beg, &x, &y, &n);
            fprintf(stderr, "spl { x: %5.lu, y: %5.lu, n: %5.lu}\n", x, y, n);
            break;
        case DIFF_LINE_MERGE:
            diffsplit_unpack(diff_beg, &x, &y, &n);
            fprintf(stderr, "mgr { x: %5.lu, y: %5.lu, n: %5.lu}\n", x, y, n);
            break;
        case DIFF_AGGREGATE:
            memcpy(&aggr_size, diff_beg + 1, sizeof(aggr_size));
            diff_beg = diff_beg + SIZE_AGGR;
            fprintf(stderr, "beg { size: %5.lu }\n", aggr_size);
            for (size_t i = 0; i < aggr_size; i++) {
                uint8_t *diff_end = diffstack_curr_mvforw(diff_beg);
                diff_show(diff_beg, diff_end, indent + 1);
                diff_beg = diff_end;
            }
            fprintf(stderr, "end { size: %5.lu }\n", aggr_size);
            break;
        default:
            ensure(0);
            break;
    }
}

void
diffstack_show(const char *msg, struct diffstack *ds) {
    uint8_t *diff_beg = ds->data + 0;
    uint8_t *diff_end = ds->data + ds->curr_checkpoint_end;
    uint8_t *diff_curr_end;

    fprintf(stderr, "diff: === %s ===\n", msg);

    while (diff_beg != diff_end) {
        ensure(diff_beg < diff_end);
        diff_curr_end = diffstack_curr_mvforw(diff_beg);
        diff_show(diff_beg, diff_curr_end, 0);
        diff_beg = diff_curr_end;
    }
}

int
reposition_cursor_redo(struct editor *ed
    , uint8_t *diff_beg
    , uint8_t *diff_end
) {
    struct selectarr *selv = ed->selv;
    uint8_t *seq_beg = diff_beg + DIFF_CHARS_OFF;
    uint8_t *seq_end = diff_end - 1;
    size_t diff_size = seq_end - seq_beg;
    struct  line *line;
    size_t x;
    size_t y;

    if (diff_beg == diff_end) {
        return -1;
    }
    if (*diff_beg == DIFF_AGGREGATE) {
        diff_beg = diffaggr_last_simple(diff_end);
        if (diff_beg == NULL) {
            return -1;
        }
        diff_end = diffstack_curr_mvforw(diff_beg);
        seq_beg = diff_beg + DIFF_CHARS_OFF;
        seq_end = diff_end - 1;
        diff_size = seq_end - seq_beg;
    }
    switch (*diff_beg) {
        case DIFF_CHARS_ADD:
            diffchars_unpack(diff_beg, &x, &y);
            selv->size = 1;
            selv->focus = selv->data;
            selv->focus->line = y;
            line = ed->doc->lines + y;
            ensure(y < ed->doc->loaded_size);

            x = glyphs_in_line_width(line, ed->doc, x);
            if (is_line_utf8(line, ed->doc)) {
                x = x + glyphs_in_utf8_span(seq_beg, seq_end);
            } else {
                x = x + diff_size;
            }
            selv->focus->glyph_beg = x;
            selv->focus->glyph_end = x;
            break;
        case DIFF_CHARS_DEL:
            diffchars_unpack(diff_beg, &x, &y);
            selv->size = 1;
            selv->focus = selv->data;
            selv->focus->line = y;
            line = ed->doc->lines + y;
            ensure(y < ed->doc->loaded_size);

            if (is_line_utf8(line, ed->doc)) {
                x = glyphs_in_line_width(line, ed->doc, x - diff_size);
            } else {
                x = x - diff_size;
            }
            selv->focus->glyph_beg = x;
            selv->focus->glyph_end = x;
            break;
        case DIFF_LINE_SPLIT:
            diffchars_unpack(diff_beg, &x, &y);
            selv->size = 1;
            selv->focus = selv->data;
            selv->focus->line = y + 1;
            selv->focus->glyph_beg = 0;
            selv->focus->glyph_end = 0;
            break;
        case DIFF_LINE_MERGE:
            diffchars_unpack(diff_beg, &x, &y);
            selv->size = 1;
            selv->focus = selv->data;
            selv->focus->line = y;
            line = ed->doc->lines + y;
            ensure(y < ed->doc->loaded_size);
            x = glyphs_in_line_width(line, ed->doc, x);

            selv->focus->glyph_beg = x;
            selv->focus->glyph_end = x;
            break;
        default:
            ensure(0);
            break;
    }
    // TODO add assert focus in bounds
    return 0;
}

int
diffstack_undo(struct editor *ed) {
    struct diffstack *ds = ed->diff;
    struct document *doc = ed->doc;
    uint8_t *diff_beg = ds->data + ds->curr_checkpoint_beg;
    uint8_t *diff_end = ds->data + ds->curr_checkpoint_end;

    dbg(diffstack_show("undo before", ed->diff));

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
    reposition_cursor_undo(ed, diff_beg, diff_end);

    if (ds->curr_checkpoint_beg != 0) {
        diff_end = diff_beg;
        diff_beg = diffstack_curr_mvback(diff_end);
        ensure(diff_beg >= ds->data);
        ds->curr_checkpoint_beg = diff_beg - ds->data;
        ds->curr_checkpoint_end = diff_end - ds->data;
    } else {
        ds->curr_checkpoint_end = 0;
    }
    win_dmg_from_lineno(ed->win, 0);

    dbg(diffstack_show("undo after", ed->diff));
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
diffstack_redo(struct editor *ed) {
    struct diffstack *ds = ed->diff;
    struct document *doc = ed->doc;
    uint8_t *diff_beg = ds->data + ds->curr_checkpoint_beg;
    uint8_t *diff_end = ds->data + ds->curr_checkpoint_end;
    uint8_t *last_diff_beg = ds->data + ds->last_checkpoint_beg;
    uint8_t *last_diff_end = ds->data + ds->last_checkpoint_end;

    dbg(diffstack_show("redo before", ed->diff));

    if (diff_end == last_diff_end) {
        dbg_assert(diff_beg == last_diff_beg);
        return -1;
    }
    diff_beg = diff_end;
    diff_end = diffstack_curr_mvforw(diff_end);
    ensure(diff_end <= last_diff_end);

    reposition_cursor_redo(ed, diff_beg, diff_end);

    switch (*diff_beg) {
        case DIFF_CHARS_ADD:
            diffstack_redo_chars_add(diff_beg, diff_end, doc);
            break;
        case DIFF_CHARS_DEL:
            diffstack_redo_chars_del(diff_beg, diff_end, doc);
            break;
        case DIFF_LINE_SPLIT:
            diffstack_redo_line_split(diff_beg, diff_end, doc);
            break;
        case DIFF_LINE_MERGE:
            diffstack_redo_line_merge(diff_beg, diff_end, doc);
            break;
        case DIFF_AGGREGATE:
            diffstack_redo_aggregate(diff_beg, diff_end, doc);
            break;
        default:
            ensure(0);
    }
    ds->curr_checkpoint_beg = diff_beg - ds->data;
    ds->curr_checkpoint_end = diff_end - ds->data;
    win_dmg_from_lineno(ed->win, 0);

    dbg(diffstack_show("redo after", ed->diff));
    return 0;
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
    , struct diffaggr_info *info
    ) {
    insert_n_line(x, y, 1, doc);
    diffstack_insert_split(ds, 1, x, y, info);
}

void
diff_line_insert(struct diffstack **ds
    , size_t pos
    , struct line *line
    , struct document *doc
    , uint8_t *data
    , size_t size
    , struct diffaggr_info *info
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
    diffstack_insert_chars_add(ds
        , data
        , size
        , pos
        , line - doc->lines
        , info
    );
}

void
diff_line_remove(struct diffstack **ds
    , size_t pos
    , struct line *line
    , struct document *doc
    , size_t size
    , struct diffaggr_info *info
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
    diffstack_insert_chars_del(ds
        , dst_beg
        , size
        , pos
        , line - doc->lines
        , info
    );
    memmove(dst_beg, src_beg, src_end - src_beg);
    line->size -= size;
}

void
diff_line_merge(struct diffstack **ds
    , size_t x
    , size_t y
    , struct document **doc
    , struct diffaggr_info *info
    ) {
    struct document *res = *doc;
    struct line *from = res->lines + y;
    struct line *into = res->lines + y - 1;
    struct line *trampoline_end = res->lines + res->loaded_size + 1;

    ensure(x == 0 && y > 0);

    dbg_assert(from < res->lines + res->loaded_size);

    diffstack_insert_merge(ds, from, 1, x, y, info);
    merge_lines(into, from, res);

    free_line(from, res);

    memmove(from, from + 1, (trampoline_end - from - 1) * sizeof(struct line));
    res->loaded_size--;
}

