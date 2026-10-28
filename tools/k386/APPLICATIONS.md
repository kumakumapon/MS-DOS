# KDESK/386 text applications (ABI 1.0)

The enhanced disk uses KSH.EXE and original MZ applications in isolated V86
sessions. These are actual DOS MZ load modules, with an explicit entry point,
stack/allocation bounds and DOS relocation table, not renamed COM binaries.

## Use

- Ctrl+N / Ctrl+O opens a shell or focuses its existing session.
- The enhanced desktop's DOS shell launcher opens KSH.EXE in a free session.
- Ctrl+K finds **Hello KDESK/386**, **Background counter**, **Keyboard echo**.
- Open a tagged EXE in Files to start it in an ended/faulted slot. A running,
  waiting or stopped session is never replaced by this action. If both slots
  are occupied, end one with Tasks or Ctrl+E in its DOS window, then retry.
- Inside KSH, `NAME [arguments]` tries `.EXE` first and `.COM` second, only when
  the EXE is missing. Explicit extensions are honored. `q` ends COUNTER;
  Escape ends ECHO. A child returns to its saved shell and private environment.
- Ctrl+L explicitly replaces the current DOS session with a named program.
  Invalid MZ/ABI metadata is checked before replacing a live session.

The 18 application executables are KSH, KHELLO, COUNTER, ECHO, KFILE, FLAGS,
DTA, KEYCHK, TIMER, MEMORY, PROCESS, INTWAIT, DF, FAULT, PORT, FPU, BADTIMER and
ROMWRITE. The latter protection diagnostics deliberately fault. K386CPU,
K386TEST and KRESCHK remain real-DOS COM diagnostics; COMMAND.COM and P98TEST.COM
remain compatibility inputs. The old V86 COM builds are test fixtures only;
the enhanced image does not ship them alongside the new applications.

## Build an application

`hello.c` is the minimal C example; `sdk.h`, `app-start.S`, `app.ld` and
`../mz/build.py` form the SDK. From this directory:

```sh
make all test
# For your own source, using the same default GCC calling convention:
gcc -m16 -march=i386 -Os -ffreestanding -fno-pie -fno-stack-protector \
  -fno-asynchronous-unwind-tables -fno-builtin -ffunction-sections \
  -fdata-sections -DDOS -c hello.c -o build/myapp.o
ld -m elf_i386 --gc-sections -T app.ld build/myapp.o build/app-c-start.o \
  build/app-dos.o build/core.o -o build/myapp.elf
python3 ../mz/build.py --k386-app 'My application' build/myapp.elf build/MYAPP.EXE
```

Use the provided DOS text/file wrappers and `kapp_session()` / `kapp_yield()`.
C uses GCC's normal stack calling convention, 32-bit near pointers with 16-bit
segments; do not mix KDESK's regparm=3 objects with SDK objects. Code, data and
stack share a 64 KiB module; the linker reserves a 12 KiB minimum stack. The
PSP is a different segment. Startup initializes BSS and establishes DS=ES=SS=CS
with FS=PSP. Assembly samples use explicit `fs:` for command tails/environment.
DS/ES may subsequently be changed by the app. General segmented MZ remains
supported by the existing loader, but this SDK's flat module has the above limit.

Start checks the V86-only application service before entering user code. A
launch from real DOS prints `Open this application in KDESK/386.` and exits1,
even when the K386 resident monitor is installed. The host monitor API and
application API have separate multiplex IDs.

## Header and service contract

MZ bytes0..27 remain standard. Offset28 contains a44-byte descriptor:

| Offset | Length | Value |
| --- | --- | --- |
| 28 | 8 | ASCII `K386APP1` |
| 36 | 2 | little-endian ABI version0100h |
| 38 | 2 | kind 1: V86 text; kind 2: exclusive desktop tool |
| 40 | 32 | printable ASCII title, 1..31 bytes, NUL terminated |

The relocation table starts at72; the standard MZ table offset and header size
include the descriptor. The monitor and native Files launcher use the same
validator. Unknown versions/kinds, invalid titles and malformed bounds fail.
The descriptor selects execution behavior; it grants no extra privileges.

V86 `INT2Fh AX=D388h BX=0` queries the service; BX=1 yields to the monitor.
Both return AX=388Dh, BX=0100h, CX=1 (read-only file services), DX=session1 or2,
with carry clear. No host addresses are exposed. `kapp_yield()` relinquishes the
current slice; preemption also applies to programs which never yield. A DOS
blocking input request waits without busy polling. Other services remain the
bounded DOS/PC-98 BIOS services documented in DESIGN.md.

## Boundaries and validation

This is the external **text application** contract. Calculator, Clock, Memo,
Files and other native GUI tasks still live in KDESK.EXE. External graphical
window/widget APIs and simultaneous GDC/PEGC apps are not implemented.

FD98.EXE, EDIT98.EXE (EDIT.EXE alias), CPANEL.EXE, KVIEW.EXE and KPAINT.EXE
use descriptor kind 2: an exclusive full-screen desktop tool. Their MZ modules
have separate code/data/stack segments and retrieve arguments from their PSP.
KDESK Launcher/Files run them through host DOS and return directly to the saved
desktop after exit, preserving file writes and mouse/clipboard handoff. They
also run from real DOS. Loading kind 2 in a V86 session returns DOS error 50
before replacing live memory; KSH explains how to open it from the desktop.
Their execution temporarily pauses V86 scheduling. They are not graphical
window plugins or independently running background GUI applications.

Build the tools with `make -C tools/filer all`, `make -C tools/editor all`,
`make -C tools/control all` and `make -C tools/window all`. Their linker/startup
contract is in `tools/mz/native.ld` / `tools/mz/start.S` (the graphics tools
share KDESK's equivalent layout); package with `--desktop-app TITLE`.
WebNP2's `verify-native-tools.mjs` validates real saves, exclusive handoff,
retained V86 identities and the incremental text raster in both display modes.

Two V86 sessions and four EXEC levels remain the limit. Their file services are read-only;
there is no shared-disk write permission, unrestricted hardware I/O, general
Windows EXE support or full DOS/BIOS compatibility. See STATUS.md.

Native tests cover header/relocation/ABI rejection and memory/resource policy.
WebNP2's `verify-k386-apps.mjs` tests real CPU segments/PSP/relocated RAM, renamed
EXE file-icon launch, simultaneous apps, busy-slot refusal, termination/reuse,
wrong-ABI rejection, input isolation, native GUI coexistence and unchanged disk
bytes. Existing COM, shell, process, timer, fault and GUI suites remain gates.

In the enhanced desktop, the shipped EDIT98/EDIT, CPANEL, KVIEW and KPAINT
root entries open cooperative GUI implementations; FD98 opens Files actions.
Their standalone MZ descriptors remain kind 2 and are still rejected by V86.
Other kind-2 programs use foreground EXEC. See ../window/WINDOW-TOOLS.md for
window controls, code banks and limits.
