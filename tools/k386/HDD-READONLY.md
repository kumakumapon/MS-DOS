# V86 verified HDD reads

Implementation/acceptance in progress; this file records the development contract.

Extend the private V86 DOS broker from A: to A: plus the KHD98 volume identified
through the actual DOS DPB/driver record. Keep all guest disk writes, create,
rename/delete and writable opens denied. A foreign label/file cannot authorize a
volume. Preserve wire layouts, two application slots and the monitor's existing
0xc000 stack boundary.

Each VM owns its default drive and A:/HDD current directories. Support select/query
drive, drive-relative/absolute paths, CD/get-CWD, read-only open/read/seek/duplicate,
attributes, owned directory search and nested EXEC. Never change host DOS's CWD
or default drive. Root application lookup uses FD precedence then verified APPS
only on missing bare FD names; a new VM starts in its root program's directory.
Nested EXEC inherits the VM's current drive/directories rather than changing them.

Use bounded internal staging without changing the public 4096-byte XMS transfer
contract. Fail disk critical errors temporarily and restore INT24 on all outcomes.
Validate paths/devices, retain existing VM on failed replacement, and reclaim
owned handles, searches, memory and vectors on normal/fault/forced termination.

Native tests cover path/drive isolation, malformed inputs, internal staging and
memory bounds. Paired webnp2 CI covers actual V86 HDD/root and nested launches,
large reads, directory searches, write denial, FD/no/foreign HDD cases, mixed peers,
media byte invariance and complete cleanup on PEGC/Cirrus. Browser E2E runs only
in GitHub Actions; do not start npm run preview in this chat.
