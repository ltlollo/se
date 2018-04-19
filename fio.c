// This is free and unencumbered software released into the public domain.
// For more information, see LICENSE.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "util.h"
#include "fio.h"

int
load_file(const char *fname, struct mmap_file *file) {
    int fd = open(fname, O_RDONLY);
    off_t length;

    xensure_errno(fd != -1);

    length = lseek(fd, 0, SEEK_END);
    xensure_errno(length != -1);
    xensure_errno(lseek(fd, 0, SEEK_SET) != -1);

    file->size = length;
    file->data = mmap(NULL, file->size, PROT_READ, MAP_PRIVATE, fd, 0);
    xensure_errno(file->data != MAP_FAILED);

    close(fd);
    return 0;
}

void
unload_file(struct mmap_file *file) {
    int res = munmap(file->data, file->size);

    file->size = 0;
    file->data = NULL;

    dbg_assert(res == 0);
}

int
load_bitmap(const char *fname, struct bitmap_data *bmp) {
    struct mmap_file file;
    struct bmp_header *bh;
    struct dib_header *dh;
    uint32_t bytes_per_row;

    xensure(load_file(fname, &file) == 0);
    
    bh = file.data;
    xensure(memcmp(&bh->signature, "BM", 2) == 0);
    dh = file.data + sizeof(*bh);

    bmp->width = dh->width;
    bytes_per_row = dh->width * dh->bits_per_px;
    bytes_per_row = (bytes_per_row / 8 + bytes_per_row % 8);
    bmp->padded_bytes_per_row = bytes_per_row + bytes_per_row % 4;
    bmp->height = dh->height;
    bmp->data = file.data + bh->offset;

    xensure(file.size == bh->size);
    xensure(dh->compression == 0 && dh->bits_per_px == 1);
    xensure(bmp->data + bmp->padded_bytes_per_row * bmp->height
        <= file.data + file.size
    );
    switch (dh->size) {
        case 12: case 16: case 40: case 52: case 56: case 64: case 108:
        case 124: break;
        default: errx(1, "malformed bmp file");
    }
    xensure(bmp->data >= file.data + dh->size);
    xensure(dh->planes == 1 && dh->colors_in_colortable == 2);

    memcpy(&bmp->handle, &file, sizeof(file));
    return 0;
}

int
convert_bmp_window_to_rfp(struct bitmap_data *bmp
    , unsigned offx
    , unsigned offy
    , const char *fname
    ) {
    int fd = open(fname, O_RDWR | O_CREAT, 0644);
    uint32_t size = 0x1440000 + sizeof(struct rfp_file);
    char *bmpdata = bmp->data;
    size_t line = bmp->padded_bytes_per_row;
    struct rfp_file *rfp;
    struct mmap_file file;
    void *end;
    size_t i, j, n, m, o, p;
    size_t r_i;

    xensure_errno(fd != -1);
    end = bmp->data + line * (offx + 0x1000);
    xensure(end <= bmp->handle.data + bmp->handle.size);

    xensure(ftruncate(fd, size) != -1);
    rfp = mmap(NULL, size, PROT_WRITE, MAP_SHARED, fd, 0);
    xensure_errno(rfp != MAP_FAILED);

    memcpy(&rfp->header, "RFP", 4);
    memcpy(&rfp->size, &size, sizeof(size));

    // NOTE : slow convert procedure, but it's just an offline precomputation
    for (i = offy, n = 1, o = 1; i < 0x1000 + offy; i++, n++, o++) {
        r_i = bmp->height - 1 - i;
        for (j = offx, m = 1, p = 1; j < 0x1000 + offx; j++, m++, p++) {
            rfp->data[o * 0x1200 + p] =
                ((bmpdata[r_i * line + j / 8] >> (7 - ( j % 8))) & 1)
                ? 0 : 0xff;
            if (m % 0x10 == 0) {
                p+=2;
            }
        }
        if (n % 0x10 == 0) {
            o+=2;
        }

    }
    file.data = rfp;
    file.size = size;
    unload_file(&file);
    close(fd);
    return 0;
}

struct rfp_file *
load_rfp_file(struct mmap_file *file) {
    struct rfp_file *rfp;

    rfp = file->data;
    xensure(file->size > sizeof(struct rfp_file));
    xensure(file->size == rfp->size);
    xensure(file->size == 0x1440000 + sizeof(struct rfp_file));
    xensure(memcmp(&rfp->header, "RFP", 4) == 0);

    return rfp;
}

struct rfp_file *
load_cfp_file(struct mmap_file *file) {
    struct rfp_file *rfp;

    rfp = file->data;
    xensure(file->size > sizeof(struct rfp_file));
    xensure(file->size == rfp->size);
    xensure(file->size == 0x288000 + sizeof(struct rfp_file));
    xensure(memcmp(&rfp->header, "CFP", 4) == 0);

    return rfp;
}
