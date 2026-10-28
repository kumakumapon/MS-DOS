# K386 application XMS implementation plan

Provide a separate, versioned INT 2Fh application service for managed XMS data
blocks. Keep the 16-bit executable ABI and use explicit bounded copies between
private V86 conventional RAM and monitor-owned XMS. No host handles, physical
addresses, XMS driver entry points or writable mappings are exposed.

Implement per-process ownership, session quotas, stale-handle rejection,
initialization before disclosure, complete range checks, and automatic cleanup
on exit, fault, interruption, root replacement and session termination. Preserve
parent allocations during nested EXEC. Calls are brokered in real mode and
bounded so another DOS session and the GUI continue to run.

Add SDK wrappers, a real EXE diagnostic exercising data beyond 64 KiB, policy
unit tests and WebNP2 browser E2E checks against the running CPU and image.
Document use, limits, and the distinction from a 32-bit executable runtime.

WebNP2 will pin the final tested MS-DOS source commit. Keep both PRs draft until
required tests and premerge CI pass; report postmerge CI separately. Do not merge.
