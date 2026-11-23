# Canvas implementation

Canvas is an HWND-backed drawing control registered by QuadooForm.dll. Studio and
the stand-alone Designer discover it through the normal factory, metadata, and
palette paths. It has a new 20x20 palette icon and a design-mode Canvas hint.
The hint is painted on the presentation DC and never enters an exported image.
Canvas is not a container; existing Panel, Image, and ImageList roles are retained.

## Public and script contracts

The initial Canvas implementation added optional `IQuadooCanvas` (UUID ending
0055), plus `CanvasShapeFlags` and `CanvasScaleMode`. The later user-provided
GetSurface extension changes that interface's vtable; the current version uses
UUID `D25A0D6A-34B4-48EA-AB90-3C8AB72A0056`. Rebuild its consumers together.
See [SpriteImplementation.md](SpriteImplementation.md) for the surface contract
and script-only Sprite extension. Other interface vtables are unchanged. Native
`DrawTextString` avoids the Win32 DrawText macro; the script method is `DrawText`.
Native Canvas has no QuadooScript dependency. The script adapter queries the
published capability and preserves external `m_srNativeHandler` precedence.

| Script method | Arguments and result |
|---|---|
| `Clear(color)` | Fill the current drawing target |
| `SetPixel(x, y, color)` | Write one pixel |
| `GetPixel(x, y)` | Return a COLORREF |
| `GetPixelSize()` | Independent object with Width and Height |
| `DrawLine(x1, y1, x2, y2, color)` | One-pixel GDI line; final endpoint excluded |
| `DrawRectangle(bounds, outline, fill[, flags])` | bounds has Left, Top, Width, Height |
| `DrawEllipse(bounds, outline, fill[, flags])` | Same geometry and flags as rectangle |
| `DrawText(text, x, y, color)` | Literal Unicode text with transparent text background |
| `MeasureText(text)` | Independent object with integer Width and Height |
| `SetFont(name, points, bold, italic)` | Update the inherited Font property |
| `ResetFont()` | Restore the factory Font default |
| `DrawImageList(images, index, x, y)` | Draw an acquired snapshot at the current DPI |
| `ExportPng()` | Independent binary object, ContentType `image/png` |
| `ResolveColor(quadooColor)` | Validate and resolve a Quadoo Color to COLORREF |
| `Invalidate()` | Schedule repaint; coalesced/no-op during this Canvas's paint callback |
| `CreateSprite()` | Create a callable, script-only Sprite associated with this Canvas |

Shape flags are Outline=1 and Fill=2; the default is 3. Native rectangle bounds
are right/bottom exclusive. Negative sizes, invalid flags, integer overflow, and
invalid colors are rejected. Drawing coordinates are bounded to +/-1,000,000
before entering GDI. GetPixel/SetPixel reject out-of-bounds indexes.

Drawing colors are **opaque COLORREF, 0x00BBGGRR**, not web RGB or alpha values.
Resolve tagged system colors with ResolveColor first. Descriptor-backed colors
retain their Quadoo Color representation until rendering.

Width/Height and persisted bounds remain 96-DPI logical units. Every drawing
coordinate, rectangle, pixel size, and text measurement is in **physical client
pixels**. Font size is in points. Do not rescale an acquired ImageList image.

MeasureText and DrawText share the same selected font and text-layout helper.
They preserve trailing spaces, treat ampersands literally, expand tabs to eight
spaces from the line origin, and recognize CRLF/CR/LF. Trailing empty lines count.
Empty text measures 0x0. Height uses the font's line height without external
leading. Width is the layout advance, not an italic glyph's overhang bounds.
There is no automatic wrapping, ellipsis, mnemonic processing, or NUL truncation.
Embedded NUL is rejected. Font fallback follows Windows GDI.

Native MeasureText fills a SIZE. Native ExportPng returns an AddRef'd
ISequentialStream positioned at byte zero and its exact byte count. The adapter
wraps that owned stream using the existing binary factory. Returned measurements
and encoded binaries remain independent after the Canvas changes or is destroyed.

## Drawing, lifetime, and persistence

AutoRedraw=True, DoubleBuffered=True, ClearOnResize=False, and ScaleMode=Pixels
are defaults. PixelWidth and PixelHeight are read-only runtime properties.

With AutoRedraw=True, drawing updates an opaque top-down BGRA DIB. Repaints
present that DIB; OnPaint may update it before presentation. Resizing preserves
the top-left physical overlap and fills new pixels from BackColor. ClearOnResize
instead starts fresh on a size/DPI change. Changing BackColor alone does not
recolor existing pixels. Switching AutoRedraw off discards the saved contents.

