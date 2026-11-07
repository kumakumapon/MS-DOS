# KGUI32 HDD workspace implementation

Connect the SDK4 workspace broker, Editor and common Open/Save picker to the
recognized PC-98 data HDD. Retain FD boot and an explicit FD fallback when no
supported HDD is present. Select the drive in the privileged guest broker, never
from an untrusted application path. Provide versioned workspace information,
current destination and real DOS free-space information to applications.

Preserve 8.3 TXT names, the 64 KiB document limit, ownership/buffer validation,
conflict refusal and TMP/BAK recovery semantics. Do not format existing images.
Integrate the verified KHD98 driver with the existing GUI boot configuration.

Acceptance: host contract/error tests plus real WebNP2 PEGC/Cirrus Editor and
picker operations, 64 KiB create and repeated overwrite, export/cold-boot read,
full disk/I/O errors, existing-file/FD protection, no-HDD fallback and resource
reclamation. Coordinate pinned native commits, images and browser CI with the
WebNP2 PR. Keep drafts until the acceptance checks pass. App migration, Files
integration, recovery UI and HDD-only boot remain later stages of issue #31.
