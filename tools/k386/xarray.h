#ifndef K386_XARRAY_H
#define K386_XARRAY_H
/* Paged byte array over one managed XMS block (see LARGE-DATA.md, XMS.md).
 * Backend and cache are caller supplied, so the same code runs in a V86 text
 * application and in host tests. All functions return 0 or a DOS-style error:
 * 6 stale/foreign token, 8 quota or backend failure, 87 invalid size or range.
 */
enum { XARRAY_PAGE=4096, XARRAY_MAX_PAGES=8, XARRAY_MAX_BYTES=4096UL*1024 };
typedef struct {
    unsigned (*alloc)(unsigned kib, unsigned *token);
    unsigned (*release)(unsigned token);
    unsigned (*read)(unsigned token, unsigned long offset, void *out, unsigned count);
    unsigned (*write)(unsigned token, unsigned long offset, const void *in, unsigned count);
} XArrayBackend;
typedef struct {unsigned long hits,misses,writebacks;} XArrayStats;
typedef struct {
    unsigned long page,stamp;
    unsigned char valid,dirty;
} XArrayLine;
typedef struct {
    const XArrayBackend *backend;
    unsigned token,lines;
    unsigned long bytes,clock;
    unsigned char *cache;
    XArrayStats stats;
    XArrayLine line[XARRAY_MAX_PAGES];
} XArray;
/* bytes: 1..XARRAY_MAX_BYTES. cache: caller-owned lines*XARRAY_PAGE bytes, lines 1..XARRAY_MAX_PAGES.
 * The new block is zero-filled by the monitor; no extra clearing pass is made. */
unsigned xarray_open(XArray *a, const XArrayBackend *backend, unsigned long bytes, void *cache, unsigned lines);
/* Flush, then release the block. The block is released even when the flush fails
 * (the first flush error is returned and the unwritten data is lost). */
unsigned xarray_close(XArray *a);
/* Write back every dirty line. Attempts all lines, returns the first error;
 * lines that failed stay dirty. */
unsigned xarray_flush(XArray *a);
/* Any offset and count, crossing pages as needed. A range outside the array
 * returns 87 and transfers nothing. */
unsigned xarray_read(XArray *a, unsigned long offset, void *out, unsigned count);
unsigned xarray_write(XArray *a, unsigned long offset, const void *in, unsigned count);
/* One resident page for tight loops. *page points at offset inside the cache and
 * *avail is the number of valid bytes up to the end of that page (or the array).
 * The pointer is valid only until the next xarray_* call on this array. */
unsigned xarray_page(XArray *a, unsigned long offset, int will_write, unsigned char **page, unsigned *avail);
void xarray_stats(const XArray *a, XArrayStats *stats);
#ifdef XARRAY_KAPP
/* Backend over the K386 application XMS service (sdk.h). */
extern const XArrayBackend xarray_kapp_backend;
#endif
#endif
