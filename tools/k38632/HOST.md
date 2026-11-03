# PM32 resident bounded-slice backend (in development)

`K32HOST.EXE` is a separate real-mode resident module. INT 2Fh AX=D390h,
BX=0 returns AX=390Dh and ES:DI pointing to K32HOST1 version 1. BX=1 executes
one mailbox request. The mailbox checks a real-mode caller, masked/unowned
IRQ0, InDOS, a busy guard and a slot index before DOS operations. The service
switches to its owning PSP for DOS handles and restores the caller PSP.

There are two private task slots. Each has a trusted conventional-memory
snapshot of `.task`, a distinct aligned three-page directory/table allocation,
and an owned XMS block (4 MiB plus alignment allowance). The shared module
contains code, private entry/exception stacks and temporary transfer buffers;
those stacks are never snapshotted. A20 is acquired/released for each PM entry,
rather than for the entire lifetime of a task. Cleanup releases only the selected
owner; global XMS free/largest values may legitimately change while peers live.
The original standalone K32RUN and its ABI 0100h/0101h remain available.

Requests: create, step, key, stop, resume, end, poll. Step performs at most one
Ring-3 syscall/IRQ quantum and returns to real mode. Poll checks a pending wait
without entering user code. Key input comes from the caller's selected task
queue, never from direct BIOS consumption. Stops retain pages/handles; timeout
remaining time is frozen while stopped. End/exit/fault revoke PTEs, close owned
files and reclaim XMS and conventional snapshots. A failed close/free retains
ownership and refuses reuse until cleanup succeeds.

A private 80x25 console consumes stdout, CR/LF/BS/TAB and bounded CSI H/f/J
sequences. Host view records publish actual CPU-produced frames, PTE samples,
data samples and ownership counters after the API call. K32HOST's MZ tag marks
it as a native real-mode service so V86 loaders reject it before replacement.

Initial bring-up: install K386, then K32HOST, leave KDESK with F10 and run
K32MIX.COM. The diagnostic first alternates a real PM32 SPIN32 with a V86
COUNTER, then alternates two PM32 SPIN32 owners. It checks actual user frames,
progress, different page directories/backing and host restoration before cleanup.
This is the prerequisite for common scheduling and GUI integration; those parts
must not be claimed complete until the corresponding live E2E passes.

## Shared real-mode manager

AX=D391h / BX=0 discovers K32MUX01 version 1 (200 bytes); BX=1 executes the
legacy command/key/path offsets through the new manager. It owns the two-slot
resource policy, budgets and journal, and exposes pointers to the actual V86 or
PM32 producer for each view. AX=D393h returns AX=393Dh / BX=occupied PM32 mask;
the small legacy CREATE guard refuses a slot already owned by PM32. Raw PM32
CREATE similarly refuses a live V86 slot. A manager transaction may prepare a
candidate before ending a V86 owner; failure preserves the previous owner.

The original V86 monitor stays within its 64 KiB image/stack bounds. Its only
extensions are the ownership guard, a no-user-dispatch clock poll, and diagnostic
pointer discovery (AX=D388h). The two execution engines keep their mode-specific
frames and exception handlers. Every slice restores real mode before another
engine or DOS broker runs. No Windows API or hardware I/O emulation is added.

The common manager reuses the existing weighted scheduling/accounting policy.
Only runnable tasks accrue credits. Waiting PM32 tasks receive a bounded poll,
not a BIOS polling loop; GUI input is routed by the active slot. File operations
remain serialized in real mode and all PM32 handles remain read-only. V86
backend events, including child EXEC exit/fault restoration, are transferred to
the common journal with the common task generation.
