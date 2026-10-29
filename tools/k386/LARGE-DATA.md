# K386 large-data applications: paged array proposal

Status: **library, host tests and `XARRAY.EXE` implemented; not yet run on the
emulated CPU.** Nothing here changes the shipped ABI. Statements about speed are
design estimates: V86 applications cannot read the clock (port I/O is trapped),
so per-miss cost still has to be measured from the host side (see Validation).

## Goal

Let a K386 text EXE work on a data set much larger than its 64 KiB module,
for example a 2 MiB buffer to sort, search or transform, without a new
executable ABI. The existing 16-bit flat SDK module keeps its code, data and
stack in 64 KiB (12 KiB minimum stack, see APPLICATIONS.md). The bulk data
lives in managed XMS blocks (XMS.md, `kapp_xms_*`) and is reached through a
small page cache in conventional memory.

## Options considered

| Option | Reach | Fit | Notes |
| --- | --- | --- | --- |
| A. Several MZ segments in conventional RAM | at most the private arena, below 90000h bytes minus other users | needs far pointers and a non-SDK link layout; `tools/mz/build.py` already packages separate code/data/stack segments and relocations | The loader accepts general segmented MZ (APPLICATIONS.md), but the arena is shared with KSH and child EXEC, so usable space is smaller than the arena size. Not verified for large data. |
| B. XMS data API with a paged library (this proposal) | 4096 KiB per session, 8 blocks | works inside the existing SDK module | Needs explicit page transfers; random access costs a trap per miss. |
| C. 32-bit flat executables or DPMI | large | none | Out of scope: XMS.md states this API is not a 32-bit executable ABI. |

Options A and B can be combined (far-pointer scratch buffers plus an XMS
backing store), but this document specifies B only.

## Proposed library: `xarray`

Files, all optional and outside the monitor: `xarray.h`, `xarray.c`,
`xarray_test.c` (host tests), and the demo `xarraycheck.c` built as `XARRAY.EXE`.
The library is linked into the application; it adds no INT 2Fh operation and no
monitor change. Build the library for an application with `-DXARRAY_KAPP` to get
`xarray_kapp_backend`; without it the library has no dependency on `sdk.h`.

### Model

An `XArray` is a byte-addressed region of up to 4 MiB backed by one XMS
block, viewed through a fixed set of cache pages. The page size is
`KAPP_XMS_TRANSFER` (4096 bytes), so one miss is one read of one page and, if
the evicted page is dirty, one write. Replacement is least-recently-used with a
dirty bit per page (write-back).

### API

```c
enum { XARRAY_PAGE=4096, XARRAY_MAX_PAGES=8, XARRAY_MAX_BYTES=4096UL*1024 };

typedef struct {                       /* hooks let host tests run on RAM */
    unsigned (*alloc)(unsigned kib, unsigned *token);
    unsigned (*release)(unsigned token);
    unsigned (*read)(unsigned token, unsigned long offset, void *out, unsigned count);
    unsigned (*write)(unsigned token, unsigned long offset, const void *in, unsigned count);
} XArrayBackend;
extern const XArrayBackend xarray_kapp_backend;   /* with -DXARRAY_KAPP */

unsigned xarray_open(XArray *a, const XArrayBackend *b, unsigned long bytes,
                     void *cache, unsigned lines);
unsigned xarray_close(XArray *a);
unsigned xarray_flush(XArray *a);
unsigned xarray_read (XArray *a, unsigned long offset, void *out, unsigned count);
unsigned xarray_write(XArray *a, unsigned long offset, const void *in, unsigned count);
unsigned xarray_page(XArray *a, unsigned long offset, int will_write,
                     unsigned char **page, unsigned *avail);
void     xarray_stats(const XArray *a, XArrayStats *stats);  /* hits, misses, writebacks */
```

`XArray` is caller-allocated; its fields are private. `lines` is 1 to 8 and the
cache is `lines*XARRAY_PAGE` caller-owned bytes. `xarray_page` returns a pointer
to one resident page position and the bytes available up to the end of that page
(or of the array); the pointer is valid only until the next `xarray_*` call.
`xarray_flush` attempts every dirty line and returns the first error, leaving
failed lines dirty. `xarray_close` flushes and then **always** releases the
block; on a failed flush the first error is returned and the unwritten data is
lost. A failed eviction write or fetch leaves the array usable: a dirty line is
kept, and a line whose fetch failed is marked invalid.

All functions return 0 on success or the underlying DOS-style error (6, 8, 87
as in XMS.md). `xarray_read`/`xarray_write` accept any `n` and any offset,
including ranges that cross page boundaries; they split the transfer
internally. A range outside `bytes` returns 87 and transfers nothing.

### Allocation and cache sizing

- `bytes` is rounded up to whole KiB for `kapp_xms_alloc`; the block is zeroed
  by the monitor before it is returned (XMS.md). `xarray_open` makes no extra
  clearing pass.
- Quota: 4096 KiB and eight blocks per session, shared with child EXEC levels.
  An array larger than the remaining quota fails with error 8. Because one
  block can be at most 4096 KiB, a single `XArray` cannot exceed 4 MiB.
- The cache is **caller-provided static memory** inside the 64 KiB module. With
  the 12 KiB minimum stack and the application's own code and data, a cache of
  4 to 8 pages (16 to 32 KiB) is the realistic range; the demo uses 4 lines per array.
  A cache of 16 pages (64 KiB) cannot fit in the same module. Whether the cache
  could instead come from the DOS arena (AH=48h) under the SDK layout is an open
  question below.

