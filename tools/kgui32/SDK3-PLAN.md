# SDK3 / protected graphical editor

Status: design checkpoint; implementation and acceptance pending.

Scope: bounded multiline ASCII editing, selection, guest clipboard, undo/redo,
shared read-only A: root file picker, cancellable close requests and opt-in key
routing. Preserve SDK1/2 ABI behavior and emergency task termination. Normal X
and desktop exit must allow dirty-document cancellation. No disk writes.

Keep two mixed slots / 4 MiB per app and dedicated GUI 4 MiB; use 16 MiB RAM.
Clipboard copies require ownership/generation checks; no foreign user pointers.
Native resident stack/layout limits remain mandatory. Original kernel unchanged.

Acceptance: meaningful model/boundary tests, actual PEGC/Cirrus browser editing,
clipboard between two editors, history/selection, picker content against FAT,
close/exit cancellation, reserved keys, stop/resume/fault/restart/reclamation,
unchanged whole disk, legacy regression, npm test/build, both repository CI.
Keep PRs draft until acceptance is complete. Do not merge automatically.
