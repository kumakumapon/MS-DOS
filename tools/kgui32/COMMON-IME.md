# Common input, phrases and explicit user dictionary persistence

`sdk/ime.h` now exports `ki_key_event` and `ki_commit_text` in addition to the
original primitives. Editor and KGWORDS are independent CPL3 consumers. A
proposed commit does not clear composition: only the destination's successful
insert permits `ki_clear`. Failed insertion, focus changes and F8 UI-language
changes retain reading, pending romaji and candidate selection. Each process
owns its own input state. No global browser IME or DOS-resident FEP is used.

Exact whole-reading candidates retain their existing order. User candidates
precede matching system candidates. Otherwise bounded dynamic programming
combines complete dictionary words of at least two kana and explicit particles;
unmatched fragments retain kana fallback. Candidate count remains 16 and text
96 Shift-JIS bytes. This is bounded phrase conversion, not a general Japanese
morphological analyzer. Original explicitly classified lexemes generate godan,
ichidan, irregular and adjective forms in ime-build.py; no -ru guessing or
third-party dictionary data is introduced. System format remains KGIMED1.

Welcome > User dictionary opens KGWORDS.EXE. F4 toggles romaji, F5 selects JIS
kana, F6/F7 select script, Space converts, Enter proposes commit, Esc edits or
cancels. Tab moves between Reading and Word; paste uses the internal clipboard.
Add/Delete modify only the private dictionary. Save/Ctrl-S explicitly writes
USERIME.DIC in Work using the common binary broker. Unsaved entries/forms and
active composition guard close/reload. Ctrl-U clears the selected field.

User dictionaries allow 256 unique readings, 12 candidates per reading and at
most 32768 bytes. Header, count, hash, Shift-JIS, bounds and duplicate readings
are checked before adoption. Invalid loads retain the complete active dictionary.
Save uses a captured file revision; conflicting saves retain edits and TMP.
Inspect TMP/BAK validates a candidate. Restore explicitly moves the inspected
candidate to unused RECIME.DIC, leaving the original and active dictionary intact.
It never implicitly loads a recovery candidate or overwrites a destination.

Editor loads USERIME.DIC on first conversion; F9 or input-settings Reload words
reloads it explicitly without changing document/Undo. Active composition refuses
reload. Missing user files use an empty valid dictionary; other failures keep
the previous dictionary. Preview/prediction, learning and arbitrary external
dictionaries remain later units. UI labels and shared composition hints are
translated without translating document, fields or candidate bytes.

Native ASan/UBSan tests cover common commits, phrases/inflections, user priority,
malformed/duplicate dictionary retention and entry limits. Actions covers real
Editor/KGWORDS input, independent owners, persistence/conflicts/recovery, actual
CG-ROM rendering, HDD/FD and cold boot of exported media.
