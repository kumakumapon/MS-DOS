# KDESK scheduling under drawing load and XARRAY32

Baseline: MS-DOS f1589e072cf32d92821e4fdd9641a3b884559bf5 and WebNP2
ea764c78bf0e4f1d123d2b002892f1b2c6d11c18. This is a development plan, not a
claim that the following work is already implemented or verified.

## Scope

1. Measure the released image with overlapping Clock, PM32 FILE32/VIEW32 and
   a second CPU loop. Record actual guest generations, modes, CPU ticks/runs,
   dispatches, output, host elapsed time and input acknowledgments.
2. Reduce overlapping-clock and console redraw work. Evaluate bounded drawing
   and safe scheduling opportunities between completed graphics operations.
   Never call DOS from IRQ handlers, reenter a DOS broker, or switch replaceable
   code banks while their code/stack is active. Keep pointer/backbuffer state
   coherent and preserve the existing rendering modes and fallback.
3. Implement XARRAY32.EXE using ordinary PM32 pointers and the existing ABI0101h
   SDK. Match XARRAY's 2 MiB zero/fill/verify, 1 MiB sequential, seeded 8192
   unsigned-16-bit merge sort, boundary search and checked-range workloads.
   Keep explicit 16/32-bit types and compare results, not assumed speedups.
4. Measure both execution/service time and elapsed/input latency at the same
   emulator clock/RAM/video settings. Report dispatch/syscall/transfer counts
   separately. Do not equate guest CPU ticks with browser CPU utilization.

## Acceptance

- Clock remains open and overlapping during the performance gate. FILE32 and
  VIEW32 complete/accept input while the other task demonstrably progresses;
  no closing Clock as a test workaround. Capture baseline and changed results.
- XARRAY32 matches the legacy workloads and deterministic results, runs beside
  PM32 and V86, retains data on stop/resume and releases owned resources after
  normal exit, fault and End. Low-memory refusal preserves an existing owner.
- Validate actual CPU contexts, ownership, RAM, result data and the complete
  exported FAT12 hash in WebNP2/Playwright on PEGC and Cirrus.
- Retain native tests, all existing GUI/V86/PM32/XMS/XARRAY/shell-latency CI,
  npm test and npm run build. Update issue #31 with achieved and pending work.
- Keep two shared slots, 4 MiB per PM32 app, read-only files, existing ABIs,
  original DOS kernel and emulator core. GUI widgets/writes/more slots are later.

Create draft PRs before functional code. Pin the validated native SHA in WebNP2;
keep generated images as CI artifacts and existing saved disks intact. Record
measurements and decisions on the PRs; Ready only after acceptance and CI pass.
Merge order: MS-DOS, successful main CI, then WebNP2. Do not merge automatically.
