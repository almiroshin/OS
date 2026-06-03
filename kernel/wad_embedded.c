#include <stddef.h>
#include <string.h>
#include "w_file.h"
#include "z_zone.h"

extern unsigned char doom_iwad[];
extern unsigned int doom_iwad_len;

typedef struct {
    wad_file_t wad;
} embedded_wad_file_t;

extern wad_file_class_t stdc_wad_file;

static wad_file_t *W_Embedded_OpenFile(char *path)
{
    (void) path;
    embedded_wad_file_t *result = Z_Malloc(sizeof(embedded_wad_file_t), PU_STATIC, 0);
    result->wad.file_class = &stdc_wad_file;
    result->wad.mapped = doom_iwad;
    result->wad.length = doom_iwad_len;
    return &result->wad;
}

static void W_Embedded_CloseFile(wad_file_t *wad)
{
    Z_Free(wad);
}

static size_t W_Embedded_Read(wad_file_t *wad, unsigned int offset,
                              void *buffer, size_t buffer_len)
{
    if (offset >= wad->length) {
        return 0;
    }
    if (offset + buffer_len > wad->length) {
        buffer_len = wad->length - offset;
    }
    memcpy(buffer, wad->mapped + offset, buffer_len);
    return buffer_len;
}

wad_file_class_t stdc_wad_file = {
    W_Embedded_OpenFile,
    W_Embedded_CloseFile,
    W_Embedded_Read,
};