With AutoRedraw=False, mutating drawing is allowed only within this Canvas's
OnPaint callback. Each paint starts with BackColor. DoubleBuffered selects a
temporary DIB or direct paint-DC drawing. GetPixel and ExportPng are unavailable
in this mode. MeasureText remains available outside paint and at zero size.

OnPaint exposes read-only ClipLeft/Top/Width/Height. OnResize exposes physical
PixelWidth/PixelHeight and is deferred/coalesced after layout. OnClick and mouse
down/up/move events are supported, with physical X/Y and Win32 button/key masks.
Focus and keyboard behavior use the inherited WinControl infrastructure.
Design/Preview do not invoke script drawing callbacks.

Paint and draw scopes retain their resources and the control across callbacks.
Nested paint/print cannot recursively dispatch OnPaint. Destruction, resize,
ImageList acquisition reentrancy, and mode changes cannot leave the active DC
dangling. Deferred resize work runs after the active scope exits. Callback errors
use the existing diagnostic path. Mutating draws never recursively invalidate
from OnPaint. WM_PAINT and WM_PRINTCLIENT use the existing Message override;
there is no additional window subclass.

All operations are UI-thread-affine. Use Form.Async/AsyncWait to marshal work.
Zero-size controls cannot draw/export an old allocation as the current image.
An allocation failure retains old pixels for recovery but rejects operations
against mismatched dimensions. The existing imaging caps apply: dimension 4096,
4,194,304 pixels, and encoded PNG 8 MiB. Exports are RGBA8 with alpha 255.

Only configuration and event names enter form JSON. Pixel mutations, measurement,
and export neither notify descriptor changes nor dirty the designer document.
Transparent backgrounds, alternate scale modes, variable pen widths, text wrapping,
and underline/strikeout font extensions are deferred. There is no Transparent
descriptor and only Pixels is offered by ScaleMode.

## Reuse and supporting repairs

CanvasSurface owns the existing QFI::CImageSurface rather than another bitmap/DC
implementation. GDI writes are flushed before CPU pixel access and alpha repair.
ImageList drawing uses only IQuadooImageList and its retained snapshot; no private
ImageList class is required.

The new composition tests exposed double premultiplication when uploading QFI
rasters through common-controls v6 ImageList_Add. NativeImageList now supplies
straight BGRA to v6, while retaining the previous classic-common-controls path.
ListView checkbox/image composition now reconstructs premultiplied RGB and
coverage from black/white native draws, then reuses that same upload helper. This
avoids the accumulated color rounding in Merge/GetIcon/ReplaceIcon and handles
legacy masks as well as alpha images. Existing pixel assertions remain unchanged.

The bundled lodepng encoder ignored several allocation failures. Focused fixes
propagate scanline preprocessing errors, guard failed vector growth, and release
partial hash/Huffman/deflate allocations. The QFI encoder tracks otherwise ignored
allocation errors with explicit Win32 TLS, so a truncated PNG is not returned as
success. Canvas disables the old optional palette profiler and emits RGBA PNG;
the old profiler's unchecked allocation assumptions are not exercised by Canvas.
No additional image/PNG library or dependency DLL rebuild was introduced.

## Validation and commands

Validation completed on 2026-11-03 with the configured Visual Studio 2008 Win32
toolchain. Both affected Debug and Release builds succeeded without warnings.

| Check | Debug | Release |
|---|---|---|
| Offscreen pixels/text/PNG and allocation failures | Passed (18,113 assertions) | Passed (18,113) |
| Canvas native HWND, lifecycle, reentrancy and DPI | Passed (96) | Passed (96) |
| Shared Designer Canvas editing/JSON | Passed (34) | Passed (34) |
| Studio source integration and sample executable build | Passed (6) | Passed (6) |
| Real QuadooScript Canvas callbacks and binary ownership | Passed (12) | Passed (12) |
| Shared palette resources and lifetime | Passed (1,501) | Passed (1,501) |
| Full native harness, including Image/ImageList/TreeView/ListView | Passed (9,890) | Passed (9,890) |
| Full private native storage/failure-injection/rollback suite | Passed (19,657) | Passed (19,657) |

The native suites cover both v6 common-controls and the classic non-manifested
test process on this workstation. PE inspection confirms x86/subsystem 5.0 for
QuadooForm.dll, QuadooStudio.exe, and AQVMT.bin. New TLS/module-query imports are
XP-supported APIs; this does not substitute for execution in an XP VM.

Build/test logs are target/Canvas-Debug-build.log,
target/Canvas-Release-build.log, target/Canvas-final-tests.log (Debug results),
target/Canvas-final-release-tests.log (the final fresh Release run), and the
individual files under tests/results/CanvasLogs. The separate broader script-suite
failure is recorded in target/Canvas-full-script-debug.log and described below.

