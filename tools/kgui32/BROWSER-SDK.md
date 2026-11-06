# GUI SDK 2 / Files

`KGFILES.EXE` is an independent CPL3 application, built with `make -C tools/kgui32 all test`. Open **Files**
from KGUI32's Welcome launcher, or use a slot's Run field (`Ctrl+L`) and enter
`KGFILES.EXE`. Clock, Calculator or DOS may occupy the other mixed slot.
The dedicated GUI remains separate. Configure 16 MiB; there are still two
application slots, each with a 4 MiB total code/data/heap/stack budget.

## Using Files

- Type in Filter to match an ASCII 8.3 name, ignoring case. Tab cycles Filter,
  List, Open/Back, Refresh/List, About, Close. Click a row to select it; Enter
  in the list or Open previews a TXT file in the **same window**.
- Up/Down, Home/End and Page Up/Down navigate the focused list or preview.
  On PC-98 these correspond to the cursor, CLR/HELP and ROLL keys. The vertical
  scrollbar supports arrows, track paging and thumb dragging, including release
  outside the canvas. A focus change or resize cancels a drag.
- Esc/Back returns from preview, retaining the filter and list position. TXT
  previews load at most 128 KiB, wrap at 40 columns, render ASCII, replace tabs
  with one space and display non-ASCII bytes as `?`. Empty files are supported.
  Binary control bytes, non-TXT files, directories and larger files show a
  dismissible dialog. There is no write, rename, delete, EXEC or subdirectory API.
- F2/Ctrl+K focuses Welcome. The taskbar switches apps. The existing title bar,
  resize grip, minimize, Stop/Resume, End and Again controls remain available.
  `Ctrl+P`, `Ctrl+R`, `Ctrl+E`, `Ctrl+L`, F2 and F10 remain desktop shortcuts.
  Close ends only Files. F10 exits the desktop and returns to DOS.

## SDK contracts

`sdk/widgets.h`, `widgets.c` and `widgets-draw.c` provide SDK version 2 input,
list, scrollbar, button, modal dialog and local-focus policy/rendering. They
run in the application, without native addresses, DOS calls or privileged I/O.
The retained frame still has at most 40 bounded commands; text helpers clip to
47 characters and the available canvas columns. Apps relayout on resize.

The version-1 `KgRequest`/`KgDraw`/`KgEvent` layouts and old calculator/clock
packages are unchanged. New operations `KG_CAPABILITIES` (5) and
`KG_SUBSCRIBE` (6, request.count bitmask) negotiate `KG_INPUT_MOTION=1` and
`KG_INPUT_TAB=2`. They require the live window token and actual slot generation.
New `KG_POINTER=5` events carry canvas-relative signed x/y and current buttons.
`KG_MOUSE` remains a button transition; `KG_FOCUS.key` is 1 on gain, 0 on loss.
Unsubscribed clients retain SDK1 Tab and mouse behavior. Motion is coalesced
and dropped before the bounded queue fills, reserving space for transitions.
Capture belongs to the exact root generation/token and is canceled on focus,
resize, hide/end or replacement; it cannot transfer to a reused slot.

## Read-only directory ABI

`tools/k38632/directory.h` defines feature bit 64 and `K32DirEntry` version 1,
exactly 64 bytes: seven 32-bit fields (`version`, `size`, `cursor`, `attributes`,
`bytes`, DOS `date`, DOS `time`), a 13-byte NUL-terminated ASCII name and 23
zeroed reserved bytes. The feature is explicitly declared by a hosted client;
Files declares 103 (startup/heap/input/GUI/directory). Its query returns 111,
including the broker's existing read-only file feature. Old apps still query 47.
The standalone runner and dedicated GUI loader reject directory packages.

| SVC | Registers | Result |
| --- | --- | --- |
| 14 FIRST | EBX=output, ECX=64 | 1=entry, 0=EOF, negative error |
| 15 NEXT | EBX=cursor, ECX=output, EDX=64 | 1=entry, 0=EOF, negative error |
| 16 CLOSE | EBX=cursor | 0, or negative error |

FIRST replaces the caller's previous cursor after size/range validation. Output
includes the cursor even on an empty root/EOF; close it explicitly. NEXT after
EOF remains EOF until close. Tokens are scoped to the current task, not globally
unique handles. Close/reopen invalidates old tokens; one directory cursor per
slot and eight existing file handles remain bounded. Missing feature returns
`-38`, bad size `-22`, invalid/RO/overflow output `-14`, stale/foreign cursor
`-9`, and DOS/transfer errors `-5`. No caller-supplied path is accepted.

The search includes hidden/system entries and directory metadata, excluding
volume labels, devices and names outside the existing safe 8.3 policy. The
43-byte DOS search DTA and cursor/EOF belong to the slot snapshot; every find
saves/restores the process DTA. Regular-file validation uses separate scratch.
DOS calls execute only in the existing real-mode service boundary, never in
INT 2F graphical callbacks or IRQ scheduling. End/fault releases preview pages,
file handles and cursor ownership; replacing the root starts from the clean seed.

## Validation and diagnostics

`make -C tools/kgui32 test` exercises widget navigation, capture/queue/ABI policy
and actual EXE parsing. `KGDIRBAD.EXE` executes 23 real CPL3 boundary checks;
`KGDIRNO.EXE` verifies the three calls refuse a client without feature 64.
`KGFILES.EXE fault` loads the BROWSE.TXT integration fixture into private heap
pages, then deliberately writes to 0x10000 to test #PF cleanup. It is a diagnostic
argument, not needed for normal use.

The first 64 bytes of Files RW memory contain `KGF2` diagnostics sampled by the
CPU: frames/events/focus/selection/counts/top/mode/content bytes+FNV-1a hash,
heartbeat/error/selected-name hash/directory metadata hash. The metadata hash
folds each actual entry's name hash, size, attributes, DOS date and time in FAT
order. These values inspect actual CPL3 data, not executable-container bytes.
WebNP2's browser suite compares them with raw FAT/file bytes on PEGC and Cirrus,
and checks background time, input isolation, stop/resume, generation changes,
malformed/stale requests, heap/handle cleanup and complete disk invariance.

Saving, IME, full Unicode, subdirectory traversal, image surfaces, additional
slots and larger per-app budgets remain later work. The original kernel,
NP2kai core, V86 ABI and legacy KDESK fallback are maintained separately.
