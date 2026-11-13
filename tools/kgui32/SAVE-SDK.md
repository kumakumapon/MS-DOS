# SDK4: workspace save API and Editor

Use the separate KGUI32 image, 16 MiB RAM, Welcome > Editor (`KGEDIT.EXE`).
Existing KDESK and the original DOS kernel remain separate and unchanged.
Documents use ASCII/JIS X 0201 kana/JIS X 0208 Shift-JIS, at most 65,536 bytes.
See [JAPANESE.md](JAPANESE.md). Open normalizes CRLF to LF;
Save uses the selected LF/CRLF format; see [RECOVERY-SDK.md](RECOVERY-SDK.md). Saving retains selection, cursor and Undo/Redo history.

- **Save / Ctrl+S** saves the current workspace file, or opens Save As.
- **Save As / Ctrl+W** opens the reusable picker, with a filename input.
  Enter an ASCII 8.3 name ending in `.TXT`, then Enter or Save. Tab moves
  between filter, list, filename, Save and Cancel. A selected list row fills
  the filename; Enter in the list also selects it. An existing destination requires Replace (R) or Cancel (Esc).
- **Open / Ctrl+O** starts in Work when a supported HDD is present, otherwise
  in the read-only A: root. Work selects the broker's workspace (normally
  `A:\WORK` on standalone HDD, `B:\WORK` on paired FD/HDD, or `A:\WORK` on FD); Root selects A:. Ctrl+W toggles Root/Work inside Open. Filtering, list navigation and Open work in
  either location. Root files can be edited, but Save As writes only to Work.
- X, F10, New and Open offer **Save (S), Discard (D), Cancel (Esc)** for a
  modified document. Canceling either a save picker or replacement also cancels
  the pending close. Save failure retains document, history and dirty state.
- Another editor changing the same file makes an ordinary Save fail with
  `File changed`; reopen it or explicitly Save As. It is never silently replaced.
- End / Ctrl+G remain forceful operations that discard unsaved changes.

Writes are restricted to writable, ordinary `.TXT` files in the selected
`WORK` directory. The generated FD and marked FAT16 HDD provision empty Work
directories. The guest broker identifies KHD98 through DOS's DPB and its resident
versioned geometry record; applications cannot choose a drive. Without a supported
HDD it retains A:. It may create a missing WORK on a recognized HDD through DOS,
but never formats or repairs a volume. An unavailable WORK on a recognized HDD
returns an error on that HDD rather than silently saving to A:.

The picker shows the destination, HDD/FD, DOS free space and cluster-rounded
temporary data requirement. Capacity is a snapshot; another task or directory
expansion may consume it before the save. The broker rechecks space and all actual
writes/closes. An overwrite needs space for the complete new copy while the old
file remains allocated. The size limit is not a free-space guarantee.
Root boot files, EXEs, devices, paths, application-selected drives, hidden/system/read-only
files and arbitrary subdirectories are outside the write API. No mkdir/delete
or arbitrary write-handle API is exposed. SDK1–3 calls and wire layouts retain
their contracts. The app-local [IME](JAPANESE.md) requires explicit commit or
cancel before save, New, Open or ordinary close. A failed commit retains the
composition and document. Host clipboard and extra slots remain later work.
The browser's existing disk persistence/export saves the modified image;
export a copy before resetting it to the bundled original.

## Broker and ownership

Native `k38632/workspace.h` defines feature **128**, supported only by K32HOST
clients also declaring GUI=32 and directory=64. KGEDIT declares **231** and
queries **239**. Old clients query their previous features. K32RUN and KGUILDR
reject save-enabled client packages. There is no new privileged I/O in clients.

INT30h **17**, EBX=RW `K32Work`, ECX=64; EAX=result. The 64-byte version-1
request has u32 version/size/operation/flags, name[16], then u32 data/bytes/
exists/length/hash/reserved[3]. Reserved fields must be zero. Flags are zero
except SAVE may set `K32_WORK_CRLF=1` for strict CRLF; LF saves use zero.
The complete encoded document is validated before mutation, including CR/LF
and Shift-JIS pairs split across 4096-byte copies. `sdk/k32x.h`
provides `k32_work`, `k32_work_name`, `k32_work_first` and `k32_work_info`.

| Operation | Request / result |
| --- | --- |
| 1 STAT | Name; returns 0 and exists/length/FNV-1a of raw bytes, bounded to 64 KiB. |
| 2 OPEN | Name; returns the existing owned read-only handle token; use read/seek/close. |
| 3 FIRST | data=RW K32DirEntry, bytes=64; enumerates Work TXT files. Use existing NEXT/CLOSE. |
| 4 SAVE | Name, data=readable document, bytes<=65536, expected exists/length/hash; returns 0 committed, 1 committed with old backup retained. |
| 5 INFO | data=RW K32WorkInfo, bytes=64; name/exists/length/hash zero. Returns 0 after copying information, including a workspace error. Optional extension; old hosts return -38. |

