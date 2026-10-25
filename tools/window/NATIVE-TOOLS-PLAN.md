# Native tools and shell responsiveness

Port FD98, EDIT98 (and EDIT alias), CPANEL, KVIEW and KPAINT from COM to
explicit MZ code/data/stack layouts. Preserve command tails, DOS allocation,
file editing, configuration changes and clipboard/mouse return to KDESK.
Give full-screen desktop tools a distinct application descriptor so they are
never accidentally executed in a restricted text session. Keep the original
IBM-target FD.COM and real-DOS diagnostics where appropriate.

Improve K386 shell responsiveness with bounded scheduling across brokered DOS
requests and text damage rendering. Preserve fairness, CPU limits, timer
delivery, input isolation and the read-only session disk policy.

Validate native builds and tests, regenerated images, actual browser launches,
command tails, save/return paths, two-session shell progress and before/after
latency. WebNP2 pins the completed native source and adds regression gates.
