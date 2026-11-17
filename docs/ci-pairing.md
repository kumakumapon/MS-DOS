# Paired MS-DOS / webnp2 CI references

Status: maintained. Native workflows currently use webnp2 master-line commit
`dde99e952664e0a8b64d716ec98ac6ea75961eb6`. CI checks its ancestry against master.
Never pin a development-only branch: it may disappear after branch deletion/GC.

1. Choose an immutable webnp2 commit reachable from `origin/master` and set the
   same full SHA in both `pc98.yml` and `pc98-dos4.yml`. Fetch master history and
   run `git merge-base --is-ancestor <sha> origin/master` in the webnp2 checkout.
2. Run the native PR's DOS 2/4 build/boot checks. Merge it, then wait for main CI.
3. Set webnp2's `scripts/prepare-msdos4-cpanel-ci.sh` `source_commit` to that
   merged MS-DOS main commit. Its CI builds media from clean sources and records
   the exact native commit and hashes in the generated manifests.
4. Merge webnp2 only after all browser checks pass; wait for master CI and Pages.
   Record both merged commits and the native source SHA in the PR acceptance.
5. When changing the emulator/bridge itself, advance the native workflows to the
   new master-line webnp2 SHA in a follow-up PR and repeat native boot checks.

This staged update avoids circular commit dependencies. Native CI validates its
explicit emulator pin; webnp2 E2E validates the new distribution against the new
native pin. They are separate, recorded checks rather than an implicit latest.
Browser/emulator servers run only inside bounded Actions jobs, never in chat.

DOS 4 CI also runs the MZ builder unit suite and uploads native SDK, GUI, video,
process and HDD OEM build artifacts. PR concurrency cancels superseded work;
main runs and their diagnostics are retained. Artifact replacement permits reruns.
