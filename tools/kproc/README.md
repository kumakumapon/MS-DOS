# Protected processes, IPC and streams

`K32PROC.EXE` adds bounded process services to the existing mixed manager.
It is a 24 KiB DOS resident with a checked 8 KiB callback stack. Load K386,
K32IO, K32HOST, then K32PROC before starting KGUILDR. The service also works
without KGUILDR: `KPRUN.COM` exercises a real protected shell/child and, in the
32 MiB profile, a two-child pipeline using a real-mode scheduling client.

The 4 MiB application space, 16/32 MiB profiles, maximum 2/4 mixed execution
slots, old application ABIs and resident linker/stack guards are unchanged.
Each child still has its own CR3, backing, guarded stack and file owner.

## Application contract

Include `tools/k38632/sdk/process.h`. New process-aware applications use ABI
258, feature 512 plus common I/O feature 256, and `package.py --process`.
`KSH32.EXE`, `KTOOL32.EXE` and `PROCTEST.EXE` declare features 783. The standalone
single-application K32RUN deliberately rejects that capability; use a manager
client. Old ABI 256/257 applications can still be started without inherited
streams. Inheriting streams requires feature 512, otherwise startup fails with
38 and the proposed endpoints are released before any child instructions run.
This prevents an old raw-console writer from silently producing an empty
redirected file. Existing GUI and standalone applications keep their old I/O.

System call 18 retains the 64-byte K32Io envelope. Operations 32 through 41 are
INFO, SPAWN, WAIT, CANCEL, SEND, RECEIVE, PIPE, READ, WRITE and CLOSE. Identity
comes from the trusted host's actual physical allocation and epoch, checked
against the manager's slot generation. User requests cannot supply an owner,
physical pointer, host handle or manager command. All payload pages are checked
in the caller's current map before transfer. The module accepts only the
trusted K32HOST sender and checks its real-mode stack canary on every call.

- Process and endpoint handles are opaque positive tokens from a monotonic
  counter. Exhaustion returns 75 instead of wrapping into an old authority.
- There are eight process records: four reserved root identities and four child
  records including queued children and zombies. Filling the child pool cannot
  prevent a newly launched root from acquiring its own identity.
  Physical execution slots remain two or four. A queued launch owns endpoint
  copies but consumes no XMS. No slot or record availability leaves the caller
  running; a failed launch is collected through WAIT with START_FAILED.
- SPAWN accepts one uppercase ASCII 8.3 `.EXE` name plus arguments, at most 126
  command bytes plus NUL. Paths, devices and unbounded argument strings are
  rejected. The existing packaged-app loader performs full validation. Native
  unprotected DOS executables are not executed by this service.
- WAIT is nonblocking (`-11` while pending). A completed wait returns the exit
  code and reason and reaps the child. Reasons are exit, fault, cancellation,
  start failure, or external replacement. A replaced slot returns code 116,
  never the new occupant's status. Only the parent can wait or cancel a child.
- Parent exit, fault or forced End cancels descendants and closes their stream
  copies. Cancellation rechecks generation/physical/epoch before stopping a
  slot. Failed resource closure retains ownership for retry. A zombie keeps
  only its bounded result until its parent waits or exits.
- Parent/child messages are FIFO, at most four queued messages of 1–64 bytes per
  process. An unrelated process cannot send by guessing a token. A full queue
  returns `-11`; a short destination does not consume the message.
- Four pipes each hold 512 bytes, with at most eight owned endpoints per process.
  Reads/writes transfer at most 256 bytes and can be partial. Full/empty live
  streams return `-11`; empty streams with no writers return EOF (zero), and
  writes with no readers return EPIPE (`-32`). Endpoint direction, ownership,
  stale tokens and inherited copies are checked separately.

The SDK offers cooperative wait loops and process-aware getchar/putchar/printf.
EOF never falls through to keyboard input. Legacy direct syscall-1 console
writes are unchanged; stream-aware programs use the process SDK stdio helpers.

## Scheduler integration

INT 2F D3A0 returns AX=3A0D. BX=0 discovers the versioned `K32PROC1` record;
BX=1 handles a trusted request and performs no DOS launch or scheduling;
BX=2 pumps deferred work outside K32HOST and DOS callbacks. A pump performs at
most one launch. Real-mode manager clients must pump after scheduling and before
reusing slots. KGUILDR does this at its regular service boundary and before
explicit Create/Restart; KPRUN demonstrates a headless client. The manager
command prefix is saved/restored across every launch/stop. InDOS, real-mode,
reentry and canary checks refuse unsafe entry. Browser diagnostics read the
published state; no application receives its address.

## KSH32

Choose **F2 → 32-bit shell** to start `KSH32.EXE` in a GUI terminal, or use `/C "command"` for one batch command.
`help` and `exit` are built in. For example:

```text
KTOOL32.EXE echo hello | KTOOL32.EXE upper >OUT.TXT
KTOOL32.EXE cat <OUT.TXT >COPY.TXT
KTOOL32.EXE cat
```

The shell supports two external commands, one pipe, quoted arguments, and one
Work input/output redirection. Names remain limited 8.3 names under Work; no
directory, wildcard, device, `&` or command-list interpretation is added.
Its maximum line is 126 bytes and the existing startup argument limits apply.
KTOOL32 provides echo, binary cat/upper, exit, IPC and diagnostic wait/fault modes.
Foreground stdout returns to the shell's console. Console/inherited stdin is
relayed through a bounded queue; Ctrl+D supplies EOF. Ctrl+C cancels and reaps
foreground children and returns to the prompt. End closes the shell and its
descendants. Stream backpressure yields to the other tasks.

Output redirection collects at most 1 MiB. The shell captures the original
revision before launch and saves through the common TMP/BAK protocol only after
all children exit normally with code zero and stdout reaches EOF. A failed
child, cancellation, overflow, stale revision, full disk or save failure keeps
the original. Failed common saves retain their recoverable temporary file.
Input is a bounded ordinary read, not an immutable file snapshot. DOS rename
does not promise power-loss atomicity. The shell reports a status for each
external command and exposes readonly CPU-produced diagnostic counters.

## Validation

`make -C tools/kproc test` runs the production policy under ASan/UBSan, including
token exhaustion, private messages, partial/wrapped/full streams, EOF/EPIPE,
orphan cancellation, generation reuse and parser failures. K38632 tests validate
the actual packaged applications, capability refusal and resident stack bounds.
PROCTEST executes real CPL3 pointer/permission/size refusals, old-ABI child
startup, exit/wait, IPC, 70001 binary bytes, fault/cancel and capacity refusal.
KVEXT covers DOS display/input. Browser/Canvas/persistence tests run only in
GitHub Actions; do not start local preview or a local browser server.

Further work includes more than two pipeline commands, background job control,
explicit consent-based IPC between unrelated roots, broader stream-aware app
ports and larger application address spaces. These are separate extensions.
