# Files / Editor integration

Implementation and verification in progress (MS-DOS #31 stage 4a).

Files keeps its default read-only A: root and can switch to the broker-selected
WORK, including verified HDD and explicit FD fallback. Enumerations and previews
use existing owned directory/file handles. Refresh closes any previous cursor.

A bounded graphical request contains only root/work namespace and an 8.3 TXT
name. The real-mode GUI service queues the request; DOS and scheduling are never
called inside its INT 2F callback. A later broker tick verifies the requestor's
slot/generation/token, finds a free peer slot, and launches only KGEDIT.EXE with
validated namespace/name arguments. Busy slots and unsaved documents are never
replaced. The request result is polled through the authenticated client service.

Editor validates startup arguments and reuses its existing load/revision/save
path. Root documents require Save As; Work documents retain their revision and
save to that Work. Closing/cancelling a client invalidates queued requests.

Acceptance: native policy/ownership tests; actual PEGC/Cirrus Work enumeration
and preview; Files -> Editor -> save -> Files refresh -> reopen; two occupied
slots with dirty peer retained; root/FD compatibility; invalid namespaces/names,
stale owners, missing/large/binary documents and complete cleanup. Coordinate a
pinned native source and matching distribution/E2E PR in webnp2. Recovery UI,
extra execution slots and generic paths are later stages. Keep draft until
acceptance and CI pass; do not merge automatically.
