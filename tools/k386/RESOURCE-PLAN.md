# K386 resource control continuation

Base: MS-DOS `10b4a307e5ca35e4f2805d5b626c03ac8c07dbf8` and webnp2
`e24309ad982ffd38a086ec7d094ba9dc789937f5` (merged PRs #18).

This change borrows resource-management ideas from current operating systems:

- Move weighted, work-conserving scheduling into the guest monitor. Offer low,
  normal and high priorities while keeping bounded timer-preempted quanta.
- Account actual PC-98 ARTIC execution time and enforce optional per-session CPU
  budgets. Exhaustion suspends the session without destroying its memory; a
  larger/disabled limit and explicit resume recover it.
- Publish versioned resource information and a bounded lifecycle/fault journal,
  including generation IDs, CPU time, runs, memory, handles and EXEC depth.
- Interrupt the current DOS program and restore its parent when one exists.
  Distinguish this operation from ending the entire session.
- Expose these controls and diagnostics in KDESK's guest task manager. Keep the
  existing mailbox layout and direct-step API for older clients.

Validate the scheduling and quota boundaries with native unit tests, then
exercise real V86 CPU counters, background GUI operation, quota stop/resume,
child interruption, fault records, generations and unchanged disk bytes in
Playwright. Retain all existing DOS/GUI suites, source pins and image provenance.

General DOS/BIOS/TSR, guest XMS/EMS, shared writable disks and external graphics
virtualization remain outside this continuation. Neither the historical DOS
kernel nor NP2kai core needs modification. Implementation starts after draft
PRs exist in both repositories; final verification is recorded in those PRs.
