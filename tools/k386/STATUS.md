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
focused input, #PF/#GP fault/restart, read/seek/close broker use, denied writes
and creation, and byte-identical full FD export after GUI/session shutdown.
Normal web tests: 486 passed, 2 existing skips; production build succeeds.

CI on final commits is still being checked. General DOS-machine acceptance
is not met: this remains a COM-service runtime, not a fully virtualized DOS OS.

Compatibility boundary under development: per-session private RAM/TVRAM/zero
page; deny port I/O; read-only serialized DOS file services. This is not yet a
complete virtualized MS-DOS kernel/BIOS session. General COMMAND.COM/MZ EXE,
resident drivers, shared writes and graphics hardware virtualization remain
unimplemented. All required acceptance items must be individually reported;
the overall feature is not complete while these execution gaps remain.
