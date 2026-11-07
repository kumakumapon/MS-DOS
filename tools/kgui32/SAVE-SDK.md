# SDK4: workspace save API and Editor

Use the separate KGUI32 image, 16 MiB RAM, Welcome > Editor (`KGEDIT.EXE`).
Existing KDESK and the original DOS kernel remain separate and unchanged.
Documents remain ASCII, at most 65,536 bytes. Open normalizes CRLF to LF;
saved files use LF. Saving retains selection, cursor and Undo/Redo history.

- **Save / Ctrl+S** saves the current workspace file, or opens Save As.
- **Save As / Ctrl+W** opens the reusable picker, with a filename input.
  Enter an ASCII 8.3 name ending in `.TXT`, then Enter or Save. Tab moves
  between filter, list, filename, Save and Cancel. A selected list row fills
  the filename. An existing destination requires Replace (R) or Cancel (Esc).
- **Open / Ctrl+O** starts in the read-only A: root. Work switches to
  `A:\WORK`; Root switches back. Filtering, list navigation and Open work in
  either location. Root files can be edited, but Save As writes only to Work.
- X, F10, New and Open offer **Save (S), Discard (D), Cancel (Esc)** for a
  modified document. Canceling either a save picker or replacement also cancels
  the pending close. Save failure retains document, history and dirty state.
- Another editor changing the same file makes an ordinary Save fail with
  `File changed`; reopen it or explicitly Save As. It is never silently replaced.
- End / Ctrl+G remain forceful operations that discard unsaved changes.

Writes are restricted to writable, ordinary `.TXT` files in the **existing**
`A:\WORK` directory. The generated KGUI32 disk provisions an empty Work directory.
Available capacity depends on free FD space; replacing a file needs additional
space for the new temporary copy. The size limit is not a free-space guarantee.
Root boot files, EXEs, devices, paths, other drives, hidden/system/read-only
files and arbitrary subdirectories are outside the write API. No mkdir/delete
or arbitrary write-handle API is exposed. SDK1–3 calls and wire layouts retain
their contracts. General Japanese/IME, host clipboard and extra slots are later
work. The browser's existing disk persistence/export saves the modified image;
export a copy before resetting it to the bundled original.

## Broker and ownership

Native `k38632/workspace.h` defines feature **128**, supported only by K32HOST
clients also declaring GUI=32 and directory=64. KGEDIT declares **231** and
queries **239**. Old clients query their previous features. K32RUN and KGUILDR
reject save-enabled client packages. There is no new privileged I/O in clients.

INT30h **17**, EBX=RW `K32Work`, ECX=64; EAX=result. The 64-byte version-1
request has u32 version/size/operation/flags, name[16], then u32 data/bytes/
exists/length/hash/reserved[3]. Flags/reserved must be zero. `sdk/k32x.h`
provides `k32_work`, `k32_work_name` and `k32_work_first`.

| Operation | Request / result |
| --- | --- |
| 1 STAT | Name; returns 0 and exists/length/FNV-1a of raw bytes, bounded to 64 KiB. |
| 2 OPEN | Name; returns the existing owned read-only handle token; use read/seek/close. |
| 3 FIRST | data=RW K32DirEntry, bytes=64; enumerates Work TXT files. Use existing NEXT/CLOSE. |
| 4 SAVE | Name, data=readable document, bytes<=65536, expected exists/length/hash; returns 0 committed, 1 committed with old backup retained. |

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

SAVE validates ASCII before mutation and checks the current revision. It creates
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
leave TXT absent and BAK/TMP present. First export the disk for backup. From DOS,
inspect `DIR A:\WORK`; if `NAME.TXT` is absent and `NAME.BAK` is the wanted old
document, use `REN A:\WORK\NAME.BAK NAME.TXT`. Inspect/export TMP separately;
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
