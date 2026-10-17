# Development acceptance record

This branch is experimental and remains a draft.

Verified in shipped NP2kai wasm using Playwright and the PC-98 guest:

- Standalone K386CPU: protected-mode enter/leave; #UD and #PF recovery; paging;
  two V86 executions at the same virtual address using separate physical pages;
  #GP interception of INT/CLI; return to DOS.
- Cooperative task queues: fairness, bounds, stop/resume, exit and fault/restart
  host tests pass. GUI integration preserves input target ordering in a batch.
- Task manager: real guest stop/resume/end/restart of Calculator with Clock.
- Existing complete desktop/media/utility/clipboard/persistence suite passes;
  frame sampling observed zero menu flashes in 889 displayed frames.

Additional verified execution tests:

- Resident K386 test driver now passes dual V86 COM execution, IRQ0
  preemption, separate CR3/RAM, stop/resume, #PF isolation and keyboard input.
  The supervisor IRETD fault was traced to inherited NT flags and corrected.

Verified guest GUI: two DOS windows run V86 programs with distinct CR3 and RAM;
Memo can be edited while both counters advance. Echo input is routed to its
window, pause/resume preserves counters, #PF leaves the other VM running,
and a faulted VM can load a new program. Existing full UI/media regression
passes after the memory-layout/ABI changes (888 sampled frames without flashes).

The Playwright experimental-image suite passes mode/exception records, physical
PTE/counter checks, independent background execution, window lifecycle,
focused input, #PF/#GP fault/restart, read/close broker use, denied writes
and creation, and byte-identical full FD export after GUI/session shutdown.
Normal web tests: 486 passed, 2 existing skips; production build succeeds.

The architectural exception-vector expansion exposed a missing #PF trampoline
branch during final source review; this is corrected and the full hardware
fault suite passes again on native runtime commit
`40ee404ade6ec3758f2f721fa0b592176d0b5e91`, including DF/device-path regressions
and the existing P98TEST.COM binary. The local image SHA-256 is
`46b3283210b2e40e24b683363a428547fe6eaf33d6295249624e4cc24a54b78c`;
Chromium 151.0.7922.173, Playwright. The native DOS 4 CI also
builds all K386 tools with warnings as errors and uploads their binaries.

CI results are recorded in the draft PRs (MS-DOS #16 and webnp2 #16).
The WebNP2 K386 CI job passes; normal-image/native regression jobs are tracked
separately. General DOS-machine acceptance
is not met: this remains a COM-service runtime, not a fully virtualized DOS OS.

Compatibility boundary under development: per-session private RAM/TVRAM/zero
page; deny port I/O; read-only serialized DOS file services. This is not yet a
complete virtualized MS-DOS kernel/BIOS session. General COMMAND.COM/MZ EXE,
resident drivers, shared writes and graphics hardware virtualization remain
unimplemented. All required acceptance items must be individually reported;
the overall feature is not complete while these execution gaps remain.

## Requested acceptance boundary

| Requirement | Current result |
| --- | --- |
| Traditional KDESK startup and existing GUI | Verified with K386 absent; full UI regression passes |
| Multiple internal applications | Verified; cooperative queues, IDs and lifecycle controls |
| Protected-mode transitions and exceptions | Verified by CPU-written PM/V86/#UD/#PF/#GP records |
| Task memory isolation | Verified by separate CR3/PTE/physical counters and contained #PF |
| Two independent DOS environments | **Incomplete**: two isolated COM runtimes, no per-VM DOS/BIOS kernel |
| Programs in both sessions | Verified for supported COM programs, not general DOS compatibility |
| State across switching and background execution | Verified for both V86 COM contexts |
| DOS windows and existing GUI coexistence | Verified with Memo and native window controls |
| End/restart without desktop failure | Verified for normal exits and contained memory/I/O faults |
| Existing disk integrity | Verified read-only broker and byte-identical complete FD export |
| Web tests and production build | 486 passed / 2 existing skips; build passes |
| Real browser validation | Playwright on the shipped NP2kai wasm; CI results recorded in PR |

Remaining engineering is a DOS/BIOS virtualization layer, not another CPU-mode
switch: virtual interrupt delivery (including PUSHF/POPF/IRET), complete PSP/EXEC
and DOS process state, MZ relocation/loading, BIOS memory/services and device
state. Until those exist, booting COMMAND.COM in each window is unsupported and
the request as a whole is not complete. Shared disk writes and direct GDC/PEGC
applications also remain outside the working prototype.
