# Canvas sprites

Canvas.CreateSprite() now creates an AQVMT script object. QuadooForm.dll has no
Sprite class, registration, descriptor, collection, JSON member, or dependency
on QuadooScript. Existing Canvas layout and serialization remain unchanged.

```text
var sprite = Drawing.CreateSprite();
sprite.LoadFile("player.png");
sprite.Draw(100, 50);
sprite(100, 50);
```

CSpriteObject in ScriptAdapter/SpriteObjects.cpp implements IQuadooObject using
CBaseUnknown and BEGIN_UNK_MAP. CanvasBindings delegates only CreateSprite to its
factory. Sprite dispatch and state do not grow CComponentObject::Invoke.

## Script API

| Operation | Contract |
|---|---|
| `Drawing.CreateSprite()` | New, empty object associated with this Canvas |
| `sprite.LoadFile(path)` | PNG, BMP or ICO, detected from file content |
| `sprite.LoadPng(binary)` | PNG bytes from an existing ILockableStream-backed binary object |
| `sprite.LoadPixels(width, height, binary[, format[, stride]])` | Owned copy of raw pixels |
| `sprite.Draw(x, y)` | Immediate clipped drawing in physical client pixels |
| `sprite(x, y)` | Same validation, drawing, result, and errors as Draw |
| `sprite.Width`, `sprite.Height` | Read-only source dimensions; zero when empty |
| `sprite.IsLoaded` | Read-only Boolean |
| `sprite.Clear()` | Release source pixels; retain Canvas association |

Raw formats are case-sensitive: RGB24 (default), RGBA32, and BGRA32. Input is
top-down with channels in the order named by the format. RGB24 is opaque.
Omitted stride is width * bytesPerPixel. An explicit stride must be positive and
at least the packed row width. The binary length must equal stride * height,
including final-row padding. Padding is ignored; extra/truncated bytes, negative
stride, invalid formats, and overflowing dimensions are rejected.

Use the existing binary APIs, for example:

```text
var png = Host.FileFromBase64(null, "...PNG base64...");
sprite.LoadPng(png);
var rgba = Host.FileFromHex(null, "ff0000ff00ff007f");
sprite.LoadPixels(2, 1, rgba, "RGBA32");
```

The VM exposes binary bytes through indexed Data access (`rgba.Data[0]`), not a
stand-alone Data property. Loading copies pixels and releases the binary lock
before returning. Modifying the binary afterwards does not modify the Sprite.

Loading is transactional: only a fully decoded replacement is committed. A
provider callback that destroys the Canvas causes the load to fail while retaining
the previous image. Reentrant Sprite operations from binary Lock/Unlock callbacks
return EInvalidState. Mutations and property access are confined to the Sprite's
creation thread. Properties and Clear remain usable after Canvas destruction;
Draw and new loads fail safely against the destroyed native control.

An empty Sprite cannot draw. Sprite X/Y accept the full signed 32-bit range,
including partial negative clipping. Entirely clipped draws succeed without
invalidation. Intersection arithmetic uses 64-bit intermediates before narrowing.
Draw/Load/Clear return Null to script. There are no transforms or implicit DPI
scaling: source pixels always retain their intrinsic physical size.

## Default invocation

The existing VM INVOKE_INDIRECT opcode calls IQuadooObject::Invoke with a null
method. CSpriteObject checks this before looking up any member name and calls
the same Draw helper used by the named method. It constructs no method-name
string and performs no named-method comparison on that route. No compiler, VM
opcode, or QuadooScript interface change was necessary.

The current compiler treats bare global call names as functions. For a Sprite
stored in a global/property, use a local alias for callable syntax, or its named
Draw method directly:

```text
var brush = Player;
brush(20, 20);
Player.Draw(20, 20);
```

The tests use the same local variable lookup in both benchmark routes, and also
verify byte-identical exported PNGs for explicit/default drawing.

## Ownership and surface validity

Ownership is one-way:

`script reference -> CSpriteObject -> IQuadooCanvas -> native Canvas`

CSpriteObject also owns an immutable CSpriteImage containing tightly packed,
top-down **straight BGRA32** bytes. Canvas never retains Sprites, their source
images, placements, or script objects. No native handles or destination pointers
are stored in the Sprite. An interface reference keeps the implementation alive;
it does not keep its HWND alive.

Every Draw calls the retained IQuadooCanvas::GetSurface afresh. It validates the
borrowed HDC/HBITMAP/SIF_SURFACE, selected bitmap, DIB pixels, dimensions, pitch,
and 32-bit format. The authoritative structure is platform/SIF/Published/SIF.h:
pbSurface, cBitsPerPixel, xSize, ySize, lPitch. No SIF library is linked and no
parallel surface structure/interface was added.

GetSurface now initializes every available output to null before validation,
checks thread/lifetime/draw guards and current dimensions, and allocates a fresh
persistent surface lazily when needed. Outside OnPaint it requires AutoRedraw.
During paint it rejects a stale surface generation. In an unbuffered transient
paint, a request promotes the current paint to a temporary DIB, preserving prior
GDI drawing; presentation uses that DIB after the callback. This also supports
sprites when AutoRedraw=False and DoubleBuffered=False.

