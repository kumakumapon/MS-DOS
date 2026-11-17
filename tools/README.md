# Native tools and dependency map

All tools use MIT; retain each directory's LICENSE and the root notice when
redistributing. Historical Microsoft sources are preserved outside these tools.

| Directory | Purpose / important dependencies |
| --- | --- |
| dos4 | Isolated DOS 4 source preparation/build and provenance |
| mz | DOS MZ image builder shared by native tools; Python unit tests |
| filer / editor / control | FD98 / EDIT98 / CPANEL DOS applications |
| window | KDESK desktop and native image/utilities; uses display |
| display | PC-98 PEGC/Cirrus native display diagnostics |
| k386 | Resident V86 monitor; uses kxms32 and kvideo |
| kxms32 / kvideo | XMS allocation / DOS display support |
| k38632 | Protected Ring-3 runtime/SDK; uses k32io and kproc |
| k32io | Privileged DOS I/O broker for the 32-bit SDK |
| kproc | Process lifecycle, IPC and pipe support |
| kgui32 | Independent GUI and graphical 32-bit apps; uses k38632 SDK |

`ports/pc98/dos4.py` composes the DOS boot profile with `control`, `filer`,
`editor`, `window` and runtime applications. `ports/pc98/hdd/` builds the
separate HDD OEM profile; it is not interchangeable with the FD BIOS.
See [documentation](../docs/README.md) and each directory's README/Makefile for
exact targets and current supported ABI. The `*-PLAN.md`, ROADMAP and STATUS
files are archived milestones, not instructions to restart completed work.
