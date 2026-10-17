# KDESK/386 Enhanced

Draft design and implementation record. Target: the PC-98 guest under WebNP2,
with an optional K386 execution manager and the existing real-mode desktop as
fallback. Historical Microsoft sources remain unchanged.

Before implementation, verify the shipped NP2kai CPU's protected-mode,
exception, paging and V86 execution. Two independent, continuing DOS sessions
require isolated CPU/memory/interrupt/device state; a screen switch alone is
not sufficient. Shared FAT12 writes must remain serialized or prohibited.

Acceptance results and limitations will be recorded here as work progresses.
