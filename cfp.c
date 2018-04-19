// This is free and unencumbered software released into the public domain.
// For more information, see LICENSE.

#include <stdio.h>
#include <stdlib.h>

#include "fio.h"

extern char *__progname;

static void
help() {
    fprintf(stderr, "Usage : %s ibmp orfp\n"
        "Scope : Converts a rfp texture of 256x256x18x18 glyphs into a"
        " compressed rfp texture\n"
        "Params :\n"
        "\tibmp<string> : name of the RFP input file\n"
        "\torfp<string> : name of the compressed RFP output file\n"
        , __progname
    );
}

int
main(int argc, char *argv[]) {
    char *ifname;
    char *ofname;

    if (argc - 1 != 2) {
        help();
        return 1;
    }
    ifname = argv[1];
    ofname = argv[2];
    rfp_compress(ifname, ofname);

    return 0;
}
