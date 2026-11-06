# SDK3: temporary editor and desktop contracts

Status: implementation under validation. Use the separate KGUI32 image with
16 MiB RAM; Welcome > Editor or Run > KGEDIT.EXE. Each editor is a CPL3 client
in one of the two 4 MiB mixed slots; the desktop remains separate.

Documents are temporary ASCII text, at most 65,536 bytes. There is no disk save.
Open reads a regular A: root 8.3 file, validates all content before replacing
the document, removes CR and retains LF/tab. Unsupported bytes, binary content,
read/close failures and oversized files leave the original document intact.
An empty file is valid. Tabs display as one space. Text wraps to the viewport,
up to 47 columns and 14 visible rows, within the 40-command frame budget.

## Controls

- Type/Enter/Tab, Backspace/Delete and navigation keys edit the document.
- Ctrl+A selects all; drag selects a range. Ctrl+B toggles a keyboard mark;
  navigation extends the selection while marked. This is not Shift+arrow input.
- Ctrl+C/X/V copy/cut/paste. The guest clipboard holds at most 1,024 bytes.
  Oversized copy/cut preserves the document and clipboard. Invalid paste or
  capacity overflow leaves the document intact. No host clipboard is accessed.
- Ctrl+Z/Y undo/redo, bounded to eight document states (seven undo transitions).
  A new edit drops redo. Undo to the loaded text clears the Modified indication.
- Ctrl+N/New and Ctrl+O/Open ask before replacing modified text. The reusable
  picker supports filtering, Tab, list navigation, scrollbar and Open/Cancel.
  It enumerates up to 192 root entries using the existing task-owned DTA/cursor.
- Title X requests normal close. Modified text offers Discard (D) or Cancel
  (Esc). F10 negotiates desktop exit with all SDK3 clients. Cancellation keeps
  the desktop running; another client may already have accepted and exited.
- F2/Ctrl+K focus Welcome. Stop/Resume/End/Again remain desktop buttons. End
  forcefully discards the process; Ctrl+G is a native emergency desktop stop that discards all owned tasks,
  even if the desktop is nonresponsive. F2 then Esc also forces desktop exit.

## ABI extensions

Wire ABI1 layouts are unchanged. KG_CAPABILITIES now advertises bits 0..3.
SDK1/2 subscriptions (bits motion=1, local Tab=2) keep their behavior.
KG_SUBSCRIBE additionally accepts close=4 and editing keys=8. Subscribed editing
clients receive Ctrl+L/P/R/E rather than desktop command interception; F2,
Ctrl+K and F10 remain desktop controls; Ctrl+G is reserved for native emergency stop. GUI controls still force-end/pause tasks.
KG_QUIT event type 6 has key=1 for title X, 2 for desktop exit. An accepting
client calls kg_close and returns; a cancelling client calls kg_cancel_close.
A paused client must resume to answer; force End remains available.

Operations 7/8 implement clipboard set/get through the existing validated whole
KgRequest: count is bounded to 1024 and bytes occupy the draw array. No pointers
are exchanged. All operations require the actual task generation/window token;
operation 9 cancels close and requires an opt-in close subscriber. Cancellation
updates a sequence in the desktop snapshot; stale generations cannot cancel
another process. Clipboard data is copied to a dedicated region of the
loader-owned conventional arena, which is now 12 KiB and freed on GUI exit.
No DOS/BIOS calls or scheduling occur inside the graphical callback.

sdk/editor.h supplies the bounded document/history model. sdk/picker.h supplies
an application-local reusable file picker: open, tick, event, draw and close.
Call tick incrementally, always close on cancellation/termination, and forward
resize/focus/input events. Its file output is an ASCII basename, not an OS handle.

Tests must cover actual CPU/private RAM, normalized FAT contents, independent
clipboard consumers, dirty close/exit cancellation, keyboard reservations,
resource recovery and whole-disk invariance on PEGC and Cirrus. See PR records
for completed checks; this document alone is not an acceptance claim.
