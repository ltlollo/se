#include <stdlib.h>
#include <stdio.h>

#include "fio.h"
#include "util.h"

uint8_t
is_wide(uint8_t *glyph) {
    int i;
    int j;

    for (i = 0; i < 0x12; i++) {
        for (j = 11; j < 0x12; j++) {
            if (glyph[i * 0x1200 + j] == 0) {
                return 1;
            }
        }
    }
    return 0;
}

void
print_glyph(uint8_t *glyph) {
    int i;
    int j;

    for (i = 0; i < 0x12; i++) {
        for (j = 0; j < 0x12; j++) {
            putchar(glyph[i * 0x1200 + j] == 0xff ? 'o' : '.' );
        }
        putchar('\n');
    }
    putchar('\n');
}

int
main(int argc, char *argv[]) {
    uint8_t *map = calloc(0x10001, sizeof(uint8_t));
    const char *fname_rfp = "unifont.rfp";
    int min_group = 4;
    struct rfp_file *rfp;
    struct mmap_file file;
    int i;
    int j;
    int k;

    if (argc - 1 != 0) {
        fname_rfp = argv[1];
    }
    xensure(map);
    xensure(load_file(fname_rfp, &file) == 0);
    rfp = load_rfp_file(&file);

    for (i = 0, k = 0; i < 0x100; i++) {
        for (j = 0; j < 0x100; j++, k++) {
            if (i >=  0xd0 && i <= 0xdf) {
                continue;
            }
            map[k] = is_wide(rfp->data + i * 0x12 * 0x1200 + j * 0x12) == 0;
        }
    }
    printf("static int\nis_glyph_wide(uint32_t glyph) {\n"
        "    if (0) {\n        return 0;\n"
    );
    for (k = 0; k < 0x10000; k++) {
        if (map[k]) {
            for (i = k; map[k]; k++);
            if (k - 1 - i  >= min_group) {
                printf("    } else if (glyph >= 0x%04x && glyph < 0x%04x) {\n"
                    "        return 0;\n"
                    , i
                    , k
                );
            }
        }
    }
    printf("    }\n    return 1;\n}\n");
    return 0;
}