Build affected individual projects without rebuilding SimbeyCore, JSON, or the
QuadooScript compiler/VM dependency DLLs:

```powershell
tests\platform\QuadooForm\build-application-lifetime.cmd Debug Win32
tests\platform\QuadooForm\build-application-lifetime.cmd Release Win32
tests\platform\QuadooForm\test-canvas.ps1 -Configuration All -Regression
```

The runner has 180-second per-process bounds and writes logs under
tests/results/CanvasLogs. Expected negative-case diagnostics can appear in passing logs.
The offscreen-only option creates no HWND and runs before COM initialization:

```powershell
tests\platform\QuadooForm\test-canvas.ps1 -Configuration All -Headless
```

It still requires Windows offscreen GDI for DIB sections and font metrics. It is
not a graphics-subsystem-independent renderer.

Focused entry points, using target/Debug or target/Release:

```powershell
QuadooFormNativeTests.exe --canvas-rendering-only
QuadooFormHarness.exe --canvas-only --trace
QuadooFormDesignerTests.exe --canvas-only --trace
QuadooStudioIntegrationTests.exe --canvas-only --trace
QuadooApplicationTests.exe --canvas-only "D:\dev\projects\trunk\tests\desktop\QuadooStudio\ApplicationTests\Runtime.quadoo"
QuadooFormDesignerTests.exe --palette-only --trace
```

Coverage includes native COM identity, factories, Form/Frame placement, physical
DPI sizing, font replacement, shapes/text/pixels, masked/alpha ImageLists,
third-party snapshot implementations, JSON round trips, persistent/transient
paint/print, reentrant destruction/resize, script callbacks and PNG binary
ownership. Offscreen tests decode and compare every exported pixel, and sweep
48 deterministic PNG allocation-failure positions. Studio integration also builds
the supplied sample project and verifies its embedded catalog/form resources.

Open DesignerExamples/CanvasApplication/CanvasApplication.qsproj in Studio for
the interactive example. Export PNG retains a binary snapshot in LastPng and
shows its byte count; it intentionally does not write a file automatically.

Actual Windows XP and ReactOS execution is pending; no such VM was available in
this run. On XP, test the sample at multiple DPIs, unavailable-font fallback,
cover/uncover and minimize/restore, repeated font replacement, alpha/masked
ImageLists, PNG export, and destruction from callbacks. On a modern desktop,
also inspect the sample visually for flicker and readability. Automated native
paint/print tests do not replace these platform and visual checks.

## Change map

- Published/QuadooForm.h: optional native capability and drawing enums.
- QuadooForm/Canvas.h, Canvas.cpp, CanvasOperations.cpp: factory, descriptors,
  window lifecycle, paint/print events, and native drawing operations.
- QuadooForm/CanvasDrawing.h/.cpp: retained DIB ownership, shared text layout,
  color validation, and PNG snapshot creation.
- ScriptAdapter/CanvasBindings.h/.cpp and RuntimeObjects.cpp: capability dispatch,
  checked arguments, independent geometry values, and existing binary wrapping.
- Registry.cpp, PaletteResources.h, QuadooForm.rc, Icons/Canvas.png/.ico, and
  Icons/GenerateIcons.py: registration and standard palette resources.
- NativeImageList.h/.cpp and ListVirtualImages.cpp: shared image upload and
  alpha-preserving checkbox composition required by the rendering regressions.
- Imaging/PngCodec.cpp, ImageResource.h, QuadooForm.cpp, and the existing
  shared/3rdparty/lodepng/lodepng.cpp: bounded encoding failure/cleanup handling.
- CanvasTests.h in Harness, TestInternals, DesignerTests, and Studio
  IntegrationTests, plus ApplicationTests/CanvasScriptTests.h,
  CanvasRuntime.quadoo, and CanvasForm.json: validation and fixtures.
- Affected .vcproj files, test entry points and palette counts: build/test wiring.
- DesignerExamples/CanvasApplication: a complete, source-synchronized Studio
  Application project with a Default form and embedded ImageList data.

The broader existing ApplicationTests Runtime.quadoo run is not green: its
StatusBar path reaches RuntimeObjects.cpp's existing GetIndexedProperty branch
for `Panels`, which queries IQuadooListView and returns E_NOINTERFACE. This code
predates Canvas and was left unchanged. The dedicated Canvas script suite passes
without excluding any Canvas operations. Full Designer/Studio suites were not
claimed; their focused Canvas and shared palette suites were run. Existing
clipboard-dependent tests have an environment-dependent clipboard-read failure.
