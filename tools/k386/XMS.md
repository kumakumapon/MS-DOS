# KDESK/386 application XMS data API, version 1.0

K386 text EXEs can retain more than 64 KiB of data in managed extended-memory
blocks. Executables, near pointers and the stack still use the existing 16-bit
ABI. This is an explicit data-storage/copy API, not a 32-bit executable ABI,
flat multi-megabyte pointers, DPMI, or an emulation of the raw XMS entry point.
Native built-in GUI tools are not V86 clients of this service.

## Use

Include `sdk.h`, use the existing `app-start.S` / `app.ld` / MZ packager, and
check `kapp_xms_version()==0x0100` before making calls. For example:

```c
unsigned handle;
unsigned char buffer[1024];
/* Fill buffer with application data. All functions return zero on success. */
if (!kapp_xms_alloc(1024, &handle)) { /* 1 MiB, initially all zero */
    if (!kapp_xms_write(handle, 0x90000, buffer, sizeof buffer))
        kapp_xms_read(handle, 0x90000, buffer, sizeof buffer);
    kapp_xms_free(handle);
}
```

Do not dereference a handle. Use a normal conventional-memory buffer and a
32-bit block offset. `kapp_xms_usage()` reports session/process KiB and block
counts. Inspect the returned status before using results. The supplied
`XMEM.EXE` diagnostic verifies the full zeroed 1 MiB, distinct per-session
patterns, copies crossing 64 KiB, odd lengths, final bytes and range rejection.
It retains its block until Q (exit), F (protection fault), P (child ownership
check), T (foreign token check), or R (retained data check). `XCHILD.EXE` tests
parent/child ownership; both are actual guest MZ applications.

## Limits and ownership

- Two V86 sessions; each has an independent **4096 KiB quota and eight blocks**.
  Nested EXEC levels share that quota; every block belongs to one PSP.
- Allocation sizes are whole KiB, 1..4096. Success depends on free host XMS and
  driver handles. The enhanced WebNP2 profile uses 16 MiB extended memory.
- A single read/write transfers 0..4096 bytes. Offsets are unsigned 32-bit.
  Both block and conventional-buffer ranges are checked before any data copy;
  buffers cannot wrap their segment or enter TVRAM/device/ROM memory.
- Ownership includes session and PSP. Children cannot read/free parent blocks,
  and receive no inherited XMS tokens. The parent keeps its blocks across EXEC.
  This does not add memory protection between DOS processes within one VM.
- Normal exit, contained faults, interruption, root replacement and End release
  the affected process/session blocks. Pause and CPU-budget suspension retain
  them. A root fault may retain its normal-RAM diagnostic snapshot, but its
  application XMS is released immediately.
- Tokens are global monotonic 16-bit IDs; reuse is refused after 65535 successful
  allocations until reboot. Freed tokens never regain authority during a boot,
  including after session restart or reuse of a DOS PSP/driver handle.
- Driver allocation/free/transfer failures return errors. A failed free retains
  its trusted record and quota (fail closed); later cleanup retries it. No raw
  driver handle, driver entry point, address or physical page is returned.

Before a newly allocated block is published, all of its bytes are cleared.
Each real-mode broker turn clears at most **16 KiB** through the resident 4 KiB
bounce buffer; subsequent scheduler turns continue initialization before
resuming that application's CPU. No guest instruction observes uncleared data.
The GUI and the other session can run between turns. End/interruption also
cancels pending initialization. Read/write calls copy at most 4 KiB; XMS driver
calls are synchronous and this is not a hard real-time guarantee.

## INT 2Fh contract

AX=D389h is a distinct V86-only multiplex service. BX selects the operation.
Unknown multiplex IDs keep their previous uninstalled behavior. Calling the
host driver directly remains unavailable. Query returns AX=389Dh, BX=0100h,
CX=4096 (session quota in KiB), DX=4096 (max transfer bytes), carry clear.

Other operations return AX=0 / carry clear, or AX=error / carry set. Input
16-bit token and ES:DI buffer offset use the low word; offset ESI and count ECX
use all 32 bits. Registers not listed as results and segment registers remain
unchanged. SDK wrappers set ES to DS temporarily and restore it afterwards.

| BX | Operation | Inputs | Results on success |
| --- | --- | --- | --- |
| 0 | Query | none | version/quota/transfer as above |
| 1 | Allocate | ECX=KiB | DX=opaque token |
| 2 | Free | DX=token | none |
| 3 | Read XMS to conventional | DX=token, ESI=block offset, ECX=bytes, ES:DI=buffer | buffer |
| 4 | Write conventional to XMS | same | none |
| 5 | Usage | none | CX=session KiB, DX=session blocks, SI=PSP KiB, DI=PSP blocks |

Errors: 1 unsupported operation; 6 invalid/stale/foreign token; 8 quota, block
slot, token, XMS-driver or transfer failure; 87 invalid size/range. The normal
DOS/BIOS and read-only disk contracts are unchanged. Allocation can return to
the monitor internally several times, but the guest observes one completed
INT 2Fh instruction, with a final token or error.

## Implementation and validation

`xmem.c` owns policy, trusted metadata, accounting and incremental clearing;
`backing.c` adapts the existing PC-98 XMS driver including odd-byte transfers;
`runtime.c` brokers V86 copies and lifecycle cleanup; `engine.c` traps the new
multiplex ID; `sdk.h` provides callers. `K386SYS1` keeps its existing size and
version; its XMS usage includes both the 664 KiB VM reservation and application
blocks. The read-only `K386XMS1` diagnostic record supports browser inspection;
it is not mapped or returned to V86 clients. Legacy application ABI 1.0 and the
ordinary KDESK fallback remain available.

`make -C tools/k386 all test` covers quotas, ownership, range arithmetic, stale
tokens, initialization budgets, nested cleanup and injected driver failures.
WebNP2 `scripts/verify-k386-xms.mjs` uses the real CPU and PC-98 driver to check
the EXEs, independent session handles, private CPU states, task accounting,
nested EXEC, lifecycle recovery and an unchanged FAT12 image. Existing K386 and
GUI suites remain required. No original DOS kernel or upstream driver is edited.
