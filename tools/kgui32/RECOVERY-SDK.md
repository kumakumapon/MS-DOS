# Work recovery and Editor tools

Welcome > Editor > **Tools** exposes Find, Replace, Recovery, Use LF and Use CRLF.
Ctrl+F opens Find, F3 repeats it, Ctrl+H opens Replace and Ctrl+T opens Recovery.
Backspace still deletes text. Shift with arrows/Home/End/Page keys extends the
selection. Modifier metadata is opt-in (KG_INPUT_MODIFIERS=16); old clients see
their existing keys and zero modifier control. Request/event sizes are unchanged.

Find is literal and case sensitive, searches forward and wraps. A failed search
or Cancel leaves the document unchanged. Replace changes the selected match;
All preflights the resulting size and commits one undo step. Empty replacement
is allowed. An empty query or over-limit result preserves text and history.

Open detects uniform CRLF and retains it as the selected format; LF is the
other explicit choice. The document model uses LF internally. Format changes
participate in dirty state and Undo/Redo. Save serializes the chosen format,
checks the serialized 64 KiB bound before mutation, displays the encoded temp
space requirement and records the raw on-disk revision. Mixed LF/CRLF opens as
LF; bare CR and malformed Shift-JIS are rejected without replacing the document.
See [JAPANESE.md](JAPANESE.md) for whole-character movement/search, Japanese
previews and the bounded guest clipboard input to Find/Replace.

The document viewport uses up to 92 columns and 21 rows at the largest canvas.
Long rows use multiple bounded text commands. Dense long-line views reduce the
visible row count to fit the existing 40-command frame; selection uses at most
three rectangles. Resizing and scrolling preserve the document and history.

## Explicit recovery

Recovery runs inside the existing Editor slot, preserving any unsaved document.
The list combines Work's TMP/BAK candidates by their TXT base. Inspect shows
TXT/TMP/BAK presence and read-only previews, with page scrolling. It does not
write or adopt the preview as the live document. Export a backup of the disk
before recovery if its state is uncertain.

Select TMP or BAK, enter an **unused 8.3 TXT name**, choose Restore, then confirm
Move. The candidate is renamed to that name, preserving all bytes; existing TXT
files and other candidates are untouched. If TXT is missing its original name
can be used. If TXT exists, recover the backup to another name such as
OLDREV.TXT. Process each remaining candidate explicitly; the original save base
becomes available once its reserved TMP/BAK names have been resolved. Refresh
Files or use Editor Open to read the recovered TXT.

Cancel does not mutate media. Protected files, stale state, invalid names and
existing destinations are refused. Recovery never truncates or automatically
deletes a candidate. A failed DOS rename keeps the source; arbitrary power loss
and partial physical media failures remain outside FAT transaction guarantees.

## Bounded Work extensions

Feature 128 and the 64-byte version-1 K32Work wire stay unchanged. New operations:
6 REC_INFO, 7/8 REC_OPEN_TMP/BAK, 9/10 REC_RESTORE_TMP/BAK, 11/12
REC_FIRST_TMP/BAK. FIRST returns the existing owned directory cursor; use
NEXT/CLOSE. OPEN returns the existing owned read-only file token.

REC_INFO uses a writable 64-byte K32RecoveryInfo at data/bytes. It contains
version/size, three 16-byte revisions (TXT, TMP, BAK: exists/length/hash/error),
and two zero reserved words. Errors are per variant so an unavailable or
protected candidate remains distinguishable from an absent file.

RESTORE reads an 80-byte K32RecoveryRestore: destination[16] plus the expected
64-byte state. All three revisions are checked again at the serialized DOS
boundary. Only a selected TMP/BAK can become a regular TXT in the same selected
Work directory. The destination must not exist. There is no arbitrary drive,
path, delete, truncate or rename service. Existing ownership/page validation and
INT24 restoration apply. Errors include -116 stale, -17 destination exists,
-13 protected, -27 over limit, -14 bad user range and -38 unsupported feature.

SAVE flag K32_WORK_CRLF=1 accepts strict CRLF bytes; flag zero retains LF-only
validation. Other flag bits remain invalid. Checks span transfer chunks, and a
trailing CR or unpaired LF in CRLF mode is refused before filesystem mutation.

## Validation

Native policy tests inject each recovery I/O failure and check original bytes,
revision and namespace refusal, CRLF chunk boundaries and text/format history.
KGRECNEG/KGRECNON issue real CPL3 negative pointer, header, name, revision and
feature requests without writes. Paired webnp2 CI covers the UI, exported
FAT12/FAT16 content/allocation/protected files, restart and complete reclamation
on PEGC/Cirrus. Browser tests run in GitHub Actions; npm run preview is not run
in this chat. Refer to the paired PRs for current verification results.
