# K32IO: shared Work file broker

Install `K32IO.EXE` before K32HOST/KGUILDR (or standalone K32RUN). It is an
original 64 KiB resident DOS service; neither DOS nor the emulator is modified.
INT2F/D39F discovers `K32IOV01` version 1. It runs at serialized real-mode
boundaries, saves/restores the caller PSP and critical-error vector, refuses
InDOS/protected-mode/reentrant dispatch and retains a checked 16 KiB stack.

PM32 feature 256 and syscall 18 provide the 64-byte version-1 K32Io request.
This feature is independent of GUI, directory and legacy text-save capabilities.
K32RUN/HOST check the complete request and each payload's actual page permissions;
the broker receives only a trusted owner/epoch envelope and conventional buffer.
Eight owner records hold eight read handles and one replacement transaction each.
Public tokens are serialised, positive and owner-specific; DOS handles never
cross the boundary. Live handle counts join the existing resource accounting.

Names are regular ASCII 8.3 files with extensions, in the broker-selected Work
directory on the verified HDD, or A: on the FD profile. No drive/path/device,
hidden/system/read-only file or raw volume write is accepted. TMP/BAK names are
reserved variants. Files are bounded to 1 MiB; each READ/WRITE and each COMMIT
revision-scan step transfers at most 4096 bytes. Return -11 from COMMIT/RECOVER
means submit another step. CPU scheduling returns between calls; DOS is never
reentered and no transaction holds execution across a whole-file scan.

STAT returns existence/length; `sdk/io.c` computes a content revision by bounded
open/read/close. BEGIN captures the expected existence/length/FNV-1a revision,
locks that Work basename and exclusively creates TMP. WRITE only fills TMP.
COMMIT closes TMP, scans the original in bounded steps, checks the captured
revision, then uses BAK/rename/rollback to publish. Later request fields cannot
change the captured revision. A short write poisons the transaction and cannot
be committed. Read streams are live DOS streams, not immutable snapshots: callers close
and verify a content revision before adopting editable data.
Legacy GUI saves/recovery also check broker locks before mutating those names.

ABORT with flags=0 explicitly deletes this transaction's TMP; flags=1 retains it.
SDK save failures and task End/fault retain TMP after closing owned handles.
Failed closes retain authority and prevent owner/backing reuse until retried.
OPEN flags 1/2 reads TMP/BAK; RECOVER verifies a selected candidate's revision in
bounded steps and exclusively renames it to an unused name. Original and other
variants remain untouched. DOS rename is not power-loss atomic; ambiguous rename
failures retain BAK/TMP for inspection. FNV revisions are not cryptographic IDs.

Link `sdk/io.c` and `sdk/libc.c`. They provide stream read/seek/close, revisions,
replacement save/abort/recovery, and minimal getchar/putchar/printf over the
existing task-local keyboard and console. Redirectable descriptors/pipes belong
to the later process/IPC unit. `PRACT32 io` writes/verifies 70001 binary bytes
and an odd read across 64 KiB; `PRACT32 cancel` holds an unfinished write and
`PRACT32 rescue` explicitly recovers it to RESCUE.BIN after End.

`make test` runs the actual transaction state machine with an independent,
faultable disk under ASan/UBSan. Actions additionally exercises actual CPL3,
PTEs, DOS disk contents, cold boot, peer progress, failures and reclamation.
