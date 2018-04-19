// This is free and unencumbered software released into the public domain.
// For more information, see LICENSE.

#include <stdint.h>
#include <string.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "util.h"
#include "fio.h"
#include "comp.h"

void
rfp_compress(const char *ifname, const char *ofname) {
    size_t size = 0x288000 + sizeof(struct rfp_file);
    int fd = open(ofname, O_RDWR | O_CREAT, 0644);
    struct mmap_file file;
    struct rfp_file *rfp;
    void *mem;
    uint8_t *curr_byte;
    v8 *beg;
    v8 *end;

    xensure(fd != -1);
    xensure(ftruncate(fd, size) != -1);
    xensure(load_file(ifname, &file) == 0);
    rfp = load_rfp_file(&file);

    beg = (void *)(rfp->data);
    end = (void *)(rfp->data + rfp->size);
    mem = mmap(NULL, size, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);

    memcpy(mem, "CFP", 4);
    memcpy(mem + 4, &size, sizeof(size));

    curr_byte = mem + sizeof(struct rfp_file);
    while (beg != end) {
        *curr_byte++ = 0
            | ((*beg)[0] & (1 << 0))
            | ((*beg)[1] & (1 << 1))
            | ((*beg)[2] & (1 << 2))
            | ((*beg)[3] & (1 << 3))
            | ((*beg)[4] & (1 << 4))
            | ((*beg)[5] & (1 << 5))
            | ((*beg)[6] & (1 << 6))
            | ((*beg)[7] & (1 << 7))
            ;
        beg++;
    }
}

void
rfp_decompress(void *restrict in, v8 *restrict out) {
    uint8_t *restrict beg __align(8) = in + sizeof(struct rfp_file);
    uint8_t *restrict end __align(8) = beg + 0x288000;

    while (beg != end) {
        memcpy(*out++, sym_table[*beg++], sizeof(v8));
    }
}

