# K386 resource control

K386 borrows weighted scheduling, resource budgets, generation IDs and a bounded
event journal from modern operating systems. These are guest monitor services,
not browser timers or simulated HTML processes. The policy unit is a DOS session,
including its synchronous EXEC descendants.

## Use

Boot the separate KDESK/386 image with 16MiB extended memory. Ctrl+N / Ctrl+O
open DOS windows. Ctrl+T opens Tasks; select DOS 1 or DOS 2 with arrows or mouse.
The expanded panel shows root-launch ID, active program, priority, execution
time, runs, XMS reservation, allocated DOS heap, virtual handles, process depth,
last stop/termination reason and recent events.

- P / Prio cycles Normal -> High -> Low -> Normal (weights 2, 4 and 1).
- L / Limit cycles off -> 1000 -> 5000 -> 10000 -> off, in execution ms.
  Limits apply to total time already used since the root launch, not remaining
  wall time. A limit below consumption stops the session immediately.
- S / Stop, R / Start and K / End retain their existing behavior. Raise/disable
  an exhausted budget before Start can resume the session.
- B / Break, or Ctrl+C inside a DOS window, interrupts the current program.
  An EXEC parent resumes with AH=4Dh status 0103h. Interrupting the root ends
  that program. End terminates all levels and releases the XMS allocation.
- [ / ] browse up to 16 lifecycle/policy/fault records, including session/root
  ID, active PSP, CS:IP, exception and CR2. This in-memory journal writes no disk.

KSH remains the default. Interrupting its child returns to the shell. This is
monitor-enforced interruption, not POSIX signals or DOS INT 23h handler delivery.
Existing GUI apps still work. Normal KDESK retains its original layout when the
resource extension is absent.

## Scheduling, accounting and limits

The monitor selects a runnable VM each poll with smooth weighted round robin.
Blocked tasks accumulate no credits; an eligible VM uses idle capacity. Low
priority still makes progress. Both private BIOS clocks are serviced before
selection so waiting timer recipients can wake. Priority changes reset credits,
not CPU context or fixed PIT quanta. Direct STEP remains for older clients.

Accounting samples the actual 24-bit PC-98 ARTIC counter (307200 ticks/s) around
protected execution, snapshots, A20 return and its serialized DOS broker. It
excludes GUI idle time, waiting, initial loading and parent cleanup. This is
emulated execution/service time, not host-browser CPU utilization. Counters
saturate. A budget can overshoot by one bounded quantum plus its broker call;
it is not a hard real-time wall deadline. PIT preemption still contains CLI loops.

Exhaustion suspends all EXEC levels and retains registers, RAM, handles and
screen. The other VM and GUI continue. Raise/disable the limit and explicitly
resume; changing the budget never resumes automatically. Root replacement
assigns a fresh globally distinct ID and zeroes usage while preserving that
slot's priority and limit. Nested EXEC shares its session budget and counters.
Heap is trusted arena allocation; XMS is the actual 664KiB reservation including
alignment. End releases it; an ended/faulted root may retain its snapshot and
reservation until End/replacement. Time used before a pause remains charged.

## API

The original INT 2Fh AX=D386h mailbox (K386ABI1 v1, 4240 bytes) and 2044-byte
views remain unchanged. INT 2Fh AX=D387h, BX=0 discovers the resource extension:
AX=387Dh, ES:DI -> K386SYS1 v1, size 1084. Older monitors chain this distinct
function without executing a stale mailbox command. The GUI checks version/size
and falls back to alternating STEP when the extension is absent.

New mailbox commands are SCHEDULE=7, PRIORITY=8, BUDGET=9, INTERRUPT=10,
DESCRIBE=11. Session must be 0/1. SCHEDULE returns the chosen slot in session
(FFFFh when none). PRIORITY key is 0/1/2 or FFFFh to cycle. BUDGET key is 0..60000
ms, zero unlimited, or FFFFh for the UI cycle. Invalid arguments return 87.
Resume of an exhausted session returns 5. DESCRIBE key chooses journal age
(zero newest), producing six bounded ASCII rows for small clients.

The 20-byte extension header precedes two 60-byte K386Usage entries, sixteen
32-byte K386Event slots and six 72-byte report rows. sequence/next/count are at
12/16/18. Exact types are in resources.h. Read only while mailbox word 22 is
zero, like the CPU diagnostics. Typed records remain authoritative. V86 guests
cannot invoke this monitor resource API through their virtual INT 2Fh.

Diagnostic formatting lives in a separate resident module. The GUI uses GCC
12+ -Oz and LTO through the GCC linker driver. The existing d000h end-of-BSS /
12KiB minimum COM stack assertion is retained.

## Validation and remaining scope

make -C tools/k386 all test covers fairness/no starvation, blocked credits,
quota boundaries, saturation, IDs and journal wrap. KRESCHK.COM executes inside
actual DOS to check weighted V86 counters, ARTIC budgets, retained RAM, child
interruption, isolated page faults and cleanup. INTWAIT.COM checks AH=4Dh after
its COUNTER.COM child is interrupted. WebNP2's resource Playwright suite checks
GUI/mouse controls, real CPU/resource records and whole-disk integrity. Existing
GUI, shell, process and timer suites remain required.

General DOS/BIOS/TSR, guest XMS/EMS APIs, shared writes, x87 and external graphics
virtualization remain incomplete. Historical DOS and NP2kai sources are unchanged.
