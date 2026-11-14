# Images: bounded Work saves and recovery

Images opens root BMPs read-only and saves BMP24/BI_RGB to the broker-selected
Work directory. Ctrl+W toggles the Load location between Root and Work; the
filename field accepts ASCII 8.3 BMP names. Files Open app supports Work BMPs.
Save (Ctrl+S) writes the displayed filename to Work. An existing destination
requires explicit Replace unless it is the revision opened/saved by this image.
Changing that revision in another instance causes File changed, retaining the
image, baseline and Undo. Changing the filename provides Save As.

Ordinary Load and Close offer Save, Discard and Cancel for a modified image.
Save updates the baseline but retains Undo. Force End remains forceful.
BMP24 output expands RGB565 channels so reopening reproduces every RGB565 bit.
Limits remain 256x192; canonical maximum output is 147,510 bytes.

Ctrl+R inspects BMP/TMP/BAK existence and hashes for the entered BMP base.
Inspect TMP / Inspect BAK reads and validates a complete BMP, including EOF
and the inspected revision, without changing the current image or Undo.
Restore moves the selected candidate to an unused BMP name (RESCUED.BMP by
default), only after explicit confirmation. It neither overwrites the original
nor deletes the other candidate. Errors retain the live image and source.

The 64-byte workspace v1 request gains flag K32_WORK_BMP=2 for STAT, OPEN,
FIRST, SAVE and recovery operations. Flags=0 retains the TXT/64 KiB contract;
CRLF cannot be combined with BMP. BMP names, complete header/dimensions/extent,
readable user ranges and revision are checked before mutation. The bounded BMP
limit is 151,552 bytes, allowing the existing decoder's 4096-byte header offset.
INFO retains its text limit for compatibility. Arbitrary paths, devices,
executables, selected drives, subdirectories and arbitrary writer handles are
not exposed. TXT and BMP with the same base share reserved TMP/BAK names and
therefore block each other while a recovery candidate exists.

The existing exclusive TMP/write/checked-close/backup/publish transaction and
rollback policy is reused. Disk full, short write and rename errors preserve
the previous complete file in BMP or BAK. DOS rename is not power-loss atomic;
export before reset, inspect candidates after interrupted saves, and explicitly
recover. No automatic candidate deletion or filesystem repair is performed.

Native sanitizer tests cover full-size BMP saves and every transaction boundary;
RGB565 round trips cover all 65,536 pixel values. Web E2E inspects actual files,
independent FAT chains, cold boot, conflicts, recovery and rejected writes.
