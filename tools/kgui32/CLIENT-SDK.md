# KGUI32 graphical client SDK, wire ABI 1

KGCALC.EXE and KGCLOCK.EXE are independent protected-mode applications.
They run in the existing two K32HOST slots (private 4 MiB each), alongside
the dedicated 4 MiB CPL3 desktop. The desktop alone composes pixels and owns
hardware access. Client code never receives another task's pointers, VRAM,
I/O permission or the server's transfer buffer. Configure 16 MiB XMS.

## Build and use

```
make -C tools/k38632 all test
make -C tools/kgui32 all test
```

Start K386.EXE, K32HOST.EXE and KGUILDR.EXE, with KGUI32.EXE, KGCALC.EXE and
KGCLOCK.EXE on A:. The optional WebNP2 KGUI32 image contains all of them.
Welcome is a minimal launcher: Calculator, Clock and DOS Shell use the first
free/owned-ended slot. A full pair reports busy without ending either task.
F2 or Ctrl+K returns to the launcher. The taskbar restores either application.

The calculator uses the existing KDESK fixed-decimal arithmetic (four fraction
digits, overflow/divide-by-zero checks), rebuilt without its real-mode UI.
Use its buttons or keys, for example `6*7=`; C clears, Backspace edits, and Q
exits. The clock reads cached DOS wall time through its service, supports a
12/24-hour button or T, and Q exits. All app computation and event handling
execute in that app's private CPL3 context.

Title drag, bottom-right resize and minimize operate on the managed task
window. Stop/Resume/End/Again and Ctrl+P/R/E keep their existing meanings;
Again restarts the successful root including arguments. Run/Ctrl+L requires
ending the current root before replacing it. Client close/exit releases its
canvas and leaves an ended task window available for Again. The title-bar X
ends the root and closes its task window. F10 always returns to DOS.
Tab, F2/Ctrl+K and Ctrl+L/P/R/E are reserved by the desktop. Other keys,
including Q/T/S, reach the focused client. GUI clients receive Ctrl+C as a
key; text sessions retain their existing interrupt behavior.

## Application API

Include `sdk/kgui.h`, link `sdk/kgui.c` and the existing PM32 CRT, and use the
same app.ld/386/freestanding/no-FPU flags as this Makefile. Package with:

```
python3 tools/k38632/package.py APP.ELF APP.EXE --abi 257 --features 39 --client
```

The PM32 descriptor remains kind 3 / ABI 0101h. Feature bit 32 requires the
GUI-client service; bit 16 remains exclusive to the desktop itself. K32RUN
and KGUILDR reject client packages. K32HOST refuses a client without a live
server before replacing a V86 root or allocating its XMS backing. Ordinary
PM32 programs keep their existing APIs and feature mask 15.

| Function | Behavior |
| --- | --- |
| kg_open(title,w,h) | Create one managed canvas, return positive opaque token. Read actual dimensions with kg_width/height. |
| kg_begin() | Reset this application's pending drawing list. |
| kg_rect / kg_text / kg_button | Append bounded RGB fill, Shift-JIS text or labelled button commands. Button IDs are app-defined integers. See JAPANESE.md. |
| kg_present() | Submit the complete retained frame in one call, up to 40 commands. |
| kg_poll(event) | Return 1 for an event, 0 for none, negative error; always refresh viewport and seconds-of-day. |
| kg_close() | Release this application's canvas/token. Process return releases remaining private resources. |

No per-pixel traps are used. Frames are retained by the server and remain
available during movement, occlusion and minimize. Hidden clients continue
executing; hidden drawing damage is omitted. A resize delivers current canvas
dimensions. If resize races a frame, error -22 leaves the old frame intact;
poll and redraw using the latest dimensions. The sample clients do this.

Mouse events are button transitions with canvas-relative coordinates and the
hit button's control ID; this first API does not stream motion/capture.
Keyboard, resize and focus events have separate types. Each owner has 16
queued events, consecutive resize notifications coalesce, and overflow drops
the oldest record and is counted. The wall-clock field is cached at safe
native loop boundaries, including when no event is queued. No DOS call is
made from the graphical callback.