`K32WorkInfo` version 1 is 64 bytes: u32 version/size/flags/drive; char root[16];
u32 sector_bytes/cluster_sectors/free_clusters/total_clusters/free_bytes/total_bytes/
max_bytes; i32 error. Drive is zero-based (A=0). Flags are HDD=1, FD=2, READY=4.
The selected drive and root remain meaningful when READY is clear; inspect error
before treating capacity as usable. The picker falls back to the legacy A: label
only when INFO itself is unsupported. Feature bit 128 and the other operations'
wire layouts are unchanged. Existing root-directory calls remain A: read-only;
Files Work navigation is available; see [FILES-EDITOR.md](FILES-EDITOR.md). See [HDD-WORKSPACE.md](HDD-WORKSPACE.md).

Stat before asking to replace; preserve that revision until SAVE. A new name
requires exists=0. On Open, compare the read bytes against STAT before adopting
the document. Revision checks use length plus a non-cryptographic content hash,
not a security guarantee against deliberate hash collisions.

The broker obtains identity from the actual loaded task and feature declaration;
callers cannot select another slot or pass DOS pointers/handles. Entire request,
output and document page ranges are checked. The complete bounded transaction
runs synchronously with DOS calls serialized at the existing real-mode service
boundary, never in an IRQ, protected-mode scheduling or INT2F graphics callback.
During that bounded DOS transfer peers wait; preemptive execution resumes after
it. Critical disk errors temporarily return Fail rather than waiting for a GUI
keyboard prompt; the original INT24 vector is restored on every return.

## Failure and recovery contract

SAVE validates Shift-JIS (including pairs split across copy chunks) before mutation and checks the current revision. It creates
`NAME.TMP` exclusively, checks each write and close, renames an existing target
to `NAME.BAK`, then publishes TMP as TXT. Publication failure attempts rollback.
An existing TMP or BAK is never truncated or silently deleted. Temporary handles
are owned, counted, closed before returning and retried at task cleanup on error.
No open writer persists between user requests. The resident immutable task seed
uses its own DOS allocation to keep the native segment below the existing stack
boundary; per-task snapshots and CPU frame formats are unchanged.

Errors include -38 missing feature/operation, -22 malformed request/name/text,
-14 invalid pointer/permission, -116 stale revision, -17 existing TMP, -28 short
write/disk full, -13 protected file, -5 I/O error, -117 recovery required. A
failed write retains the old TXT; an ambiguous rename/rollback failure preserves
the old complete contents in BAK. Return 1 means the new TXT committed but the
old BAK could not be removed; retain it and resolve it before the next overwrite.

**DOS rename is not power-loss atomic.** A reset between the two renames can
leave TXT absent and BAK/TMP present. Editor Tools > Recovery inspects the three
variants and explicitly moves a selected candidate to an unused TXT name while
retaining the live document; see [RECOVERY-SDK.md](RECOVERY-SDK.md).
The following DOS commands are an alternative for manual recovery. From DOS,
inspect `DIR B:\WORK` (substitute the displayed workspace drive); if `NAME.TXT`
is absent and `NAME.BAK` is the wanted old document, use
`REN B:\WORK\NAME.BAK NAME.TXT`. Inspect/export TMP separately;
do not blindly delete either recovery file. This is recovery of interrupted
saves, not a claim of transactional FAT durability through arbitrary power loss.

## Verification

`make -C tools/k38632 all test` tests the same transaction policy with a 64 KiB
document, empty files, every I/O boundary fault, short writes, stale revisions,
reserved files and failed rollback. `make -C tools/kgui32 all test` covers saved
history/dirty state and real EXE feature acceptance. `KGSAVEB.EXE` and
`KGSAVENO.EXE` execute negative pointer/feature requests in actual CPL3 without
writing. WebNP2 `verify-kgui32-save.mjs` validates real files/FAT allocation,
Save/Save As/cancel/conflicts, cold boot of exported bytes, full disk, peer
progress and complete cleanup on PEGC/Cirrus; legacy suites retain whole-disk
invariance when no saving is requested.

`verify-kgui32-hdd-workspace.mjs` adds the real HDD broker/picker, 64 KiB create
and three overwrites, independent FAT16 validation and cold boot, peer conflict,
full disk, actual short write after directory expansion, protected files, blocked
WORK, missing/foreign HDD, legacy HDD provisioning and a real BIOS write fault.
