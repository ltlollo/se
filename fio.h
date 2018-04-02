// This is free and unencumbered software released into the public domain.
// For more information, see LICENSE.

#ifndef FIO_H
#define FIO_H

#include <stdint.h>
#include "util.h"

struct mmap_file {
    size_t size;
    void *data;
};

struct bmp_header {
    uint16_t signature;
    uint32_t size;
    uint32_t reserved;
    uint32_t offset;
} __pack;
_Static_assert(sizeof(struct bmp_header) == 14, "unsupported architecture");

struct dib_header {
    uint32_t size;
    uint32_t width;
    uint32_t height;
    struct {
        uint16_t planes;
        uint16_t bits_per_px;
    } __pack;
    uint32_t compression;
    uint32_t image_size;
    uint32_t x_px_per_m;
    uint32_t y_px_per_m;
    uint32_t colors_in_colortable;
    char incomplete[];
} __pack;

struct bitmap_data {
    uint32_t width;
    uint32_t padded_bytes_per_row;
    uint32_t height;
    void *data;
    struct mmap_file handle;
};

struct rfp_file {
    uint32_t header;
    uint32_t size;
    uint8_t data[];
} __pack;
_Static_assert(sizeof(struct rfp_file) == 8, "unsupported architecture");

int load_file(const char *, struct mmap_file *);
void unload_file(struct mmap_file *);
int load_bitmap(const char *, struct bitmap_data *);
int convert_bmp_window_to_rfp(struct bitmap_data *, unsigned, unsigned, const char *);
struct rfp_file *load_rfp_file(struct mmap_file *);

#endif // FIO_H
