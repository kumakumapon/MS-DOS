# Execution and memory profiles

The default 16 MiB webnp2 profile retains two mixed V86/PM32 slots plus the
independent protected desktop. The optional 32 MiB profile enables four mixed
slots. The desktop remains a separate fifth context. Existing DOS applications,
CPL3 virtual addresses (00800000h–00BFFFFFh), per-process 4 MiB PM32 limit,
and all v1 record sizes and offsets are unchanged.

INT 2Fh/D392h (BX=0, AX=392Dh, ES:DI) discovers K32MUX02 version 2, 264 bytes.
The first 152 bytes retain command/key/path offsets. It publishes capacity,
profile, and four pointers each to the actual backend view, PM32 record, unified
usage and CPU frame. These are privileged real-mode records, never user APIs.
BX=1 dispatches the bounded command. Command 15 requests capacity 2 or 4 in key;
invalid capacities fail 87, insufficient XMS or conventional RAM fail 8, and
shrinking occupied extra slots fails 5. Expansion requires 24 MiB of total
free XMS and a contiguous 4 MiB block before worker creation; 16/24 MiB profiles retain two slots. Allocation
failure rolls back all control allocations. The GUI attempts expansion before
creating workers, exposes feature bit 16, and releases its expansion after
ending only the worker generations it owns. Pre-existing expanded managers and
foreign workers remain owned by their caller.

D391h/K32MUX01 (200 bytes), D390h/K32HOST1 (7576 bytes), D386h/K386ABI1
(4240 bytes), D387h/K386SYS1 (1084 bytes) remain valid for the original two
slots. D391 cannot change capacity or address slots 2/3. D38Bh/K386EXT1 is the
additive V86 backend bank descriptor (64 bytes); it provides four stable view,
usage and frame pointers. Its bounded operations use the legacy command box.
The unified manager schedules all active slots with smooth weighted fairness.

Extra control state lives in separately owned conventional RAM. Only one pair
is loaded into the resident near area during a synchronous operation, and busy
publication blocks observation/re-entry until the original pair is restored.
Each active task still owns its XMS block, private page tables and handles.
Page-permission metadata uses one byte per page; hardware PTEs remain 32 bit
and reconstruct physical offsets from the fixed private mapping. Physical XMS
allocations may extend to 32 MiB; supervisor mappings and user bounds do not
expand. Both resident linker stack guards remain enforced. Real-mode C uses
4-byte stack alignment; the protected application ABI is unchanged.

GUI_SESSION, GUI_SNAPSHOT and graphical client envelopes accept 0–capacity-1.
Generation/token checks apply to every slot. The 48-byte GUI_EVENT is retained;
a 64-byte form appends two extra states and counters. Graphical client queues,
tokens and frame buffers occupy separate private far pages, including slots
2/3. The clipboard remains shared only through the explicit clipboard API.
Closing a worker, faulting, exiting the GUI or failing midway through allocation
must reclaim handles, heap, XMS, page tables and owned conventional control RAM.

The expanded desktop provides four New DOS buttons, four taskbar entries,
four System rows and independent launch/focus/pause/resume/restart/close controls.
The default two-slot layout retains its existing coordinates.

The bundled FDXMS286 PC-98 binary remains unchanged and manages the low pool
below 16 MiB. KXMS32.SYS is an original optional supplement loaded after it.
Only BIOS configurations advertising >=16 MiB above 16 MiB activate its separate
16–32 MiB pool. The PC-98 reserved gap is never allocated. Low handles, HMA and
A20 delegate to FDXMS286; high handles have independent bounded accounting.
High moves validate both ranges, lock any low handle, pair the old driver's A20
reference, and use 386 protected copies of at most 32 KiB with exact restoration
of CR0/GDTR/segments/IF. No DOS/BIOS is called in protected mode. XMS 2 total and
largest queries combine the two pools without claiming they are contiguous.
