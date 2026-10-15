#include "bitmap.h"

static u8 row[960], header[54], mapping[256];
static u16 word(const u8 *p) { return p[0] | (u16)p[1] << 8; }
static u32 dword(const u8 *p) { return word(p) | (u32)word(p + 2) << 16; }
static void setword(u8 *p, unsigned n) { p[0] = n; p[1] = n >> 8; }
static void setdword(u8 *p, u32 n) { setword(p, n); setword(p + 2, n >> 16); }
void bitmap_free(Bitmap *image) { mem_free(image->segment); *image = (Bitmap){0}; }
int bitmap_new(Bitmap *image, int width, int height, int color) {
    if (width < 1 || width > IMAGE_WIDTH || height < 1 || height > IMAGE_HEIGHT) return 0;
    unsigned segment = mem_alloc(width * (u32)height);
    if (!segment) return 0;
    mem_fill(segment, 0, width * (u32)height, color);
    bitmap_free(image); *image = (Bitmap){segment, width, height}; return 1;
}
int bitmap_clone(Bitmap *out, const Bitmap *in) {
    Bitmap next = {0};
    if (!bitmap_new(&next, in->width, in->height, WHITE)) return 0;
    mem_copy(next.segment, 0, in->segment, 0, in->width * (u32)in->height);
    bitmap_free(out); *out = next; return 1;
}
int bitmap_load(Bitmap *image, const char *path) {
    int handle = p_open(path), error = -11; Bitmap next = {0}; Entry file;
    if (handle < 0) return handle;
    if (p_read(handle, header, 54) != 54 || header[0] != 'B' || header[1] != 'M') goto finish;
    u32 dib = dword(header + 14), offset = dword(header + 10), colors = dword(header + 46);
    int width = (int)dword(header + 18), signed_height = (int)dword(header + 22);
    unsigned bits = word(header + 28), height = signed_height < 0 ? -(u32)signed_height : (u32)signed_height;
    if (dib < 40 || dib > 124 || width < 1 || width > IMAGE_WIDTH || height < 1 || height > IMAGE_HEIGHT ||
        word(header + 26) != 1 || dword(header + 30) != 0 || !(bits == 1 || bits == 4 || bits == 8 || bits == 24)) goto finish;
    u32 stride = ((width * bits + 31) / 32) * 4;
    if (p_stat(path, &file) < 0 || offset > file.size || height * stride > file.size - offset) goto finish;
    if (bits <= 8) {
        if (!colors) colors = 1u << bits;
        if (colors > (1u << bits) || offset < 14 + dib + colors * 4 || p_seek(handle, 14 + dib) < 0) goto finish;
        for (unsigned i = 0; i < colors; i++) {
            if (p_read(handle, row, 4) != 4) goto finish;
            mapping[i] = gfx_nearest(row[2], row[1], row[0]);
        }
    } else if (offset < 14 + dib) goto finish;
    if (!bitmap_new(&next, width, height, WHITE)) { error = -8; goto finish; }
    if (p_seek(handle, offset) < 0) goto finish;
    for (unsigned y = 0; y < height; y++) {
        if (p_read(handle, row, stride) != (int)stride) goto finish;
        unsigned to = signed_height < 0 ? y : height - y - 1;
        for (int x = 0; x < width; x++) {
            unsigned color;
            if (bits == 24) color = gfx_nearest(row[x * 3 + 2], row[x * 3 + 1], row[x * 3]);
            else {
                unsigned index = bits == 8 ? row[x] : bits == 4 ? (row[x / 2] >> ((1 - (x & 1)) * 4)) & 15 : (row[x / 8] >> (7 - (x & 7))) & 1;
                if (index >= colors) goto finish;
                color = mapping[index];
            }
            mem_put(next.segment, to * (u32)width + x, color);
        }
        input_poll();
    }
    bitmap_free(image); *image = next; next.segment = 0; error = 0;
finish:
    p_close(handle); bitmap_free(&next); return error;
}
int bitmap_save(const Bitmap *image, const char *path) {
    char parent[FD_PATH], temp[FD_PATH], backup[FD_PATH]; Entry entry;
    fd_text(parent, path, sizeof parent); fd_parent(parent);
    if (fd_path(temp, parent, "KDIMG.TMP") || fd_path(backup, parent, "KDIMG.BAK")) return -FD_BADNAME;
    if (!fd_cmp(path, temp) || !fd_cmp(path, backup)) return -FD_BADNAME;
    int exists = p_stat(path, &entry);
    if (!exists && (entry.attr & 1)) return -5;
    int old = p_stat(backup, &entry);
    if ((exists < 0 && exists != -2) || old != -2) return -FD_EXISTS;
    int handle = p_create(temp);
    if (handle < 0) return handle;
    unsigned stride = (image->width + 3) & ~3u; u32 size = 1078 + stride * (u32)image->height;
    for (unsigned i = 0; i < sizeof header; i++) header[i] = 0;
    header[0] = 'B'; header[1] = 'M'; setdword(header + 2, size); setdword(header + 10, 1078);
    setdword(header + 14, 40); setdword(header + 18, image->width); setdword(header + 22, image->height);
    setword(header + 26, 1); setword(header + 28, 8); setdword(header + 34, size - 1078); setdword(header + 46, 256);
    int success = p_write(handle, header, 54) == 54;
    for (int i = 0; i < 256 && success; i++) {
        u32 rgb = gfx_rgb(i); row[0] = rgb; row[1] = rgb >> 8; row[2] = rgb >> 16; row[3] = 0;
        success = p_write(handle, row, 4) == 4;
    }
    for (int y = image->height - 1; y >= 0 && success; y--) {
        for (unsigned x = 0; x < stride; x++) row[x] = x < (unsigned)image->width ? mem_get(image->segment, y * (u32)image->width + x) : 0;
        success = p_write(handle, row, stride) == (int)stride; input_poll();
    }
    if (p_close(handle) < 0) success = 0;
    if (!success) { p_unlink(temp); return -29; }
    if (!exists && p_rename(path, backup) < 0) { p_unlink(temp); return -5; }
    if (p_rename(temp, path) < 0) {
        if (!exists && p_rename(backup, path) < 0) return -FD_ROLLBACK;
        p_unlink(temp); return -5;
    }
    if (!exists && p_unlink(backup) < 0) return 1;
    return 0;
}
