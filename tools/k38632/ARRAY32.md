# XARRAY32 and rendering load

`make -C tools/k386 all test` also builds `tools/k38632/build/XARRAY32.EXE`.
The enhanced image installs it and the native **PM32 array lab** command.
It needs K32HOST, ABI 0101h features 7 (startup, heap, input), and the existing
4 MiB reservation. It does not request file access. Use 16 MiB emulator RAM
for two apps. No kernel, emulator, or app ABI change is needed; normal KDESK
without the enhanced monitor remains supported.

## Use

Keys 1–5 run individual workloads; A runs all; Q exits. Optional argument `1`
or `2` selects an independent data identity. H allocates and verifies a 2 MiB
pattern, then retains it while waiting. C verifies every retained byte again
and frees it; Q verifies/frees and exits; F deliberately faults while owning
the allocation. End releases the owner without app cooperation. Other owners
retain their resources. This is a diagnostic app, not a new memory service.

Datasets match V86 `XARRAY.EXE`: 2 MiB zero/write/verify; 256 4 KiB pages
with two sequential reads each; 8192 seeded unsigned 16-bit values sorted by
bottom-up merge sort; an eight-byte match crossing a page boundary at 12285;
and six checked requests on a 10000-byte array. Identity 1 uses the legacy
seed 12345. Values explicitly wrap at 32 bits. PM32 fill and retained-data
verification use aligned 32-bit accesses to the **same byte pattern**; V86
uses its existing cached byte view. Work counts are logical bytes or element
accesses, not equal instruction or bus counts. Range rejection is an app
byte-view check, not hardware protection at an unaligned object's end inside
an allocated page.

Both demos print actual `bytes`, `sum`, `detail`, `work`. PM32 also publishes
64 bytes at the first writable ELF address: 16 little-endian dwords: magic
58413332h, version 1, sequence, scenario, state, error, bytes, checksum, detail,
work, allocations, frees, first word, last word, reserved, identity. Scenarios
1–5 correspond to the menu, 6 holds, 7 faults. States: idle 0, running 1,
done 2, failed 3, held 4. Inspect actual CPU samples after the task waits;
a title or stale snapshot is insufficient.

## GUI boundaries

Overlapping Clock ticks previously requested complete scene composition.
Packed PEGC/Cirrus now draw the clock face in the composition page, restore
front-window intersections from the visible page, then present only the face.
The three text/attribute mirror rows are preserved under occluders as well.
Overlapping console changes similarly preserve changed rows, including the
status row. A 320-byte scratch avoids a second text framebuffer. Cursor
hide/show brackets updates. The repaired composition page stays valid for
subsequent move, hide, restore and drag operations.

Modal panels, viewport scrollbar changes under occlusion, active drag and
16-color occlusion retain the composition fallback. Scheduling stays at
completed event-loop boundaries. No IRQ calls DOS, switches a code bank, or
reenters the broker. The GUI code-bank limit remains checked by the linker.

`KDPERF01` in `tools/window/performance.h` is optional debugger metadata, not
an app ABI. Read ready=1/busy=0. ARTIC runs at 307200 Hz with 24-bit wrap;
individual measured draws/gaps must be shorter than one wrap. Counters
distinguish full-scene composition from preserved-clock updates. These are
guest measurements, not browser CPU utilization.

## Verification and limits

WebNP2's `measure-k386-render-load.mjs` compares a frozen released image and
the candidate at RAM=16, clk=8, worklet=0. It leaves Clock open, checks FILE32,
VIEW32 input and peer CPU progress, and compares visible pixels and TVRAM
characters/attributes, including an odd-pixel occluder and a printing console.
`verify-k38632-array.mjs` compares both demos to an independent oracle, inspects
real CPU contexts/page mappings, retained patterns, stop/resume, End, fault,
normal exit, low-memory refusal and whole-disk hashes. Both run on PEGC and
Cirrus. Timing includes allocation, verification, console output and services.
See the paired WebNP2 PR for measurements and CI artifacts; one workload
does not establish a universal speedup.

Two shared PM32/V86 slots, 4 MiB per PM32 owner, existing APIs and read-only
FAT access remain the limits. GUI widgets, more slots, writable shared storage
and a general-purpose DOS/graphics virtual machine are separate work.
