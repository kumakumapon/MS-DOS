# K386 process-memory and interrupt continuation

The merged runtime provides two isolated V86 contexts, KSH, bounded COM/MZ,
one child EXEC level, private cwd/DTA and read-only DOS services. General DOS
compatibility is still incomplete. This continuation investigates the actual
COMMAND.COM startup and expands the guest implementation rather than adding
browser-side processes.

Planned work, subject to actual CPU/driver probes:

- Private process memory with a DOS paragraph allocator, PSP/environment and
  larger MZ allocations. Investigate locked XMS backing so concurrent programs
  and parent snapshots do not exhaust host conventional DOS memory.
- Nested EXEC with restored CPU/memory/DTA, read-only file inheritance and
  bounded failure handling. Retain strict disk and device restrictions.
- Per-session virtual timer/software interrupt delivery respecting virtual IF,
  with CPU exception and guest memory containment preserved.
- Validate real bundled DOS programs and COMMAND.COM; record concrete missing
  services rather than describing an unbooted shell as supported.
- Pin the tested native commit in webnp2, build separate images and extend
  Playwright checks for actual guest memory, contexts, files and background work.

Do not change the historical kernel or replace guest execution with browser UI.
Normal KDESK and saved disks must remain available. Shared writes and direct
PC-98 graphics/ports require separate virtualization and stay denied until
implemented safely. Keep draft PRs while full DOS acceptance remains unmet.