Borrowed outputs are valid only until a resize, mode change, destruction, or
another reentrant operation replaces the target. GetSurface is not a lock or
lease. Safety comes from synchronous UI-thread execution: Sprite performs no
script/provider calls and pumps no messages between GetSurface and the last
pixel write. Calls made by a foreign native GetSurface implementation must obey
the same contract. No additional pin/release interface was introduced.

Prior GDI writes are flushed before CPU access. The blend clips first, respects
pitch, skips alpha 0, copies alpha 255, and blends intermediate channels using
`(source * alpha + destination * (255 - alpha) + 127) / 255`. Canvas destination
alpha remains 255. Invalidate is called once after changed pixels; the existing
Canvas suppresses nested invalidation during OnPaint. Persistent paints preserve
pixels; transient paints require the script to draw its scene again.

## Image loading and bounds

PNG uses the existing QFI/lodepng decoder and retains straight source channels.
BMP import and ICO selection/decoding reuse CImageResource. The latter's existing
premultiplied output is converted once to owned straight BGRA. Low-alpha BMP/ICO
channels retain the existing decoder's quantization. Unsupported classic ICO
invert/XOR pixels are rejected by the existing renderer; alpha and ordinary mask
icons are supported. BMP import disables the optional legacy PNG palette profiler.
No new image codec or SIF DLL dependency was added.

Each decoded image uses the existing 4096 dimension / 4,194,304 pixel limits.
Encoded PNG/file input is at most 8 MiB. Owned decoded Sprite images share a
64 MiB budget per application adapter module; atomic replacements temporarily
count both old and new images. Decoder workspace retains its separate existing
QFI limits. Failed reservations and allocations release their accounting.

## ABI and build

The user-provided GetSurface appends a method to IQuadooCanvas. The revised
interface therefore has UUID D25A0D6A-34B4-48EA-AB90-3C8AB72A0056, replacing the
previous 0055 capability. Rebuild native consumers and implementers together;
this is an interface version change. Other public interface UUIDs/vtables were
not changed. There is no additional public surface-access or Sprite interface.

AQVMT and QuadooApplicationTests compile SpriteObjects, SpritePixels, and the
reused QFI source files directly. They retain the VS2008/no-STL/no-exceptions
toolchain. The SIF dependency is header-only. QuadooForm.dll remains VM-independent.

```powershell
tests\platform\QuadooForm\build-application-lifetime.cmd Debug Win32
tests\platform\QuadooForm\build-application-lifetime.cmd Release Win32
tests\platform\QuadooForm\test-canvas.ps1 -Configuration All -Regression
# Offscreen GDI/codec tests only: no HWND, COM initialization or message loop.
tests\platform\QuadooForm\test-canvas.ps1 -Configuration All -Headless
```

The Canvas runner now includes QuadooApplicationTests --sprite-headless. Its
existing --canvas-only script selection includes native Sprite and actual VM
tests; --sprite-only is an alias for the same focused selection. Builds reuse
SimbeyCore.dll, JSON.dll, and the compiler/VM dependency DLLs.

## Example and validation scope

DesignerExamples/CanvasApplication now demonstrates file PNG, embedded PNG binary,
and raw RGB24 loading, both drawing call forms, partial negative clipping, and
PNG export. Sprites remain ordinary runtime script properties and are absent
from the form JSON and Objects tree. Sprite.png is supplied beside the project;
Studio uses that directory as the example's working directory. Its synchronized
source and embedded executable resources are checked by Studio integration tests.

Tests cover guarded/padded buffers, exact alpha 0/1/127/254/255 composition over
several colors, all clipping edges, raw formats/strides, file/memory equivalence,
PNG/BMP/alpha ICO loading, replacement failure, memory limits, repeated Sprite
lifetimes/GDI resources, wrong threads, reentrant binary providers, DPI/resize,
destroyed Canvas references, both transient paint modes, and decoded PNG output.
Native Sprite calls consume only the published IQuadooCanvas capability.

The broader application script suite has the previously documented unrelated
StatusBar Panels indexed-property dispatch failure; the focused Sprite/Canvas
selection isolates it. No claim is made that the broader script suite is green.
Actual Windows XP/ReactOS VM execution and interactive visual review remain
pending. Modern offscreen/native tests and PE compatibility checks are reported
separately and do not establish execution on those systems.

## Implementation validation (2026-11-03)

VS2008 Win32 Debug and Release builds completed with zero errors and warnings.
The individual-project build runner did not rebuild SimbeyCore, JSON, or the
QuadooScript compiler/VM dependency DLLs.

| Validation | Debug | Release |
|---|---:|---:|
| Native offscreen Canvas | 18,113 passed | 18,113 passed |
| Offscreen Sprite pixels/codecs/failures | 4,976 passed | 4,976 passed |
| Canvas HWND harness | 96 passed | 96 passed |
| Designer Canvas | 34 passed | 34 passed |
| Studio Canvas/sample packaging | 6 passed | 6 passed |
| Focused native Sprite and actual VM/Canvas | 57 passed | 57 passed |
| Palette regression | 1,501 passed | 1,501 passed |
| Full native harness | 9,890 passed | 9,890 passed on rerun |
| Full private native tests | 19,657 passed | 19,657 passed |

