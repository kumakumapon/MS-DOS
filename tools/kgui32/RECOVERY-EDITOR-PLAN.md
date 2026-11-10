# Work recovery and Editor usability

MS-DOS #31 stage 4b and the requested Editor usability work form one coordinated
native/webnp2 development change. Keep the existing wire sizes, two application
slots, four MiB budgets and real-mode stack bounds. Use draft PRs before coding;
acceptance and CI must pass before review. Do not merge automatically.

Recovery is limited to the broker-selected WORK. Enumerate TMP/BAK candidates,
inspect the TXT/TMP/BAK revisions and preview candidates without changing a live
Editor document. The user explicitly selects a candidate and an unused 8.3 TXT
destination. A revision-checked DOS rename preserves its complete contents under
the new name. Existing TXT destinations, stale revisions, protected files and
invalid paths are rejected before mutation. Other candidates remain untouched.
Never auto-delete backups, truncate files or format media. A failed rename keeps
its source. Recovery does not claim FAT durability through arbitrary power loss.

Extend the bounded Work service with optional operations and typed revision
records; do not expose arbitrary drives, paths, DOS handles, delete or rename.
All disk operations remain at serialized real-mode boundaries with the critical
error handler restored. Enumeration and preview use owned, bounded handles.

Editor gains literal forward search with wrap, selected-match replacement and
atomic Replace All with one undo step. Search/cancel must not alter text. Preserve
selection, undo/redo and dirty state on invalid or oversized replacement. Opt-in
keyboard modifier metadata carries Shift selection and distinguishes Ctrl+H from
Backspace, preserving old clients and packet layouts. LF/CRLF is explicit,
retained on open and tracked in undo/dirty/saved state. Encode before saving,
check the 64 KiB serialized limit and record the raw revision. Increase the text
viewport within the existing 40-command frame by splitting long text runs and
bounding visible rows/selection rectangles; dialogs retain complete controls.

Acceptance: native recovery policy fault tests and real CPL3 negative requests;
Editor search/replace/undo, modifier selection, LF/CRLF round trips and maximum
viewport tests. PEGC/Cirrus E2E must check orphan TMP/BAK inspection, cancel,
explicit recovery, existing-target/stale/protected/error refusal, unchanged
original contents on failures, exported FAT12/FAT16 media, cold boot and complete
resource reclamation. Retain existing suites. Pin a clean native source in
webnp2; build assets in CI. Never run npm run preview in this chat. Browser E2E
runs on GitHub Actions. Implementations and verification are in progress.
