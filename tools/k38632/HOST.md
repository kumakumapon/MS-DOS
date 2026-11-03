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
