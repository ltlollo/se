// This is free and unencumbered software released into the public domain.
// For more information, see LICENSE.

#include <stdint.h>
#include <stddef.h>
#include <err.h>
#include "fio.h"

int
conf_add(struct conf_file *conf
    , char key[static KEY_LEN]
    , char val[static VAL_LEN]
) {
    if (conf->size == CONF_LEN) {
        warnx("conf full");
        return -1;
    }
    memcpy(conf->data[conf->size].key, key, KEY_LEN);
    memcpy(conf->data[conf->size].val, val, VAL_LEN);
    conf->size++;

    return 0;
}

struct conf_data *
conf_find(struct conf_file *conf, char *key) {
    for (struct conf_data *i = conf->data; i < conf->data + conf->size; i++) {
        if (strcmp(i->key, key) == 0) {
            return i;
        }
    }
    return NULL;
}

int
conf_fill(struct conf_file *conf) {
    char *beg = conf->file.data;
    char *end = conf->file.data + conf->file.size;
    char key[KEY_LEN];
    char val[VAL_LEN];
    char *memb_beg;
    char *memb_end;
    size_t memb_size;
    size_t line = 0;
    char *endl;

    for (; beg < end; line++, beg = endl + 1) {
        endl = beg;

        while (endl < end && *endl != '\n') {
            endl++;
        }
        if (*beg == '#') {
            continue;
        }
        memb_beg = memb_end = beg;
        
        while (memb_end < endl && *memb_end != '=' && *memb_end != ' ') {
            memb_end++;
        }
        memb_size = memb_end - memb_beg;

        if (memb_size > sizeof(key) - 1) {
            warnx("conf: error key too long %s:%lu:%lu", conf->path
                , line + 1, memb_beg - beg + 1
            );
            return -1;
        }
        if (memb_size == 0) {
            warnx("conf: error key zero sized %s:%lu:%lu", conf->path
                , line + 1, memb_beg - beg + 1
            );
            continue;
        }
        for (char *c = memb_beg; c < memb_end; c++) {
            if (*c == '\0') {
                warnx("conf: fatal error found '0' %s:%lu:%lu", conf->path
                    , line + 1, c - beg + 1
                );
                return -1;
            }
        }
        memcpy(key, memb_beg, memb_size);
        memset(key + memb_size, 0, sizeof(key) - memb_size);

        for (memb_beg = memb_end
            ; memb_beg < endl && *memb_beg == ' '
            ; memb_beg++
        );
        if (memb_beg + 1 > endl || *memb_beg != '=') {
            warnx("conf: missing '=' %s:%lu:%lu", conf->path, line + 1
                , memb_beg - beg + 1
            );
            continue;
        }
        for (memb_beg = memb_beg + 1
            ; memb_beg < endl && *memb_beg == ' '
            ; memb_beg++
        );
        memb_end = endl;
        memb_size = memb_end - memb_beg;

        if (memb_size == 0) {
            warnx("conf: zero sized member, please use = {} %s:%lu:%lu"
                , conf->path, line + 1, memb_beg - beg + 1
            );
            continue;
        }
        while (memb_end[-1] == ' ') {
            memb_end--;
        }
        if (*memb_beg == '{' && memb_end[-1] == '}') {
            memb_beg++;
            memb_end--;
        }
        memb_size = memb_end - memb_beg;

        if (memb_size > sizeof(val) - 1) {
            warnx("conf: fatal error val too long %s:%lu:%lu", conf->path
                , line + 1, memb_beg - beg + 1
            );
            return -1;
        }
        for (char *c = memb_beg; c < memb_end; c++) {
            if (*c == '\0') {
                warnx("conf: fatal found '0' %s:%lu:%lu", conf->path, line + 1
                    , c - beg + 1
                );
                return -1;
            }
        }
        memcpy(val, memb_beg, memb_size);
        memset(val + memb_size, 0, sizeof(val) - memb_size);

        warnx("conf: %s = '%s'", key, val);

        if (conf_find(conf, key) != NULL) {
            warnx("conf: error duplicate key '%s' %s:%lu:%lu", key, conf->path
                , line +1, memb_beg - beg + 1
            );
            continue;
        }
        conf_add(conf, key, val);
    }
    return 0;
}

void
init_conf_file(struct conf_file *conf) {
    char *home = getenv("HOME");

    if (home == NULL) {
        warnx("conf: fatal error cannot read $HOME");
        return;
    }
    int res = snprintf(conf->path, sizeof(conf->path), "%s/%s", home
        , ".se.conf"
    );
    if (res < 0 || (size_t)res >= sizeof(conf->path)) {
        warnx("conf: fatal error malformed $HOME");
        return;
    }
    if (load_file(conf->path, &conf->file) == -1) {
        warn("conf: fatal error cannot load %s", conf->path);
        return;
    }
    conf->size = 0;
    conf_fill(conf);
}
