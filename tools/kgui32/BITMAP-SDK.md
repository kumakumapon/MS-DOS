# Bitmap SDK and Images

The existing client ABI stays version 1 / 3144 bytes, with <=40 draw commands.
Capability bit 32 adds one owned RGB565 surface per client, up to 256x192 pixels.
The independent desktop stores two buffers per slot in its existing private
4 MiB mapping. Four slots consume 786432 bytes for front/back image buffers;
no extra per-image DOS or XMS allocation is needed. Apps keep their own source
pixels in their existing private 4 MiB process budgets.

`kg_surface_create(w,h)` returns a positive opaque handle. `kg_surface_upload`
accepts exactly w*h*2 private bytes. It sends <=3040-byte even chunks in order,
waiting for a bounded acknowledgement after each. `kg_bitmap(handle,x,y,sx,sy,w,h)`
adds a source crop to the frame; negative destinations clip at the client canvas
and compositor damage/occlusion boundaries. Source rectangles must fit the owned
surface. `kg_surface_destroy` invalidates it after acknowledgement. Transfer
calls precede `kg_begin` because they use the SDK's request scratch.

Raw operations 12/13/14/15 are create/upload/status/destroy. Upload uses width as
handle, height as byte offset and count as bytes in draw[]. Zero/odd/oversized
chunks, gaps, overflow, another handle, another token or generation are refused.
Offsets restart at zero only after a complete previous upload. FRAME with an
incomplete or pending surface is refused. A previous complete frame and front
buffer remain visible during a new transfer. The front swaps only after the
complete replacement has arrived. Creation zeroes both buffers; Close, fault,
End and generation replacement clear ownership. No application can address
another client, native RAM, VRAM or an arbitrary desktop pointer.

The native-to-desktop transport reuses the client's existing private far frame
page. It includes a validated 16-byte header in title (handle, offset, dimensions)
and a native revision. GUI_SURFACE_ACK (73) accepts only the dedicated desktop's
24-byte request, matching slot, owned generation, token and pending revision.
The old frame remains cached in CPL3 until a later FRAME is committed. The
48-byte/64-byte GUI event forms, old client envelopes and original draw types
are unchanged. `bitmap.c` adds private-buffer rectangle/line clipping, invert,
flip and strict BMP24 decoding helpers; decoder failure leaves its output plan
unchanged.

Images (`KGIMAGE.EXE`) opens uncompressed 24-bit BITMAPINFOHEADER BMPs from the
read-only A: root or broker-selected Work. It validates file extent, reserved fields, dimensions, stride,
BI_RGB, planes and bit depth, accepts top/down or bottom/up rows, closes handles
on every path, and converts BGR to RGB565. Load failures preserve current pixels,
original and Undo. Load a name, use Invert / Flip H / Undo, toggle Paint (P) and
drag a magenta 3-pixel brush. R resets, arrows pan, Q/Close exits. Edits are kept
in private memory; [IMAGE-SAVE.md](IMAGE-SAVE.md) describes explicit Work saving,
revision checks and TMP/BAK recovery. F2 returns to the launcher,
F10 ends the desktop. Files' Open app sends root BMPs to Images and TXT files to
Editor, including BMP files in Work. SAMPLE.BMP is an original deterministic fixture.

GUI packages optionally compact file payloads to 16-byte alignment and omit
section/symbol tables. Their virtual segments still begin on 4 KiB pages and
retain every loaded byte, address, permission, BSS size and entry point. The
parser accepts both file alignments (16 or original 4096), checks compact payloads
start after program headers, and retains all overlap/overflow/private-page checks.
This keeps the complete FD, old fixtures and usable Work capacity together.

KGBITBAD.EXE issues actual CPL3 invalid buffers, sizes, offsets, peer-token/handle
forgery, incomplete frames, source overflows and stale handles. Its valid test
frame uses a negative destination to exercise exact one-pixel clipping. The
native producer's reserved[8..23] publishes four surface handles, uploaded bytes,
pending revisions and owner generations. CPL3 GUI debug fields after byte 192
publish private cache metadata/pointers for read-only CPU/PTE-matched CI inspection.
