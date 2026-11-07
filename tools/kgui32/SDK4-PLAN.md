# SDK4: workspace saving

Implementation in progress. Follow SDK3 with an opt-in, bounded real-mode file
broker, Editor Save / Save As and a reusable save picker. Retain SDK1–3 ABI,
two mixed slots / 4 MiB per app, the separate GUI and 16 MiB configuration.

Writes are restricted to regular ASCII 8.3 `.TXT` files in `A:\WORK`, never
boot/root files. A whole-document request (at most 64 KiB) runs serially at the
existing real-mode service boundary, never in IRQ or graphical callbacks.
Validate ownership, package feature, user range, names and expected old content.
Use exclusive temporary creation, checked writes/close, backup/rename with
rollback. Preserve recovery files after an ambiguous failure; DOS renames are
not a power-loss-atomic filesystem transaction. Document recovery explicitly.

Editor keeps its document/history and dirty state on failure or cancellation.
Save As confirms an existing destination and uses a content revision to reject
stale overwrites. Normal close/new/open offer Save, Discard and Cancel.

Acceptance: model/policy/boundary tests; real PEGC/Cirrus browser Save/Save As,
overwrite cancellation/conflict, save/open after restart, disk full/write errors,
fault/End cleanup, independent apps and all existing regression suites. Compare
actual FAT chains and file contents and protect unrelated/boot files. Run npm
test/build, both PR CIs, then merge MS-DOS, wait for main CI, merge WebNP2 and
verify deployment, as explicitly requested by the user.