## Transport and ownership

Client syscall 13 takes EBX=request, ECX=3144 bytes (KgRequest), and returns
EAX signed result. The complete user range must be mapped writable, including
query requests. `sdk/protocol.h` defines version, size and fixed-width fields.
OPEN validates title and preferred size. FRAME validates command count,
operation, signed bounds, 24-bit RGB and NUL-terminated printable text before
changing any retained data. Current viewport bounds are enforced. Return
values include -14 bad range, -22 malformed/bounds, -16 occupied window,
-116 stale window/generation and -38 unavailable/unsupported service.

The host copies the request through its existing validated XMS transfer,
stamps a native-only envelope with actual slot/generation, and calls the
transient INT 2F D394h service. User-supplied slot/generation cannot select an
owner. The server also requires its own loader-owned matching generation and
window token, so a client cannot control another or a recycled slot.
Client permission never grants desktop calls 64..72.

KGUILDR installs the callback only while its desktop is live, preserving the
previous vector. The callback uses a separate D000-DFFF native stack below
the existing protected stack, borrows the loader's 4096-byte buffer and only
validates/copies bounded data. It never enters DOS/BIOS, schedules a task or
renders. Retained frames use a separate 8 KiB conventional allocation rather
than growing the 64 KiB resident host. The native linker guards both modules'
stack boundaries. The desktop receives snapshots through call 71 and posts
owner-checked events through call 72.

Host exit/End/fault notifies the server before reclaiming the task, removing
its token/events. GUI exit/fault/forced stop ends matching owned generations,
restores the vector, frees the arena, and performs the existing graphics,
conventional and XMS cleanup. The single CPU serializes all callbacks at real
mode boundaries; there is no DOS IRQ scheduling or shared writable disk API.
The underlying DOS/driver layer remains real mode, with bounded PM32 slices.

## Verification and limits

Native tests cover command bounds, malformed text/RGB, ownership/stale tokens,
atomic rejection, queue bounds, resize coalescing, EXE parser capabilities and
module/stack layout. KGBAD.EXE issues 14 actual CPL3 negative/identity checks,
including real low/RO/overflow pointers and a supplied real peer token.
`KGUILDR KGUI32.EXE sessionapi` also tests the desktop snapshot/event boundary.
KGPROOF's live DOS parent checks no-server client refusal before its existing
foreign-owner exit/fault and DOS/XMS reclamation scenarios.

WebNP2 verify-kgui32-clients.mjs checks actual app RAM samples, CPU frames,
private CR3/backing, keyboard/mouse calculator results, clock progress, resize,
stop/resume/minimize, negative requests, write #PF and exact restart, GUI+DOS,
vector/arena/conventional/XMS cleanup and full FAT12 preservation on PEGC and
Cirrus. Existing text-session, GUI foundation, legacy and array tests remain.

The preceding sections record the original client SDK baseline. Current widgets,
clipboard, Editor/Work saves and graphical Shift-JIS are described in
[EDITOR-SDK.md](EDITOR-SDK.md), [SAVE-SDK.md](SAVE-SDK.md) and [JAPANESE.md](JAPANESE.md).
The original baseline was one graphical canvas per application in two mixed slots, with ASCII
8x16 text, RGB fills/buttons and read-only files. Multiple windows per client,
text-entry/menu widgets, Japanese/IME, clipboard, image surfaces, save/write
services, more slots and larger heaps are later milestones. No Windows APIs,
resources or binaries are used. Saved user disks and the original kernel are
not changed.


SDK 2 adds opt-in input, widgets and a read-only root browser without changing
the version-1 wire layouts. See [BROWSER-SDK.md](BROWSER-SDK.md) for the controls,
capability negotiation and directory ABI. SDK1 packages remain supported.

SDK3 adds the temporary editor and reusable picker; see [EDITOR-SDK.md](EDITOR-SDK.md) for lifecycle, keyboard and clipboard contracts.
