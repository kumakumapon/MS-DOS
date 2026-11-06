# KDESK rendering snapshot contract

The optional 64-byte KDPERF01 producer exposes ready and a nested busy counter.
A debugger may read composed text/attributes only while ready is one and busy
is zero. The counter must cover transient text writes as well as visible pixel
composition. Full scenes and clock updates already use it; incremental DOS row
updates, partial scene updates and resource updates need the same coverage.

The busy counter now covers each incremental DOS update from its first text
write through restoration of occluding characters/attributes and presentation.
The complete input transaction (partial drawing, nested full composition and
live drag) and resource-pane repaint are guarded too. Nested full/clock drawing
increments the same counter; every normal and exit path balances its decrement.

The producer remains version 1 and 64 bytes. Offset 52, previously reserved,
is now `dos_updates`: a saturating count of incremental updates that actually
painted a row, status or scrollbar. Existing readers can ignore it. New tests
use it to prove that the printing peer's native redraws continue while Memo
occludes it, rather than only observing CPU progress.

WebNP2 reads the idle producer, both text VRAM planes and a protected graphics
rectangle in one synchronous browser evaluation. Busy, unready, replaced and
malformed producers are rejected, as is a changed header at the end of the
read. The emulator runs on that browser thread, so no guest slice executes
between these reads. This is a debugger snapshot contract, not a guest lock
or permission to read native data from an untrusted application.

The E2E establishes expected Memo contents with the printing task paused,
checks clock preservation, moves by odd pixel offsets, resumes the DOS peer,
then compares every accepted snapshot byte for byte. At least eight snapshots,
three actual DOS repaints, two clock preservation updates and continued CPU
progress are required. Hide/restore checks both graphics and logical text.
No draw result is accepted merely because it becomes stable later.

Run `make -C tools/window all test` and WebNP2's
`scripts/measure-k386-render-load.mjs` in changed mode on PEGC and Cirrus.
The original DOS kernel and saved disks are unaffected.
