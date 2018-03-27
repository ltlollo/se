// This is free and unencumbered software released into the public domain.
// For more information, see LICENSE.

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"

void *
reallocarr(void **arr, size_t nmemb, size_t size) {
    void *res;

    if (nmemb > SIZE_MAX / size) {
        errno = ENOMEM;
        res = NULL;
    }
    res = realloc(*arr, nmemb * size);
    if (res != NULL) {
        *arr = res;
    }
    return res;
}

void *
reallocflexarr(void **arr, size_t header, size_t nmemb, size_t size) {
    size_t header_nmemb = header / size + header % size;
    void *res;

    if (nmemb + header_nmemb > SIZE_MAX / size) {
        errno = ENOMEM;
        res = NULL;
    }
    res = realloc(*arr, nmemb * size + header);
    if (res != NULL) {
        *arr = res;
    }
    return res;
}

void
memzero(void *arr, size_t nmemb, size_t size) {
    memset(arr, 0, nmemb * size);
}
