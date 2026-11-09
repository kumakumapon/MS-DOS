# Files / Editor integration

MS-DOS #31 stage 4a combines Work browsing and document launch in one change.
The matching distribution and PEGC/Cirrus acceptance runner live in webnp2.

## Usage

Files starts at the read-only A: root. **Work** / **Root** or **Ctrl+W** switches
between that root and the broker-selected Work (verified HDD, or explicit FD
fallback). Work lists only TXT documents. Filtering, Refresh, TXT preview and
Back retain their existing behavior. Refresh reloads directory metadata after
another application saves. The **Editor** button or **Ctrl+O** opens the selected
TXT in a free peer slot. Documents larger than 64 KiB are refused before launch.
The list/preview shows at most 18 rows so all controls fit the 40-command frame.

Editor receives the root/work namespace and filename. Work documents use the
existing revision-checked save path; root documents require Save As into Work.
The desktop still has two application slots. If the peer is running, waiting or
stopped, Files displays “Both slots in use: close a task”. An existing Editor,
including unsaved text and undo history, is preserved. Close it explicitly or
save the document and end the task before opening another document from Files.

## Broker contract

`KG_EDIT_DOCUMENT` carries namespace 0 (root) or 1 (Work) in `count` and a bounded
8.3 TXT filename in `title`. `KG_EDIT_STATUS` returns -11 while pending, zero
when launched, or a negative error. These calls retain the graphical ABI's
packet size and token/generation authentication. Names reject paths, DOS devices,
wildcards, invalid characters and non-TXT extensions, and are canonicalized.
The command is constructed internally as `KGEDIT.EXE root NAME.TXT` or
`KGEDIT.EXE work NAME.TXT`; callers cannot choose an executable or arbitrary args.

The INT 2F callback only validates and queues. A later GUI broker tick verifies
that the requestor is still owned, live, and has the same token/generation,
then launches into a free peer or the GUI's own faulted peer. It never replaces
a running/waiting/stopped or foreign faulted task. Client close, lifecycle end,
and a new generation invalidate pending requests. No DOS call, EXEC or scheduler
runs inside the interrupt callback. Serialized snapshots reuse loader wire
scratch so the existing real-mode stack/memory bound remains intact.

Editor validates startup arguments and reuses its load/revision/save path.
Missing, binary, over-limit or unavailable documents leave an empty clean Editor
with an error message. No host-side writer participates in the live operation.

## Verification

`make -C tools/kgui32 all test` covers command validation, stale tokens/owners,
queued/duplicate requests, close cancellation, occupied peers and fixed launches.
`node scripts/verify-kgui32-files-editor.mjs BASE OUTPUT pegc` in webnp2 (also
`cirrus`) boots the pinned native build and checks Work preview -> Editor -> save
-> Files refresh/preview -> reopen, dirty occupied peer preservation, cold boot,
large/binary rejection, read-only root Save As and FD Work fallback. Exported
FAT12/FAT16 media are checked independently, including both FAT copies, allocation,
protected app/root contents, unchanged distribution FD, and full DOS/XMS/bridge
cleanup. Recovery UI, arbitrary paths, extra slots and Japanese editing remain
separate stages. Merge the paired native PR before its pinned webnp2 PR.
