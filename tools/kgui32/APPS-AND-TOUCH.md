# Calendar, window placement and shell touch controls

**Calendar** reads the DOS date when it opens and when you tap Today. It
highlights that day in the viewed month. Previous and Next change the month,
and the titlebar Menu also changes the year. The app supports 1980–2099 and
Gregorian leap days. The date comes from the native driver's real-mode clock
tick outside its client gate.

The `+` title control maximizes a session and `=` restores it. The placement
control to its left cycles through left, right and original geometry. On a
640-pixel PEGC desktop the minimum client widths require some overlap. The
original position and dimensions are saved per slot and discarded on reuse.
Calendar and Clock menus show disabled and checked commands where applicable.

The terminal's Run prompt offers Enter and Esc buttons for touch users. The
32-bit shell offers Cancel and EOF buttons that send the same input as Ctrl+C
and Ctrl+D. Use the normal on-screen keyboard for the text of a command.

KSH32 accepts three stages with two pipes when four execution slots are
available. For example:

```text
KTOOL32.EXE cat <IN.TXT | KTOOL32.EXE upper | KTOOL32.EXE wc >COUNT.TXT
```

`wc` reports LF-delimited lines, ASCII-whitespace-delimited words and bytes.
Redirection uses the same revision check and transaction as other Work
writes. All stages must exit successfully before it publishes a destination.
A failed stage or insufficient slots retains the original.

`bg COMMAND` starts one background child with its own private console.
`jobs` reports its token or its last exit status. `cancel` requests cancellation
of that child. `status` reports the last foreground result. Background jobs
accept a single command with private console I/O. Exit and End reclaim
children. `k32_stderr_write` displays application diagnostics in the
child's private console while stdout follows the pipeline or redirection.

Independent process receivers may admit one currently live sender token
through `k32_message_allow_peer`. Passing zero revokes future sends. The same
64-byte message and four-message queue limits apply. The grant identifies the
current process token. Parent ownership controls waiting, cancellation and
pipe inheritance.

The private V86 DOS console implements INT 21h/AH=0Ch (flush and selected
input). For a blocking read, keys arriving after the flush reach the same
call on resume. The flush is confined to the calling VM's queue.
