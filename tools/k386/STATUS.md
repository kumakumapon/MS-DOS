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

In progress, not yet accepted:

- Resident K386 timer-preempted V86 COM sessions. The test driver currently
  detects a supervisor exception; diagnose rather than advertise success.
- Guest DOS session windows, file-service broker and independent execution E2E.

Compatibility boundary under development: per-session private RAM/TVRAM/zero
page; deny port I/O; read-only serialized DOS file services. This is not yet a
complete virtualized MS-DOS kernel/BIOS session. General COMMAND.COM/MZ EXE,
resident drivers, shared writes and graphics hardware virtualization remain
unimplemented. All required acceptance items must be individually reported;
the overall feature is not complete while these execution gaps remain.
