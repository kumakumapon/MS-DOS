# Calendar, window placement and shell touch controls

**Calendar** reads the DOS date when it opens and when you tap Today. It
highlights that day in the viewed month. Previous and Next change the month,
and the titlebar Menu also changes the year. The app supports 1980–2099 and
Gregorian leap days. It does not edit the DOS clock. The read is supplied by
the native driver's existing real-mode clock tick, outside its client gate.

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
of that child. `status` reports the last foreground result. The shell rejects
pipelines and redirection in background commands. Exit and End reclaim
children. Applications can use `k32_stderr_write` to write to their own
console when stdout is piped. KSH32 does not redirect stderr.

Independent process receivers may admit one currently live sender token
through `k32_message_allow_peer`. Passing zero revokes future sends. The same
64-byte message and four-message queue limits apply. A recycled slot cannot
inherit a grant, and sending cannot wait for, cancel or borrow another
owner's process or pipe.

The private V86 DOS console implements INT 21h/AH=0Ch (flush and selected
input). For a blocking read, keys arriving after the flush reach the same
call on resume. The flush is confined to the calling VM's queue.
