# Switchable Japanese interface and original vocabulary

F8 switches the desktop and current graphical apps between English and Japanese.
English remains the initial setting. The language is shared for the lifetime of
this desktop; new apps adopt it when their event loop starts. F8 is handled by
the desktop and never becomes a document key. Open documents, selections,
Undo/Redo, pending IME composition and disk contents remain unchanged.

Main desktop commands and state labels, Files, Editor, common picker/recovery,
Images save/recovery and IME controls use original Shift-JIS labels rendered
through the existing PC-98 CG-ROM path. File names, document/preedit/candidate
contents and application data are not translated. Technical identifiers and
labels without a translation retain their English spelling. Per-module tables
contain only their own labels, avoiding a full dictionary in every executable.
Generated Japanese draw strings fit the 47-byte client text bound.

The additive dedicated-GUI syscall 74 (GUI_LANGUAGE) accepts EBX=0 English or
1 Japanese, ECX=0. Other values fail. Ordinary applications cannot invoke that
privileged namespace. New SDK apps opt into input bit 32; capability bit 64
announces language notifications. On POLL, bit 31 of event.control reports
Japanese only to opted-in callers. The SDK strips that bit before returning
input and schedules a synthetic resize/redraw on an empty poll after a change,
without dropping a queued real event. Existing event/request sizes, input
modifiers and non-opted-in callers keep their contract. Old-host subscriptions
fall back without the optional input bit.

The original MIT dictionary grows from 840 readings / 14,017 bytes to 1,024
readings / 18,691 bytes. New vocabulary covers images, files, backups, recovery,
Japanese input, execution slots, memory protection and UI operations. Existing
preferred candidates keep their order; new candidates are appended. Bounds
remain 1,024 entries, 32 KiB total, 96-byte readings and 12 lexical candidates.
No third-party dictionary, network conversion, learning or user-file write is
introduced. Whole-reading conversion, kana/romaji and script selection retain
existing behavior.
