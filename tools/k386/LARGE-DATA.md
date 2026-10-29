# K386 large-data applications: paged array proposal

Status: **implemented and verified on the emulated CPU** (library, host tests,
`XARRAY.EXE`, and the WebNP2 browser verifier). Nothing here changes the shipped
ABI. V86 applications cannot read the clock (port I/O is trapped), so costs
below are taken from the monitor's per-session CPU tick record by the verifier.

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

## Measured cost

A cache hit is a function call plus a copy; a miss adds one or two INT 2Fh calls
that leave V86, enter the monitor and return through the real-mode broker.
Sequential or blocked access is the intended use; pointer-chasing over the whole
array would miss on nearly every access and is not a goal.

Measured by `scripts/verify-k386-xarray.mjs` in WebNP2 (NP2kai core, `clk=8`,
16 MiB extended memory), from the monitor's per-session CPU ticks (ARTIC,
307200 Hz) between the key press and the scenario's result line. Three
consecutive runs agreed to within 0.1%, and the hit/miss/write-back counts were
identical to the host run.

| Scenario | hits | misses | write-backs | CPU time | Reading |
| --- | --- | --- | --- | --- | --- |
| sequential 1 MiB (2 reads per page) | 256 | 256 | 0 | 2.35 s | 2818 ticks, about 9.2 ms, per missed 4 KiB page; almost no guest computation, so the cleanest miss figure |
| fill 2 MiB (3 byte-wise sweeps) | 0 | 1536 | 512 | 129.7 s | dominated by the guest's own byte loop (6.3 M iterations, about 6 ticks each) |
| sort 8192 values | 114632 | 56 | 28 | 44.8 s | about 120 ticks (0.39 ms) per `xarray_read`/`xarray_write` of 2 bytes; the 84 transfers account for roughly 1% |
| search 64 KiB | 65 | 16 | 2 | 1.6 s | scanning dominates |
| range errors | 0 | 1 | 0 | 0.04 s | |

What this means for a design:

- A miss costs about 9 ms per 4 KiB page. Processing the same page byte by byte
  in the guest cost about 10 times that in the fill scenario, so paging was not
  the bottleneck there.
- The per-call overhead of the byte-addressed `xarray_read`/`xarray_write` is
  what makes element-wise code slow (the sort). Tight loops should use
  `xarray_page` and work on a whole resident page, as the fill scenario does.
- The ticks include the guest's own computation, so only the sequential row is a
  direct miss cost; the others are upper bounds for their transfers.
- These are emulated-CPU figures for this core and clock setting, not a
  prediction for real hardware.

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
3. Sort 8192 16-bit values with a bottom-up merge sort between two 16 KiB
   arrays, each behind a 2-line cache so that 4 pages really pass through 2
   lines; check ordering and that the sum is preserved. The size is a build
   constant (`SORT_VALUES`). 32768 values did not finish within five minutes on
   the emulated CPU, which matches the per-call cost measured below (about a
   million element calls at roughly 0.4 ms each); 1 M values was not tried.
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

`scripts/verify-k386-xarray.mjs` in `kumakumapon/webnp2` (CI job `k386-xarray`),
modeled on `verify-k386-xms.mjs`, drives the real emulated CPU and PC-98 XMS
driver. It checks that:

1. `XARRAY.EXE` starts from the enhanced image and prints its menu;
2. each of the five scenarios passes in the guest, its hit/miss/write-back counts
   match exactly (sort: counts positive and hit-dominated), and the XMS records
   show no block left afterwards;
3. ending the program while the sort runs shows two 16 KiB initialized blocks
   with different tokens, then an ended session with no blocks;
4. two sessions run scenarios 2 to 5 at the same time (the 2 MiB fill is left out
   to keep the run short), both finish, and no block remains;
5. the exported FAT12 image is byte-identical to the original after a clean
   desktop exit.

It also records the measurements above in `verification.json`. WebNP2 pins the
tested MS-DOS commit in `scripts/prepare-msdos4-cpanel-ci.sh` and bundles
`XARRAY.EXE` through `scripts/update-k386.py`. What it does not cover: a protection
fault during a scenario (the demo has no fault key), a forced failure of the XMS
driver, and the real-hardware speed.

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
   this would be a separate, versioned change. The measurements give no evidence
   for it: a miss is about 9 ms per page, small against guest processing of that
   page. It could matter for copy-only workloads, which have not been measured.
3. Should a read-ahead hint (`xarray_prefetch`) exist for sequential scans? Each
   call is synchronous, so a hint cannot overlap with computation; probably not.
4. Is a second access method needed for 16-bit element arrays? The sort result
   shows the byte interface is expensive per element; either a typed accessor
   that avoids the generic copy loop, or guidance to use `xarray_page`, is needed.
5. Should the library live in `tools/k386` next to the SDK, or in a shared
   directory that other tools may include?

## Implementation order

1. Done: `xarray.h`, `xarray.c`, `xarray_test.c`, wired into
   `make -C tools/k386 test`.
2. Done: `XARRAY.EXE` with the self-checking scenarios and statistics, built by
   `make -C tools/k386 all`.
3. Done: run on the emulated CPU and measure miss cost from the host side (see
   Measured cost); the default stays at 4 lines per array in the demo.
4. Done: WebNP2 verifier, enhanced-image entry and CI job (`k386-xarray`).
5. Open: the open questions above, mainly the per-element access cost.