The final focused tests include OnCreate Sprite creation/drawing, retention of a
destroyed Canvas by its Sprite, factory instance counts before/after final Sprite
release, and zero Sprite objects/owned image bytes after runtime shutdown and VM
unload. Before the application message loop starts, the runtime intentionally
retains its form script and that script's OnCreate Sprite until sweep/shutdown.

The initial Release full-harness run failed the existing focus-sensitive
KeyboardMemoTests.h:156 default-button assertion. The isolated keyboard test
passed 1,017 assertions, then the complete harness passed all 9,890 on rerun.
No assertion or keyboard implementation was changed. The separate broader
application script run still fails in the preexisting RuntimeObjects.cpp Panels
branch, which queries IQuadooListView and returns E_NOINTERFACE. Sprite tests do
not hide or repair that unrelated dispatch issue.

Three alternating 10,000-draw VM batches used a local Sprite variable and a
16x16 partially transparent source at clipped positions. Final aggregate times:

| Configuration | Explicit Draw | Default call | Draws per route |
|---|---:|---:|---:|
| Debug | 229,370 us | 229,409 us | 30,000 |
| Release | 94,312 us | 92,351 us | 30,000 |

These measurements include script dispatch, validation, GetSurface and blending.
They are observations, not a speed guarantee. Native default dispatch bypasses
the method-name table; both routes use the same drawing helper.

Build logs are target/Sprite-Debug-build.log and Sprite-Release-build.log. Suite
logs are target/Sprite-Debug-tests.log, Sprite-Release-tests.log and CanvasLogs;
the latter Release aggregate records its initial keyboard failure. Final focused
logs are target/Sprite-{Debug,Release}-focused-final.log. Successful Release
reruns are Sprite-Release-keyboard.log, Sprite-Release-native-final.log and
Sprite-Release-private.log. PE headers/imports were inspected in Sprite-imports.log:
QuadooForm.dll and AQVMT.bin are x86, subsystem version 5.00, with no SIF DLL import.
AQVMT reuses GDI+ for BMP import; QuadooForm.dll remains VM-independent.

Commands for the focused checks and the Release reruns, from the repository root:

```powershell
tests\results\bin\Debug\QuadooApplicationTests.exe --sprite-only tests\desktop\QuadooStudio\ApplicationTests\Runtime.quadoo
tests\results\bin\Release\QuadooApplicationTests.exe --sprite-only tests\desktop\QuadooStudio\ApplicationTests\Runtime.quadoo
tests\results\bin\Release\QuadooFormHarness.exe --keyboard-only --trace
tests\results\bin\Release\QuadooFormHarness.exe --continue-on-failure --trace
tests\results\bin\Release\QuadooFormNativeTests.exe
```

The implementation spans ScriptAdapter/SpriteObjects and SpritePixels, Canvas
binding/surface access/presentation, private QFI BMP-import configuration, AQVMT
and ApplicationTests project wiring, focused tests/runner, and the CanvasApplication
sample. No Sprite palette assets, form schema changes, native component factory,
or unrelated runtime refactoring were introduced.

Manual follow-up: run CanvasApplication in Studio on Windows XP/ReactOS; verify
the file/memory/raw samples, clipped transparency, repaint and resize visually,
then export PNG. Exercise transient OnPaint drawing with DoubleBuffered both on
and off, including partial-window invalidations. The automated HWND/WM_PRINTCLIENT
tests cover pixel results on this workstation; interactive and VM execution remain
unverified.

## Clipping hardening (2026-11-04)

The existing compositor already intersected overlapping images. Its remaining
restrictions were a +/-1,000,000 coordinate check and source decoder limits
incorrectly applied to a borrowed destination surface. The compositor now accepts
all signed 32-bit coordinates and validates destination row bytes, positive pitch
and addressable byte span independently of source decoding limits. It intersects
the destination rectangle first, then calculates source offsets and pixel pointers.
INT_MIN/INT_MAX and an oversized Sprite crossing several edges therefore cannot
overflow intermediate coordinates. Alpha blending and synchronous GetSurface use
retain their existing contracts.

Source image allocation/decoding still uses its documented limits. The native
Canvas allocator has its own existing resource limits; accepting a large borrowed
surface in the Sprite compositor does not enlarge that allocator.

Both VS2008 Win32 configurations passed 5,577 headless Sprite assertions. Added
coverage uses a 7x5 source with distinct pixels over a guarded, padded 3x2 target;
it checks every edge/corner, no-overlap cases, extreme coordinates, and destination
surfaces exceeding the decoder's dimension or total-pixel limits. The previous
alpha, codec, replacement-failure, allocation and ownership checks also passed.
Logs: target/SpriteSound-{Debug,Release}-sprite.log. No HWND/interactive or XP VM
tests were run for this scheduled pass.

The companion AQVMT Host sound implementation and commands are documented in
[SoundImplementation.md](../QuadooScript/AQVMT/SoundImplementation.md).
