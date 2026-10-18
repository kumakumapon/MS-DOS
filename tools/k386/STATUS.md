# K386 compatibility and verification record

The merged #16 introduced the experimental PM/V86 runtime. Development continues
in MS-DOS #17 and webnp2 #17; final source pins and CI results are recorded there.
The requested general DOS/BIOS environment is still incomplete, so these PRs
remain drafts until that acceptance boundary is met.

## Implemented in this continuation

- Fresh views contain spaces; control bytes are rendered blank rather than `?`.
- Each DOS window opens the guest KSH command shell. HELP/VER/DIR/TYPE/CD/ECHO/CLS/
  TIME/EXIT use guest DOS services. Console output follows the viewport.
- COM and bounded small MZ loading, relocation/header/entry/stack checks and PSP
  command tails. The historical DOS kernel and NP2kai core are unchanged.
- One child EXEC level with private saved parent RAM, low page, CPU, DTA and
  virtual IF; normal exits and contained faults return to that parent.
- Per-session read-only directory searches and current directory, serialized
  real-mode DOS calls, restored host DTA and strict A: paths/device rejection.
- PUSHF/PUSHFD, POPF/POPFD, IRET/IRETD, user software interrupts and virtual IF.
  PC-98 BIOS keyboard peek uses BH and does not consume input.

## Verification

The initial question-mark display was reproduced in the real wasm desktop.
Host loader tests cover malformed/truncated headers, relocations, memory size,
CS:IP and SS:SP. Initial browser bring-up passes KSH, VER/ECHO/DIR, real MZ
relocation and arguments, virtual flags/IRET, existing P98TEST.COM, and recovery
to KSH after a child page fault. Expanded E2E and final regression/CI results are
recorded in the PRs rather than inferred from unit tests.

The required browser suites inspect actual CPU-generated records, private RAM,
PTE/CR3, independent input/screens and continued background execution. They also
check interleaved DTA searches, parent/child return, bad images, denied writes,
GUI coexistence and the whole exported disk hash. Fixtures written by native
DOS in an isolated browser are compared against their prepared baseline; the
separate core suite still requires the original full image hash unchanged.

## Incomplete compatibility

This remains a DOS-service runtime, not two independently booted DOS kernels.
General COMMAND.COM, full BIOS, asynchronous virtual IRQs, large/overlaid EXEs,
additional DOS heap allocations, environment/FCB/internal DOS structures,
nonstandard open-handle inheritance and nested EXEC beyond one level are
unimplemented. Parent snapshots use conventional memory and may return DOS
error 8. Input is ASCII; KDESK function keys and Tab remain GUI controls.

Shared disk writes and direct GDC/PEGC graphics virtualization are unavailable.
x87/SIMD save/restore is not implemented: FPU use faults within its VM instead
of sharing state. K386 requires real mode and an unowned masked IRQ0; its separate
boot profile omits EMM386/XMS loaders. Normal KDESK remains available unchanged
when the monitor is absent. Existing foreground EXECs pause GUI-driven VM quanta.
