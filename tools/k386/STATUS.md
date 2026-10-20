# K386 compatibility and verification record

Merged PRs #16-18 provide the experimental guest PM/V86 runtime and DOS process
support. The resource-control continuation follows in MS-DOS #19 and webnp2 #19.
These PRs record the exact source pins, CI/browser results and remaining work.

## Implemented

- Two isolated V86 contexts, locked XMS-backed conventional RAM, private IVT,
  TVRAM, text windows, directories/search state and input queues.
- CPU-generated PM/paging/#UD/#PF/V86 records, PIT preemption, virtual IF,
  bounded exception containment and safe real-mode return.
- KSH plus bounded COM/MZ execution, private PSP/environment/trusted DOS arenas,
  four EXEC levels, inherited read-only handles and shared file cursors.
- Bounded operation of the bundled original COMMAND.COM, exercised explicitly
  by the process suite; this is not general DOS/BIOS compatibility.
- Private PC-98 BIOS one-shot timers with waiting wakeups and ownership across
  EXEC. FDXMS286 supplies the monitor's private XMS backing; EMM386 is omitted.
- Sanitized DOS text cells, existing window/mouse/scroll/clipboard/GUI apps and
  normal KDESK fallback.
- The #19 continuation adds monitor-owned weighted scheduling, CPU time budgets,
  resource/generation records, a bounded event journal and current-program
  interruption. See RESOURCES.md for controls, ABI, accounting and boundaries.

## Verification

Native loader/arena/resource-policy tests run with the guest builds. Existing
Playwright suites inspect actual CPU frames, page tables, CR3, PSP/module RAM,
independent input/screens, continued background work, timers, handles and file
contents. The resource suite runs KRESCHK.COM inside real DOS, then exercises
GUI controls and monitor-owned records. Tests check the full exported disk hash;
no synthetic browser DOS processes or fabricated CPU snapshots are used.

Verification status belongs to the PR/CI for the exact source revision, not to
an assumption that all implemented paths already pass. Normal GUI, core, shell,
process and resource suites remain separate regression gates.

## Incomplete compatibility

This is a DOS-service runtime, not two independently booted DOS kernels. Full
BIOS/IRQ/TSR behavior, arbitrary COMMAND programs, large/overlaid EXEs, full NLS
and Japanese DOS input, complete FCB/internal DOS structures and guest XMS/EMS
APIs remain incomplete. Maximum EXEC depth is four; arenas and handles are bounded.

Shared disk writes and direct GDC/PEGC virtualization remain unavailable. Host
DOS broker operations serialize read-only disk access. x87/SIMD is not saved:
use faults inside its VM. Existing foreground real-DOS EXEC pauses GUI-driven
scheduling. The monitor requires real mode and masked/unowned host IRQ0.
CPU budgets measure emulated execution/service time, with bounded-quantum
overshoot, rather than a hard wall deadline or browser CPU percentage.
