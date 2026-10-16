# KDESK calculator, clock and icon launcher (1.3)

Start `KDESK` at the MS-DOS 4.0 PC-98 prompt in WebNP2, then open Calculator or
Clock from Apps (F2). Both are built-in desktop windows, so the clock continues
updating while you calculate or edit a memo. Minimize with F8, restore with the
taskbar, close with Escape, and move/resize using the existing desktop controls.
Their minimum sizes keep the controls visible.

## Calculator

Use the mouse keypad or type digits, `.`, `+`, `-`, `*`, `/` and `=` (or Enter).
Operations follow entry order: `2+3*4=` gives 20. Repeated Equals repeats the last
operation. The display accepts eight integer and four fractional digits, from
-99999999.9999 to 99999999.9999. Fixed decimal arithmetic avoids binary floating
point errors such as `0.1+0.2`; multiply/divide round to four fractional places,
with halves away from zero. Overflow and division by zero show an error; C or a
new digit recovers.

| Control | Action |
| --- | --- |
| C | Clear calculation; retain memory |
| CE | Clear current entry; retain pending operation |
| BS / Backspace | Remove last digit |
| +/- / S | Change sign |
| % | Divide current entry by 100 |
| MC / MR | Clear / recall memory |
| M+ / M- | Add / subtract current entry to/from memory |
| Copy / Ctrl+C | Copy the result to the shared desktop text clipboard |
| Paste / Ctrl+V | Paste a signed decimal number; reject invalid/oversized input |

The clipboard is shared with Memo and WebNP2's KDESK Clipboard and Images panel.
Use Get text in that panel to copy the result to the host. Host Paste to DOS
still targets Memo; copying its number then pasting in Calculator also works.
C retains memory until MC or desktop exit. Closing/reopening the calculator
retains its state for the current KDESK session.

## Clock

The clock shows the DOS date, weekday, digital time and analog hands, derived
from the emulator RTC. It is a display app; use DOS/settings controls to change
the system time. T or the 12-hour/24-hour button toggles AM/PM. Copy or Ctrl+C
copies `YYYY-MM-DD HH:MM:SS` with AM/PM in 12-hour mode. Time updates while
other desktop windows are active and refreshes after returning from an external
DOS program. External DOS programs still run in the foreground one at a time.

## Icon launcher

Apps (F2) opens the icon grid by default. Click once to select, double-click or
use Open/Enter to start. V or List/Icons toggles the existing list, where a single
click starts a tool. Left/Right moves one icon; Up/Down moves a row. Home selects
the first entry. Resizing changes the column count; the selection scrolls into
view, including Calculator and Clock at the end of the launcher.

## Validation

`make -C tools/window test` covers decimal arithmetic, rounding, repeated
operations, range errors, memory and clipboard parsing. The Chromium guest
verifier exercises mouse launch/keypad/memory, Memo clipboard exchange, safe
resize, clock progression alongside Calculator, 12/24-hour switching, copy,
minimize/restore, and icon/list navigation in normal and narrow windows. Existing
window, image, DOS launch and persistence checks run in the same suite.
