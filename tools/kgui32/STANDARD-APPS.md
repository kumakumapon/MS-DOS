# Work management and boot settings

## Files

Open Files → Work → Manage, or use Manage Work in the window menu. The manager
shows DOS 8.3 names, sizes, modification dates and types and sorts by each
column. Use Open and Up to navigate. Use Tab, arrow keys and Enter for keyboard
operation. Copy and Move accept a path relative to Work, for example
`DOCS\COPY.TXT`. New folder and Rename accept a name in the current folder.

The manager uses the verified HDD Work directory when present and A:\WORK
when booting from FD alone. The A: root view is read-only. Root Work TXT and
BMP files can be opened in the corresponding KGUI32 application. Subfolders
support up to three path components below Work. A copy can transfer up to
1 MiB. Cancel a running copy with its Cancel button or Escape.

Delete prompts before moving a file or empty directory to the hidden
`WORK\KTRASH` area on the same volume. From the original parent folder,
choose Trash and Restore. An occupied destination leaves the recycled item
in place. DOS 4 supports renaming a directory within its parent. Folder
moves across parents, recursive copies and recycling nonempty directories
are outside this implementation.

K32IO serializes mutations and checks DOS attributes. Target names are
exclusive. Copies write to an owned temporary file before publication and
keep partial data on write failure. Existing TMP and BAK entries are protected.
The Editor recovery screen handles supported root Work documents. DOS tools
can inspect failed copies in subfolders. FAT media should be exported or
saved by the emulator to carry Work and Trash across browser sessions.

## Settings, Memo and tasks

Choose Settings in System or press F9 while a desktop window is active.
Change XMS and EMS, the DOS FILES count and BUFFERS, or FD98 autostart.
Apply CONFIG and Apply AUTOEXEC write their respective A: files separately.
Both commands support boot files up to 2 KiB, the same limit as CPANEL.
K32IO accepts the selected boot options and verifies the revision previously read
by Settings. It edits the selected directives and retains the other lines.
TMP and BAK guard publication. Reboot the saved medium to apply the settings. KGUI32 requires XMS
and cannot start under an EMM386 V86 owner. Settings asks for confirmation
before saving either change. Use DOS or CPANEL to restore compatible settings
if the changed boot configuration prevents KGUI32 from starting.

Memo (F4) opens Work's NOTE.TXT in Editor and uses the same save, Undo and IME
functions. A new memo is written on Save. An existing KDESK note can be copied
into Work using the DOS filer. System lists a program name and state for each
running slot. Tap a task row to switch to its window.

## Verification

The native build exercises path validation, ownership, no-clobber rules,
revisions and CONFIG/AUTOEXEC transformations. WebNP2 boots the shipped HDD
and complete FD, drives these applications with keyboard and pointer input,
and inspects the exported FAT16 and FAT12 images.
