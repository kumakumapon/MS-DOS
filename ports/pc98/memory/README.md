# PC-98 DOS 4.0 extended-memory drivers

Tracking issue: kumakumapon/MS-DOS#7.

The PC-98 port will expose configured extended RAM through an XMS-compatible
HIMEM driver and evaluate the DOS 4.0 386 expanded-memory manager (EMM386).
The IBM AT assumptions in the original EMM386 OEM layer must be replaced with
PC-98-safe memory discovery, A20, and mapping behavior. Do not enable an EMS
page frame or UMB range unless it is backed by memory the NP2 PC-98 model can
actually map.

Initial investigation:

- The DOS 4.0 source tree contains `v4.0/src/MEMM/MEMM` (386 manager) and
  `v4.0/src/MEMM/EMM` (EMS manager), but its build disables high-memory support
  with `NOHIMEM` and there is no standalone XMS/HIMEM driver.
- The PC-98 SYSINIT build currently reports no IBM INT 15h extended-memory
  service; its image builder also ships a minimal CONFIG.SYS.
- WebNP2 accepts a configurable extended-memory size. The driver must use a
  guest-visible PC-98 memory interface and be tested at multiple sizes.

Acceptance is tracked in issue #7. This note is an implementation checkpoint;
claims of XMS/EMS/UMB support require guest-level API and DOS memory tests.
