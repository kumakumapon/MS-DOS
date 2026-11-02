# K38632 first milestone: protected Ring-3 bring-up

Issue: https://github.com/kumakumapon/MS-DOS/issues/31

Build an independent PC-98 DOS diagnostic, leaving the resident K386 runtime
unchanged. Exercise 32-bit CPL3 with PE/PG set and VM clear, user/supervisor page
permissions, a DPL3 exit gate, real IRQ0 preemption of an infinite user loop,
contained #PF/#GP, a genuine #DF, and safe return to DOS.

PC-98 IRQ0 shares vector 8 with #DF. Preserve PIC vector configuration and
classify the architectural stack frames; do not use V86's VM flag alone.
Record actual CPU frames, CR0/CR3/CR2, user CS/flags/cookies, interrupt counts and
restored host state for browser inspection. Refuse an existing PE owner or
unmasked IRQ0. No firmware or disk I/O occurs while protected.

Verify repeated runs, unchanged disk bytes and continued KDESK/V86 operation
on WebNP2. This milestone is not a production PM32 loader/SDK or GUI app ABI.