### Concurrency and lifetime

- One `XArray` is used by one process; handles are not shared (XMS.md: a child
  cannot read or free a parent block and receives no inherited tokens). A
  child that needs the data must open its own array and receive the content
  through a file or a pipe-like convention defined by the application.
- Blocks are released automatically on normal exit, fault, interruption and
  root replacement. **Dirty cache pages are not written back in those cases**;
  an application that needs durability writes a file itself (the disk is
  read-only for sessions today, see STATUS.md), so XMS content is volatile.
- `kapp_yield()` is not called inside the library. Each miss is a bounded
  INT 2Fh call, and the monitor can preempt the application between calls.

## Expected cost (to be measured)

A cache hit is plain memory access. A miss costs up to two INT 2Fh calls that
leave V86, enter the monitor and return through the real-mode broker, so
sequential or blocked access patterns are the intended use. Pointer-chasing
over the whole array would miss on nearly every access and is not a goal.
No numbers are claimed. The demo reports miss counts; elapsed time per scenario
must be measured from the host side (guests cannot read the clock), which gives
the cost per miss and the best cache size.

## Demo and tests

### `XARRAY.EXE` (V86 text application, ABI 1.0)

Descriptor kind 1, title `XMS paged array`. It checks
`kapp_xms_version()==0x0100`, prints `XARRAY READY` and a key menu (1 to 5, `A`
all, `Q` quit), and runs self-checking scenarios. Each prints
`XARRAY OK <name> hits=.. misses=.. writebacks=..`, a failure prints
`XARRAY FAIL: <reason>` and exits 1, and `A` ends with `XARRAY ALL OK`. Two
4-line caches (32 KiB of static buffer) fit the module: the image is about
39 KiB with the 12 KiB stack margin intact.

1. Fill 2 MiB: verify the new block is zero, write an address-derived pattern,
   verify it, all through `xarray_page`.
2. Sequential 1 MiB: read the first and last byte of every page; the statistics
   must be exactly one miss and one hit per page and no write-back.
3. Sort 32768 16-bit values with a bottom-up merge sort between two 64 KiB
   arrays; check ordering and that the sum is preserved. The size is a build
   constant (`SORT_VALUES`); 1 M values was judged too slow for per-element
   calls on the emulated CPU and has not been tried.
4. Search for an 8-byte pattern planted across a page boundary; it must be found
   exactly once at the planted offset.
5. Out-of-range read, write and page requests return 87 and leave the
   statistics unchanged; empty and last-byte ranges succeed.

### Host unit tests (`xarray_test.c`)

Compile the library against a RAM backend, as `xmem_test.c` does for policy;
`make -C tools/k386 test` runs it. It covers argument validation, allocation
failure, `bytes` not a multiple of the page size, page-crossing and odd-length
transfers, LRU order, write-back exactly once per dirty eviction, `flush`
idempotence, a one-line cache, `xarray_page`, injected backend failures
(retryable dirty data, no stale line after a failed fetch, close releasing
despite a failed flush, double close), and a randomized comparison against a
flat reference for every cache size from 1 to 8 lines.

The demo scenarios were additionally run once on the host against a model of the
XMS contract (4096 KiB quota, eight blocks, 4 KiB transfers, zeroed blocks); all
passed. That is a logic check only, not a run on the emulated CPU.

### Browser verification (WebNP2)

**Not yet written.** Planned as `scripts/verify-k386-xarray.mjs`, modeled on
`verify-k386-xms.mjs`: run `XARRAY.EXE` in both sessions, wait for
`XARRAY ALL OK`, confirm task-manager XMS usage while running and release after
exit and after a forced fault, and an unchanged FAT12 image hash. Because guests
cannot read the clock, the verifier is also the place to measure elapsed
emulated time per scenario and so the cost per miss. WebNP2 pins the tested
MS-DOS commit in `scripts/prepare-msdos4-cpanel-ci.sh` as for earlier K386 work,
and `XARRAY.EXE` must be added to the enhanced image there.

## Non-goals

- No change to INT 2Fh AX=D389h, the quota or the 4 KiB transfer limit.
- No flat 32-bit pointers, DPMI, raw XMS entry point or EMS.
- No sharing of an array between sessions or between parent and child.
- No persistence; no disk write support.
- No use from native KDESK built-in tools, which are not V86 clients of the
  XMS service (XMS.md).

## Open questions

1. Can the page cache be allocated from the DOS arena at run time inside the SDK
   module layout, so cache size is not bound by the 64 KiB module?
2. Is a larger transfer unit worthwhile? The 4 KiB limit is part of ABI 1.0, so
   this would be a separate, versioned change and needs a miss-cost
   measurement first.
3. Should a read-ahead hint (`xarray_prefetch`) exist for sequential scans, and
   would it help given that each call is synchronous?
4. Is a second access method needed for 16-bit element arrays, or is the byte
   interface plus `xarray_page` enough?
5. Should the library live in `tools/k386` next to the SDK, or in a shared
   directory that other tools may include?

## Implementation order

1. Done: `xarray.h`, `xarray.c`, `xarray_test.c`, wired into
   `make -C tools/k386 test`.
2. Done: `XARRAY.EXE` with the self-checking scenarios and statistics, built by
   `make -C tools/k386 all`.
3. Open: run on the emulated CPU, measure miss cost from the host side and settle
   the default cache size; resolve the open questions with those results.
4. Open: WebNP2 verifier, enhanced-image entry and CI job.
