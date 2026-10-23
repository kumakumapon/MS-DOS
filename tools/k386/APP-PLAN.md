# KDESK/386 executable applications

Convert the bundled V86 applications from flat COM programs to explicitly linked
MZ executables with an entry point, allocation/stack bounds and relocation records.
Provide a documented KDESK/386 application startup and contract; launch supported
applications in guest DOS windows and use KSH.EXE as the default session shell.
Retain bounded DOS COM compatibility as a regression fixture, not the default
application format. Native GUI internal apps remain event-driven KDESK tasks.

Validation: native image/loader tests; actual WebNP2 browser checks of MZ bytes,
relocated CPU segments, independent sessions, child EXEC/return, faults and disk
integrity; existing GUI/shell/process/resource suites, npm test and build.
The paired WebNP2 PR pins the tested MS-DOS commit and rebuilds both images.
General DOS/BIOS and external graphical application compatibility remain bounded.
