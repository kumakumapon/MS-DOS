# KDESK 1.5: scrollbars and file icons

Files, Launcher, Memo, Help and About have independent vertical scroll positions.
A scrollbar appears when their content exceeds the available rows. Click the
arrows for a row, the track for a page, or drag the proportional thumb. Changing
focus retains the position. Resizing clamps it to the new content range. Icons
reflow into columns and text wraps, avoiding horizontal clipping. Calculator and
Clock already fit within their enforced minimum sizes and need no scrollbar.

## File manager

The File manager launcher icon now opens the built-in Files window. F3 and the
Files menu open the same window. Folders, documents, programs, BASIC files and
BMP images have file-type icons. A single click selects; double-click, Enter or
Open opens a folder or an associated application. Backspace/Parent goes up.
COM/EXE, TXT, BAS and BMP retain their existing DOS program associations.

V or List/Icons toggles the list view. Arrows select items; Up/Down moves a grid
row, Left/Right one item. Page Up/Down moves a page and Home/End selects an end.
Keyboard navigation brings the selection into view, while mouse scrolling lets
you browse without changing it. Up to 128 directory entries are displayed; the
existing `128+` indicator identifies truncated listings.

F or FD98 opens the existing two-pane filer in the current directory for copy,
move, rename, delete and directory creation. The button is hidden in narrow
windows; F remains available. DOS tools run in the foreground and return to the
desktop after a key or click. Files itself does not introduce drag-and-drop or
new destructive file operations.

## Text windows

Memo, Help and About support arrows, Page Up/Down and Home/End to scroll without
changing text. Memo keeps its existing append-only editing behavior: typing,
Backspace, Cut or Paste reveals the final line again. Copy and Cut still use the
whole memo. Scrolling never saves implicitly; Save/Ctrl+S and the existing safe
replacement/unsaved-change dialog retain their behavior. Japanese and larger
files remain associated with EDIT98.

## Validation

`make -C tools/window all test` builds the COM files and tests scrollbar bounds,
monotonic thumb movement, endpoints and short tracks, alongside calculator tests.
The linker continues to reserve at least 12 KiB of stack. Memo I/O uses temporary
stack storage instead of occupying the COM's data segment throughout the session.

`tools/window/verify-ui.mjs` runs the native guest in WebNP2/Chromium. It checks a
28-file directory in both views, arrow/page/thumb scrolling, focus persistence,
keyboard paging, minimum-size reflow, icon selection/opening after scrolling,
FD98 launch/return, Memo scrollback/discard and wrapped Help/About. The existing
EXEC, mouse, clipboard, image, calculator/clock and frame-sampling redraw checks
continue to run. The same verifier is copied to webnp2 for its bundled-image CI.
