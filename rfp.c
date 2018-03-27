// This is free and unencumbered software released into the public domain.
// For more information, see LICENSE.

#include <stdio.h>
#include <stdlib.h>

#include "fio.h"

extern char *__progname;

static void
help() {
    fprintf(stderr, "Usage : %s ibmp orfp [offx] [offy]\n"
        "Scope : Converts a padded bmp texture of 256x256x16x16 glyphs into a"
        " rfp texture\n"
        "Params :\n"
        "\tibmp<string> : name of the BMP input file\n"
        "\torfp<string> : name of the RFP output file\n"
        "\toffx<int> : horizontal pixel padding\n"
        "\toffy<int> : vertical pixel padding\n"
        , __progname
    );
}

int
main(int argc, char *argv[]) {
    int offx = 32;
    int offy = 64;
    struct bitmap_data bmp;
    char *ifname;
    char *ofname;

    if (argc - 1 < 2) {
        help();
        return 1;
    }
    ifname = argv[1];
    ofname = argv[2];

    if (argc - 1 > 2) {
        offx = atoi(argv[3]); 
        xensure(offx >= 0);
    }
    if (argc - 1 > 3) {
        offy = atoi(argv[4]); 
        xensure(offy >= 0);
    }
    xensure(load_bitmap(ifname, &bmp) == 0);
    xensure(convert_bmp_window_to_rfp(&bmp
            , (unsigned)offx
            , (unsigned)offy
            , ofname
    ) == 0);
    return 0;
}
