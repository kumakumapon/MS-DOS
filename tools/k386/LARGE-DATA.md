# K386 large-data applications: paged array proposal

Status: **proposal, not implemented.** Nothing here changes the shipped ABI.
Statements about speed or fit are design estimates and must be measured before
they are relied on (see Validation).

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

New files, all optional and outside the monitor: `tools/k386/xarray.h`,
`tools/k386/xarray.c`, and a demo EXE `XARRAY.EXE`. The library is linked into
the application; it adds no INT 2Fh operation and no monitor change.

### Model

An `XArray` is a byte-addressed region of up to 4 MiB backed by one XMS
block, viewed through a fixed set of cache pages. The page size is
`KAPP_XMS_TRANSFER` (4096 bytes), so one miss is one read of one page and, if
the evicted page is dirty, one write. Replacement is least-recently-used with a
dirty bit per page (write-back).

### API sketch

```c
typedef struct XArray XArray;          /* caller-allocated; fields private */

/* Backend hooks let host tests run the same code against RAM. */
typedef struct {
    unsigned (*alloc)(unsigned kib, unsigned *token);
    unsigned (*free)(unsigned token);
    unsigned (*read)(unsigned token, unsigned long off, void *out, unsigned n);
    unsigned (*write)(unsigned token, unsigned long off, const void *in, unsigned n);
} XArrayBackend;

extern const XArrayBackend xarray_kapp_backend;   /* wraps kapp_xms_* */

/* cache points at pages*XARRAY_PAGE bytes owned by the caller. */
unsigned xarray_open(XArray *a, const XArrayBackend *b, unsigned long bytes,
                     void *cache, unsigned pages);
unsigned xarray_close(XArray *a);                 /* flushes, then frees */
unsigned xarray_flush(XArray *a);

unsigned xarray_read (XArray *a, unsigned long off, void *out, unsigned n);
unsigned xarray_write(XArray *a, unsigned long off, const void *in, unsigned n);

/* Direct access to one resident page for tight loops. The pointer is valid
 * until the next xarray_* call on this array. */
unsigned xarray_page(XArray *a, unsigned long off, int will_write,
                     unsigned char **page, unsigned *avail);
```

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
  4 to 8 pages (16 to 32 KiB) is the realistic range; the default demo uses 6.
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
No numbers are claimed; the demo must record the miss count and the elapsed
ARTIC ticks so the cost per miss and the best cache size can be determined.

## Demo and tests

### `XARRAY.EXE` (V86 text application, ABI 1.0)

Descriptor kind 1, title `XMS paged array`. It checks
`kapp_xms_version()==0x0100`, then runs a menu of self-checking scenarios and
prints the result and the cache statistics (hits, misses, write-backs, ticks):

1. Fill 2 MiB with an address-derived pattern, read it back and compare.
2. Sequential pass touching every page twice.
3. Blocked sort of 1 M 16-bit values in place (merge of cache-sized runs).
4. Search for a pattern that spans a page boundary.
5. Out-of-range read and write returning 87 without a transfer.

### Host unit tests (`xarray_test.c`)

Compile the library against a RAM backend, as `xmem_test.c` does for policy.
Cover: boundary crossing reads and writes, odd lengths, LRU order, write-back
exactly once per dirty eviction, `flush` idempotence, error propagation from the
backend (first error returned, array left consistent), cache of one page, and
`bytes` not a multiple of the page size.

### Browser verification (WebNP2)

`scripts/verify-k386-xarray.mjs`, modeled on `verify-k386-xms.mjs`: run
`XARRAY.EXE` in both sessions with different patterns, confirm independent
contents, task-manager XMS usage, release after exit and after a forced fault,
and an unchanged FAT12 image hash. WebNP2 pins the tested MS-DOS commit in
`scripts/prepare-msdos4-cpanel-ci.sh` as for earlier K386 work.

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

1. `xarray.h`/`xarray.c` and `xarray_test.c` with a RAM backend; wire into
   `make -C tools/k386 all test`.
2. `XARRAY.EXE` with the self-checking scenarios and statistics.
3. Measure miss cost and settle the default cache size; update this document
   with the results and resolve the open questions.
4. WebNP2 browser verifier and CI job; link from APPLICATIONS.md and XMS.md.
