# QuadooForm Canvas Control — Implementation Plan

Updated 2026-11-03 after reviewing the current native runtime, imaging helpers, and script adapter. Implementation was subsequently authorized and completed; see [CanvasImplementation.md](CanvasImplementation.md) for the implemented API, validation results, and remaining platform checks. Text measurement is part of the initial control.

## Objective

Implement a native **Canvas** control for QuadooForm, inspired by the programmable drawing capabilities of the Visual Basic 6 `PictureBox`. This complements the existing **Image** control (displaying pictures) and **Panel / ScrollingPanel** controls (containing other controls). The new Canvas is primarily a script-accessible drawing surface, not a replacement for those controls.

A QuadooScript application should be able to place a Canvas on a Form, draw pixels, lines, rectangles, ellipses, text, and images from an existing **ImageList**, measure arbitrary text with the currently selected Canvas font, and export the current canvas contents to a QuadooScript **binary object containing PNG-encoded data** using the project's existing **lodepng** integration. The Canvas should handle repainting and optional persistent drawing without requiring scripts to manage an `HDC`.

**Please inspect the current repository implementations before changing interfaces or writing new decoding/rendering infrastructure.** The interface and method names in this document are proposed contracts, not assumptions that matching declarations already exist. Adapt them to the established `IQuadoo*` ABI, property/event descriptors, `RSTRING`, native-object registration, binary-value type, color conventions, and serialization framework.

### Compatibility and project constraints

- Native Win32 C++, Visual Studio 2008 compatible; preserve Windows XP support and test ReactOS where feasible.
- Follow existing QuadooForm / Quadoo Studio conventions. Do not introduce STL dependencies, C++ exceptions, a new graphics toolkit, or a parallel image/PNG library.
- Do not alter published interfaces in place; use an appropriate new/optional interface for canvas-specific operations.
- Preserve designer/runtime separation, correct control lifetime, and the existing thread-affinity model.

## 1. Relationship to VB6 PictureBox

VB6 offered two different standard image-oriented controls:

- **Image**: lightweight picture display, with little direct drawing support.
- **PictureBox**: picture display **plus** drawing methods (`PSet`, `Point`, `Line`, `Circle`, `PaintPicture`, `Print`, `Cls`), optional persistence via `AutoRedraw`, a device context, and the ability to host child controls. It also offered alternate coordinate systems through `ScaleMode`.

For QuadooForm, implement **Canvas** as the focused drawing surface. **Do not make it a container in this iteration.** QuadooForm already provides Panel and ScrollingPanel; a later PictureBox-like container could compose these facilities if genuinely useful.

## 2. Control registration and designer integration

- Register **Canvas** with the normal control factory and descriptor system.
- Add a distinct 20×20 Toolbox icon and appropriate Toolbox placement (near Image / Panel).
- Implement as an **HWND-backed `IQuadooWinControl`**, consistent with the current base-class architecture. It owns its paint lifecycle and receives normal mouse events; support focus/keyboard events if the common WinControl layer makes them appropriate.
- Canvas can be positioned, resized, named, selected, deleted, serialized, and previewed like other windowed controls.
- Add its properties, required `OnPaint`, Canvas-specific `OnResize`, and input events using the existing typed event/delegate infrastructure. Common WinControl metadata currently supplies focus/keyboard events, but not a generic paint or resize event; define these in Canvas metadata rather than assuming they already exist.
- Its design-time surface should visibly indicate a Canvas, but must not invoke application drawing/event handlers in designer mode. Preview should follow existing Quadoo Studio semantics for suppressed script callbacks.
- The Canvas is **not** an `IQuadooStaticControl` and is **not** a child-control container.

## 3. Proposed public properties

| Property | Type | Initial default | Meaning |
|---|---|---|---|
| `BackColor` | Quadoo Color | `COLOR_WINDOW` (or existing equivalent) | Background fill; may be a mapped system color |
| `AutoRedraw` | Boolean | `True` | Preserve rasterized pixels in an offscreen backing bitmap; no command replay list |
| `DoubleBuffered` | Boolean | `True` | Present the resulting drawing without flicker |
| `ScaleMode` | Enum | `Pixels` | Drawing coordinate units; only Pixels required initially |
| `ClearOnResize` | Boolean | `False` | Whether a resize clears persistent contents rather than preserving the overlap |
| `Transparent` | Deferred | — | Do not publish a descriptor or script property in the initial opaque implementation |
| `PixelWidth` / `PixelHeight` | Read-only runtime Int | Actual client dimensions | Drawing-surface dimensions in physical pixels; distinct from logical `Width` / `Height` |

`Width`, `Height`, `Left`, `Top`, `Visible`, `Enabled`, `Font`, and other inherited properties should continue to behave according to the existing framework. Avoid duplicating inherited properties.

**Scope decisions:**

- Implement `ScaleMode=Pixels` correctly first. Investigate whether alternate coordinate systems (twips, points, inches, millimeters, user-defined) fit the existing logical/DPI coordinate infrastructure; if not, **do not advertise unsupported values**. Add them in a later pass rather than implement partially correct conversions.
- Defer `Transparent` and all transparent-background modes. The first implementation composites onto an opaque background and exports alpha 255. ImageList inputs can still contain transparency.
- `AutoRedraw` and `DoubleBuffered` must remain semantically distinct.

### Coordinate and DPI contract

- Control bounds, docking, anchors, and persisted Width/Height remain **96-DPI logical units**, as required by the existing framework.
- `ScaleMode=Pixels` means Canvas drawing coordinates, native `RECT` arguments, `GetPixel`/`SetPixel`, text measurements, and PixelWidth/PixelHeight use **physical client pixels**. Do not scale those inputs again in the adapter.
- Use `GetClientRect()` for pixel dimensions rather than rounding the stored logical size a second time. A measured text size can be used directly with drawing coordinates.
- Font Size remains an integer number of points; the existing font/DPI path produces the physical font. ImageList `Acquire(currentDPI)` supplies its already scaled image dimensions.
- A DPI change changes text metrics and the physical surface size. With ClearOnResize=False, copy the top-left physical-pixel overlap without stretching the old bitmap; subsequent drawing uses the newly scaled font. Applications can redraw the whole scene after OnResize.
- Initially expose only the Pixels enum choice. Reject other values during JSON/property validation.

## 4. Backing surface and painting semantics

Use a **top-down 32-bit DIB section** as the preferred persistent pixel representation. Reuse existing graphics helpers where they can meet these requirements. Keep any `HBITMAP`, memory `HDC`, and selected original GDI objects under explicit RAII-equivalent ownership consistent with the codebase.

### `AutoRedraw = True`

- Graphics operations modify a persistent backing bitmap.
- `WM_PAINT`, uncovering, minimization/restoration, and other invalidation should present the saved contents, not erase prior drawing.
- Resizing with `ClearOnResize=False` preserves the overlapping existing pixels, fills newly exposed pixels with the resolved background color, and recreates the surface only if needed.
- Resizing with `ClearOnResize=True` resets the backing pixels to the background.
- The bitmap should be allocated lazily when appropriate; handle zero-size and allocation failures cleanly.

### `AutoRedraw = False`

- Drawing is nonpersistent and normally belongs to an `OnPaint`-style callback (or an explicitly documented transient drawing operation).
- A redraw must be reproducible by the app; contents are not guaranteed to survive invalidation.
- Implement the bounded OnPaint path as part of this control. In this mode all mutating draw methods, including Clear, are permitted **only while this Canvas is dispatching its own OnPaint callback**; otherwise return QuadooForm::EInvalidState. Do not use GetDC(HWND) to provide undocumented transient drawing outside paint.
- Begin each transient paint with the current resolved BackColor. DoubleBuffered=True uses a temporary full-client surface for that paint; False draws directly into the current paint DC. Neither creates persistent pixel state.
- GetPixel and ExportPng return QuadooForm::EInvalidState in this mode, including from OnPaint. MeasureText remains available outside paint and does not require a pixel surface.
- Avoid recursive paint/invalidate/event loops when a draw operation is performed from OnPaint. See the paint-scope rules in section 10.

### `DoubleBuffered`

- Means the final rendered content is composed offscreen before presentation, reducing flicker.
- It does **not** imply persistence by itself.
- Avoid redundant allocations/copies if the persistent DIB can already be presented directly without flicker.
- Ensure background erasure does not introduce flashing (`WM_ERASEBKGND` policy should be coordinated with `WM_PAINT`).

### State changes and allocation failures

- Reuse `QFI::CImageSurface` in Imaging/ImageSurface.h/.cpp instead of adding another DIB/DC owner. Its Create/Swap pattern already builds a candidate before replacing the old surface. Add only small private accessors/helpers if required; keep general image behavior unchanged.
- Use the current QFI bounds as the first implementation limit: maximum dimension 4096, maximum 4,194,304 pixels, and maximum encoded PNG 8 MiB. Check multiplication, stride, rectangle addition, and text-height arithmetic before allocating or narrowing to Win32 INT/ULONG. Do not increase shared limits silently.
- Zero-sized/minimized controls have no drawable/exportable current surface. Retain the previous persistent allocation when useful for a later restore, but never export stale dimensions. MeasureText is still valid for a live zero-sized control.
- A failed resize/allocation must preserve the previous bitmap for recovery, without exposing it as if it had the new size. Drawing/export report the failure; paint can fill the new client background and record a diagnostic. Retry after dimensions/resources change, rather than starting a paint/error loop.
- ClearOnResize=True clears on a real surface-size/DPI change; repeated layout of identical dimensions does not clear the drawing. ClearOnResize=False preserves only the overlap.
- Changing BackColor affects new allocations, newly exposed pixels, transient paints, and later clears caused by resize. It does not recolor pixels already stored in a persistent canvas. Clear(color) changes pixels without changing BackColor.
- True-to-False AutoRedraw discards persistent contents after the mode change commits. False-to-True starts a fresh background-filled surface; it does not capture screen contents. Reject mode/surface-affecting property changes during an active paint/draw scope, or defer internal resize work until that scope exits.

### Pixel-format and alpha conventions

- **All drawing methods accepting a `color` parameter take a plain Win32 `COLORREF`**, with the normal `0x00BBGGRR` layout; these are not Quadoo Color values.
- An ordinary `COLORREF` represents an **opaque color**; do not interpret its high byte as alpha.
- Keep the internal DIB byte order, premultiplied/straight alpha handling, and PNG export conversions explicitly defined. GDI generally draws RGB without guaranteeing meaningful destination alpha bytes; repair/set alpha where needed so the exported PNG does not become accidentally transparent.
- Alpha-blended ImageList drawing should preserve correct compositing; don't create dark halos or discard transparent edges. For an opaque Canvas background, exported pixels will normally have alpha 255. If transparent canvas backgrounds are eventually supported, preserve genuine per-pixel alpha as appropriate.
- Source imaging from ImageList and encoded PNG export must have consistent BGRA/RGBA conversion and dimensions.
- The initial persistent DIB is opaque BGRA8 (A=255, so premultiplied and straight RGB are identical). Composite transparent source images onto it before export; do not copy source alpha into the opaque destination.
- Track pending GDI writes. Before CPU pixel access, copying pixels for resize/export, or repairing alpha, call GdiFlush and handle failure. Do not assume that UI-thread affinity alone completes batched GDI operations. Microsoft documents this synchronization requirement for [CreateDIBSection](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-createdibsection).
- After GDI writes, restore alpha 255 in the affected region before direct reads/export; do not premultiply already-composited RGB again. Keep dirty bounds conservative for glyph overhang and antialiasing.

## 5. Canvas drawing interface

Add a script-accessible **`IQuadooCanvas`** (or an appropriately named optional interface) using the repository's existing COM-style interface conventions. The exact C++ return/parameter patterns should match existing QuadooForm and QuadooScript APIs; the sketch below is conceptual:

```cpp
// Assign a new, unused interface UUID when implementing this proposal.
// All SIZE/RECT values here are physical pixels, not QuadooForm::Size/Rect.
interface IQuadooCanvas : IUnknown
{
	STDMETHOD(Clear)(COLORREF color) = 0;
	STDMETHOD(SetPixel)(INT x, INT y, COLORREF color) = 0;
	STDMETHOD(GetPixel)(INT x, INT y, COLORREF* color) = 0;
	STDMETHOD(GetPixelSize)(SIZE* size) = 0;
	STDMETHOD(DrawLine)(INT x1, INT y1, INT x2, INT y2,
		COLORREF color) = 0;
	STDMETHOD(DrawRectangle)(const RECT* bounds,
		COLORREF outlineColor, COLORREF fillColor, DWORD flags) = 0;
	STDMETHOD(DrawEllipse)(const RECT* bounds,
		COLORREF outlineColor, COLORREF fillColor, DWORD flags) = 0;
	STDMETHOD(DrawTextString)(RSTRING text, INT x, INT y,
		COLORREF color) = 0;
	STDMETHOD(MeasureText)(RSTRING text, SIZE* size) = 0;
	STDMETHOD(SetFont)(RSTRING faceName, INT pointSize,
		BOOL bold, BOOL italic) = 0;
	STDMETHOD(ResetFont)(VOID) = 0;
	STDMETHOD(DrawImageList)(IQuadooImageList* images,
		UINT imageIndex, INT x, INT y) = 0;
	STDMETHOD(ExportPng)(ISequentialStream** data, ULONG* byteCount) = 0;
	STDMETHOD(ResolveColor)(COLORREF quadooColor, COLORREF* color) = 0;
	STDMETHOD(Invalidate)(VOID) = 0;
};
```

Publish a new optional UUID-bearing interface using the existing __declspec(uuid(...))/STDMETHOD conventions, without changing existing IIDs or vtables. Use native DrawTextString to avoid the Windows DrawText A/W macro changing the interface method name between builds; keep the script method named DrawText. Do not #undef a Windows API macro in the published header. Forward-declare ISequentialStream or use the existing SDK declaration; the native contract must contain no QuadooScript interfaces. The native declarations must not override the existing exposed object model. Prefer methods that are easy to invoke from QuadooScript and that return HRESULT/error details consistent with existing APIs.

Minimum operations:

| Script-facing concept | Behavior |
|---|---|
| `Canvas.Clear(color)` | Fill whole drawing surface in opaque `COLORREF` |
| `Canvas.SetPixel(x, y, color)` | Write one pixel; bounds-check |
| `Canvas.GetPixel(x, y)` | Return pixel's `COLORREF`; define behavior for alpha-bearing pixels |
| `Canvas.DrawLine(x1, y1, x2, y2, color)` | Draw a line |
| `Canvas.DrawRectangle(bounds, outlineColor, fillColor[, flags])` | Stroke/fill a rectangle |
| `Canvas.DrawEllipse(bounds, outlineColor, fillColor[, flags])` | Stroke/fill an ellipse |
| `Canvas.DrawText(text, x, y, color)` | Render text with currently selected Canvas font |
| `Canvas.MeasureText(text)` | Return an independent `{Width, Height}` result using the same selected font and layout as DrawText |
| `Canvas.SetFont(faceName, pointSize, bold, italic)` | Set the inherited Font property for drawing and measurement |
| `Canvas.ResetFont()` | Restore the factory default Font descriptor |
| `Canvas.DrawImageList(imageList, index, x, y)` | Draw selected ImageList image at pixel position |
| `Canvas.ExportPng()` | Return PNG bytes as a QuadooScript binary object |
| `Canvas.ResolveColor(quadooColor)` | Validate and resolve a Quadoo Color using the existing native resolver |
| `Canvas.Invalidate()` | Request an asynchronous repaint without changing persistent pixels |

Native C++ methods differ from script-visible results where appropriate: native MeasureText fills a Win32 SIZE; the adapter returns a fresh object with integer Width/Height properties. Native ExportPng returns an AddRef'd, independently owned ISequentialStream positioned at its first byte, plus the exact byte count. Only the adapter creates the QuadooScript binary object. There is no assumed IQuadooBinary interface.

### Geometry and fill details

- Specify a single rectangle convention matching Win32 `RECT` (left/top inclusive, right/bottom exclusive) and apply it consistently.
- Define shape flags Outline=1 and Fill=2; accept combinations 1, 2, or 3, and default to 3 in the script adapter. Keep outlineColor and fillColor as COLORREF arguments. Reject unknown flags rather than using a color sentinel for no fill/outline.
- Ensure drawing is clipped to the canvas and safe at extreme/negative coordinates.
- Script rectangle arguments use `{Left, Top, Width, Height}` for familiar field names, but their values are physical canvas pixels. Convert to native RECT with checked 64-bit addition. Negative sizes/reversed native edges are E_INVALIDARG; zero-area shapes and fully clipped operations are harmless S_FALSE no-ops.
- SetPixel/GetPixel require 0 <= x < PixelWidth and 0 <= y < PixelHeight; return HRESULT_FROM_WIN32(ERROR_INVALID_INDEX) outside these bounds. GetPixel returns resolved opaque RGB only.
- Start with line width 1. Document GDI line endpoint behavior: the start is included and the final LineTo endpoint is excluded; use SetPixel for an isolated dot. Filled/outlined rectangle and ellipse extents follow the same right/bottom-exclusive bounds.
- Validate colors and geometry before touching the surface. Reject arithmetic overflow and coordinates that cannot safely reach the selected GDI primitive; do not let extreme INT values wrap in clipping/dirty-rectangle calculations.
- Use SaveDC/RestoreDC around operations: MM_TEXT, zero origin, controlled pen/brush/font, and clipping to the client surface. Call global GDI APIs explicitly where their names overlap interface methods. Additional pen/brush state is a later enhancement.

## 6. Color conversion: Quadoo Color → COLORREF

QuadooForm properties already support a **Quadoo Color** representation that includes literal RGB values and symbolic/mapped Windows system colors. Drawing functions, however, should take **only resolved `COLORREF` values**.

Reuse the already exported ResolveQuadooColor(COLORREF) in QuadooForm/QuadooForm.cpp; do not add a second export or an alternative encoding. Add checked validation around it in the Canvas method, usable from both native code and scripts:

- Script: `Canvas.ResolveColor(quadooColor)` → `COLORREF` integer.
- Native: IQuadooCanvas::ResolveColor validates the tag/index, then calls the existing ResolveQuadooColor export. The existing export returns COLORREF rather than HRESULT and is not itself an input validator.

Requirements:

1. Accept an ordinary literal Quadoo Color and return its normal Win32 `COLORREF` (`0x00BBGGRR`).
2. For a mapped system-color value (e.g. `COLOR_WINDOW`, `COLOR_BTNFACE`, `COLOR_WINDOWTEXT` using the project's existing mapped-value encoding), resolve the proper system index via `GetSysColor()` at **call time**. Do not treat the encoded high bit or index as literal RGB.
3. Reuse the **actual** current Quadoo Color definitions, including any distinct mappings/sentinels. Check for invalid/unsupported codes and return the established error rather than guessing.
4. Expose this conversion to scripts so authors can deliberately resolve UI palette colors before invoking `DrawLine`, `DrawText`, etc.
5. Document that changing the Windows system color/theme subsequently does not retroactively recolor previously drawn pixels; a script can resolve and redraw after a relevant system color change if desired.
6. Do not silently auto-convert ordinary drawing-method parameters: their contract remains strict COLORREF. Require a zero high byte, rejecting system-color tags, CLR_NONE/CLR_INVALID, and palette flag values.
7. For ResolveColor, accept literal high-byte 0 or system tag exactly 0x80, with a recognized system index. Reuse ColorNames.h/SystemColorName() for the existing named-index set, including its invalid gaps; do not assume every number below the table length is valid. Preserve the existing resolver's ABI and behavior for its other callers.
8. Script conversion accepts an I4 bit pattern or an I8 in the supported 32-bit color range, then validates the tag. Do not reject a valid signed I4 representation of 0x800000xx or silently narrow a larger I8.

Example **illustrative only**, dependent on the current QuadooScript syntax and constants:

```javascript
var textColor = Canvas.ResolveColor(COLOR_WINDOWTEXT);
Canvas.DrawText("Hello from QuadooScript", 12, 12, textColor);
```

## 7. Font selection, text drawing, and required measurement

### 7.1 One selected font, shared with the inherited Font property

Use the existing QuadooProperty::Font descriptor and CControl/CWinControl font recreation path. There must be one selected Canvas font specification, not an independent drawing font that can disagree with the inspector.

- The current descriptor supports exactly Name, Size, Bold, and Italic. Size is integer points (currently 1..256), and Name must fit LF_FACESIZE. Underline and StrikeOut are deferred because the shared descriptor currently rejects those fields; do not offer six-argument SetFont and silently ignore the last two arguments.
- Native and script SetFont(faceName, pointSize, bold, italic) construct that existing descriptor value and set it through the normal property path. Setting Canvas.Font directly and editing Font in the designer have the same effect. ResetFont applies the descriptor's default value.
- RecreateFont uses the Canvas's current layout DPI. Follow the existing font fallback behavior on XP if a requested family is unavailable; do not require Segoe UI to be installed.
- Select the current HFONT only inside an operation's SaveDC/RestoreDC scope. Do not leave it selected into a persistent memory DC: the base Font setter may delete/replace it later. Restore selected objects before deleting a DC/bitmap/font, and never delete borrowed or stock GDI objects.
- Font changes affect subsequent drawing and measurement; they do not repaint text already rasterized into persistent pixels.

### 7.2 Required native and script contract

MeasureText is part of the first implementation, not an optional follow-up:

```cpp
STDMETHOD(MeasureText)(RSTRING text, SIZE* size) = 0;
```

```javascript
var size = Canvas.MeasureText("Hello from QuadooScript");
Canvas.DrawRectangle({Left:10, Top:10, Width:size.Width + 4,
    Height:size.Height + 4}, textColor, backColor);
Canvas.DrawText("Hello from QuadooScript", 12, 12, textColor);
```

The script example illustrates result usage; validate object-literal syntax against existing QuadooScript tests when implementing the adapter.

- Return integer Width/Height in **physical canvas pixels**, matching DrawText positions and PixelWidth/PixelHeight. Use Win32 SIZE on the native interface because QuadooForm::Size is documented as 96-DPI logical units.
- Measure with the exact current font and the same mapping/layout policy as DrawText. Do not scale the returned numbers a second time. A DPI or Font change can change the result.
- Measurement is independent of the canvas clipping rectangle, its visible size, scroll position, AutoRedraw, and DoubleBuffered. It works on a live hidden or zero-sized Canvas and outside OnPaint without allocating its full pixel bitmap.
- Use a compatible temporary or reusable measurement DC with MM_TEXT and the selected font. It must not depend on a retained paint HDC. Initialize output dimensions to zero on failure; a NULL size pointer is E_POINTER, wrong-thread calls return EWrongThread, and destroyed controls return EDestroyed.
- Measurement does not draw, invalidate, dispatch OnPaint, move an implicit cursor, change the font, or dirty the document. The script result is an independent snapshot; modifying it has no effect on the control.

### 7.3 Shared text layout policy

Implement one internal text-layout routine used by both MeasureText and DrawTextString. Measure full Unicode runs, not a sum of per-character widths. [GetTextExtentPoint32W](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-gettextextentpoint32w) uses the selected font but does not provide multiline line-height handling, so do not call it once on an entire multiline string and treat that as a block measurement.

Initial text behavior:

- Text is a counted UTF-16 RSTRING; use RStrLen(), not a runtime recount of the string. Preserve surrogate/combining sequences and rely on the same GDI font rendering/fallback in both paths. Native NULL RSTRING can follow the usual empty-string convention; a script argument must have the string variant type. Reject embedded NUL rather than silently truncating it.
- Support unwrapped multiline text. Normalize CRLF as one break and accept LF and standalone CR. Preserve empty lines and trailing line breaks. Empty text measures 0 by 0; nonempty text has one line plus its explicit breaks, including blank trailing lines.
- Width is the maximum measured line advance. Height is lineCount times the current font's tmHeight, with checked arithmetic. Do not add tmExternalLeading in this first version. Ordinary trailing spaces contribute to width.
- Tabs advance to stops every eight space-character widths from the start of each line. Measure and render the same runs at those positions; reject overflow. Ampersands are literal; there is no mnemonic processing, ellipsis, automatic word wrapping, rotation, or implied padding.
- DrawText's x/y denotes the top-left of the text layout block, not a baseline. Use transparent text background and clip only to the canvas. A shared per-line/run renderer using TextOutW/ExtTextOutW can implement this policy without separate DrawTextW formatting assumptions.
- The returned size is a layout/advance extent, not a promise that every italic or antialiased ink pixel lies inside that rectangle. Allow conservative glyph overhang in invalidation and visual comparisons. Text color and the pixels under the text do not affect measurement.
- Use the same selected font, character spacing, and justification defaults in both operations. Cache only if profiling warrants it; any cache must include the full font specification, actual DPI, text, and layout policy and must be invalidated on changes.

Wrapping, formatted text rectangles, underline/strikeout, and exact glyph-ink bounds can be added through a later optional interface, rather than changing this vtable's meaning.

## 8. Draw an indexed image from ImageList

Add a drawing method to place an image directly from a QuadooForm ImageList onto Canvas, using an **image index**:

`Canvas.DrawImageList(imageList, imageIndex, x, y)`

The selected entry is drawn at `(x, y)` at the ImageList's configured display size (adjusted for DPI according to the existing ImageList/TreeView infrastructure).

Requirements:

- Accept an existing ImageList **component object or interface** consistent with QuadooScript's component reference model, not an arbitrary unvalidated Win32 handle.
- Query the appropriate existing ImageList interface / native image-list capability. Do not change or duplicate its existing exported ABI merely to obtain `HIMAGELIST`.
- Validate image-list ownership/lifetime and index bounds. Error cleanly on null, destroyed, or incompatible image lists.
- Draw with the appropriate mask/per-pixel-alpha semantics, including `ILC_COLOR32`; prefer the current image-list draw helper / `ImageList_DrawEx` or `ImageList_DrawIndirect` as applicable.
- Call IQuadooImageList::Acquire(currentDPI), retain the returned IQuadooImageListSnapshot for the whole operation, then call GetHandle/GetSize. The HIMAGELIST is borrowed and immutable; never destroy or modify it. Validate index using ImageList_GetImageCount on that retained snapshot.
- Canvas stores only the rasterized result of each call, not a live ImageList connection. Acquire afresh for later draws; no persistent observer/subscription is needed. Existing pixels do not change when the ImageList is edited or destroyed.
- The script adapter unwraps the supplied component and QIs for IQuadooImageList. Allow any live, compatible object on the owning UI thread, including third-party factories; a same-form namespace restriction is unnecessary for this nonserialized argument. Do not cast to a built-in ImageList implementation.
- Third-party Acquire/GetHandle calls may reenter application code. Retain the Canvas and target surface, reject nested surface mutation, and recheck lifecycle/surface generation before drawing after such calls.
- Use a well-defined default rendering state (normal image) and document how disabled/selected rendering might be added later.
- Use the existing DPI policy for ImageList sizes; avoid double-scaling image content.
- Mark the changed Canvas region dirty and trigger presentation as needed, without automatically erasing the drawing.

Optional later extensions: custom destination size, image state, source clipping, opacity, and tiled drawing. **Only index-based drawing is required in this iteration.**

## 9. Export canvas as PNG binary data

Provide a method that reads the **current canvas pixels** and returns a **QuadooScript binary object containing a full PNG file**:

`binaryData = Canvas.ExportPng()`

Use the repository's existing **lodepng** implementation for encoding. Do not invent a second codec, serialize raw scan lines instead of PNG, or return a Base64 string when the script API expects a binary object.

Requirements:

1. Export the logical canvas surface at its actual current **physical pixel width and height** (document the DPI behavior explicitly).
2. Copy or convert pixel data from the DIB into the channel order and alpha representation expected by lodepng (commonly RGBA8). Preserve genuine alpha where supported.
3. Return a properly allocated QuadooScript binary object through the **existing** binary-value interface, with correct ownership/refcounts and cleanup.
4. Ensure the return value contains the exact byte sequence of a standard PNG file (PNG signature/chunks), suitable for saving, transmitting via HTTP, or further script processing.
5. Avoid exposing raw native bitmap pointers or leaking heap buffers to the caller.
6. AutoRedraw=False returns QuadooForm::EInvalidState. Do not capture the desktop or invoke an extra paint callback to manufacture export pixels. AutoRedraw=True exports the current backing pixels as of this call, without UpdateWindow or an implicit OnPaint; pending future paint work is not included.
7. Provide reliable error reporting for zero dimensions, allocation failures, and PNG encoding failures.
8. Keep UI-thread access, resizing, and destruction synchronized with export so the DIB cannot be destroyed while being read.
9. Include a round-trip automated test: draw known pixels and transparent ImageList inputs, export PNG, decode it with the existing QFI/lodepng wrapper, then compare expected dimensions, composited RGB, and opaque output alpha.

Concrete layering:

1. QuadooForm.dll converts the current opaque BGRA pixels into top-down RGBA8 in a CMemoryStream and calls the existing QFI::EncodePNG from Imaging/PngCodec.cpp. That wrapper already embeds lodepng, enforces limits, and maps allocation/codec errors; do not compile another lodepng copy.
2. Encode completely into an independently owned heap CMemoryStream before publishing success. ExportPng returns that stream through ISequentialStream** and its exact ULONG byte count, with a read position of zero. On failure return NULL/zero, leaving the Canvas unchanged. Native C++ clients can read standard PNG bytes without a VM.
3. The script adapter uses existing CLockableStream to wrap that stream and calls QVMCreateBinaryFile(RSTRING_CAST(L"image/png"), NULL, data, 0, byteCount, result). The existing factory is declared in Published/QuadooVM.h and returns a QVARIANT holding a binary object; it does not return IQuadooBinary. Its final range argument is an end offset. Validate byteCount <= INT_MAX before narrowing.
4. Retain the encoded stream through the binary object's ownership chain; never expose a pointer into a stack CMemoryStream or the mutable Canvas DIB. Each export is an independent snapshot that remains valid after later drawing, resize, or destruction.
5. Native rendering code must not include QuadooVM.h, query IQuadooObject, or link the compiler/VM DLLs. Binary wrapping lives only in ScriptAdapter.

### Example usage (pseudocode; validate syntax against current bindings)

```javascript
var back = Canvas.ResolveColor(COLOR_WINDOW);
var text = Canvas.ResolveColor(COLOR_WINDOWTEXT);

Canvas.Clear(back);
Canvas.SetFont("Segoe UI", 11, false, false);
var titleSize = Canvas.MeasureText("QuadooForm Canvas");
Canvas.DrawText("QuadooForm Canvas", 12, 12, text);
Canvas.DrawLine(12, 12 + titleSize.Height + 4, 180,
    12 + titleSize.Height + 4, 0x00FF8000); // COLORREF, NOT #RRGGBB
Canvas.DrawImageList(ImageList1, 3, 12, 52);

var pngBytes = Canvas.ExportPng();
// Hand pngBytes to the existing binary-file or HTTP API.
```

Note: a numeric RGB hex literal in `COLORREF` order is **0x00BBGGRR**, not web `#RRGGBB`. Update examples as appropriate for actual QuadooScript color-constant conventions.

## 10. Invalidation, events, thread affinity, and shutdown

- All Canvas HWND and GDI operations should run on the owning **UI thread** unless the framework already offers an explicitly safe dispatch model.
- Background work should use the existing `Form.Async()` or registered-wait callback mechanism to request drawing on the form's UI thread; do not draw directly from a thread-pool callback.
- A draw operation updates the backing pixels immediately, then invalidates only the affected region when practical (or the full canvas as a simple initial implementation).
- OnPaint is required. In Runtime mode, dispatch it once per outer paint **before presentation**, targeting the persistent bitmap when AutoRedraw=True or the current transient paint target otherwise. Persistent mode does not automatically clear existing drawing before the handler; transient mode does. Initially use ordinary sender/event arguments and read-only physical ClipLeft/ClipTop/ClipWidth/ClipHeight integers if clip information is exposed; never pass an HDC or DIB pointer to scripts.
- Drawing from OnPaint updates its current target without calling InvalidateRect/UpdateWindow. Nested WM_PAINT/WM_PRINTCLIENT must not redispatch the event. Invalidate() marks/coalesces a normal update region with InvalidateRect(..., FALSE) outside paint, without sending/posting WM_PAINT directly and returns S_FALSE when called during the active callback; applications should schedule the next frame through their existing Timer/Async path.
- The paint scope owns or retains its rendering resources and holds a strong self-reference while invoking delegates. A handler can close/destroy the control or pump nested messages. Recheck component state, HWND identity, and surface generation afterwards; skip presentation when destroyed/replaced. Destruction must not free a bitmap/DC still used by an outer paint scope.
- Balance BeginPaint/EndPaint and SaveDC/RestoreDC on every exit path. Record callback failures through the existing reporting mechanism, present only a valid target/background, and do not repeatedly invalidate merely because a handler failed.
- Handle WM_PRINTCLIENT using the same render/presentation helper and the caller's DC; do not call BeginPaint for it. Suppress callbacks in Design/Preview and guard nested print/paint dispatch. Do not attach another generic window subclass when the existing CWinControl::Message path suffices.
- Add Canvas OnResize only after pixel dimensions and the replacement/preserved surface state have been committed. Coalesce nested layout changes and suppress callbacks during construction, teardown, Design, and Preview. Keep normal inherited focus/keyboard events; define Canvas mouse-event coordinate units explicitly as physical pixels rather than assuming all inherited input metadata already exists.
- Handle `WM_SIZE`, `WM_PAINT`, `WM_ERASEBKGND`, `WM_DPICHANGED`/the existing DPI notification path, `WM_SYSCOLORCHANGE` or relevant theme changes, and component/window destruction appropriately.
- Window creation/destruction and modal preview must not leave stale HDCs, pending callbacks, fonts, brushes, bitmaps, or image-list references.
- Calls against a destroyed Canvas should return the framework's usual destroyed-object error.

## 11. Design-time persistence

Persist **Canvas configuration** (size, position, colors, font specifications if applicable, drawing flags, events) through the existing Form/Frame JSON schema.

The pixels drawn **at runtime** do **not** need to be serialized into `.qform.json` for this iteration. The script is responsible for drawing the initial contents (e.g. in `OnCreate`/`OnPaint` as appropriate). If an initial design-time background picture is desired later, treat that as a separate feature.

Designer mode should show the Canvas control and allow normal property changes without running scripts. Actual PNG export is a runtime capability.

## 12. Tests and acceptance criteria

Extend the current native, script-binding, designer, and integration harnesses to exercise:

1. Create, select, resize, serialize, reconstruct, and destroy a Canvas in a Form and Frame.
2. All drawing primitives, clipping, zero/negative/out-of-range coordinates, correct COLORREF byte ordering.
3. Quadoo Color literal and mapped system-color resolution via `ResolveColor()`, including invalid codes and theme/system color changes.
4. Font family/size/style selection, required MeasureText/native and script results, Unicode text, transparent text background, DPI changes, and GDI font cleanup.
5. `AutoRedraw=True`: drawings survive cover/uncover, resize (both ClearOnResize settings), minimize/restore, and invalidation.
6. `AutoRedraw=False`: clearly documented paint-time/transient behavior, without invalid HDC usage.
7. `DoubleBuffered` enabled/disabled; no unnecessary flickering or recursive redraw.
8. ImageList drawing for `ILC_COLOR32` alpha and traditional mask-based lists, different image indices, out-of-range indices, rebuilds, and destroyed source components.
9. ExportPng returns an actual QuadooScript binary object; verify PNG signature, dimensions, decoded pixels/alpha, and ownership across repeated exports.
10. PNG export after drawing text and ImageList icons.
11. Error paths: zero size, out-of-memory simulation when feasible, invalid images/font data, destroyed controls, and encoder errors.
12. No leaks of GDI objects or native image-list handles, including repeated create/resize/export/destroy cycles.
13. Build Win32 Debug and Release using existing VS2008 toolchains and run appropriate Windows XP tests. Report ReactOS testing separately where available.
14. Regression tests of the existing Image, Panel, ImageList, TreeView, Button, and Form controls.
15. MeasureText for empty strings, spaces/trailing spaces, literal ampersands, tabs, CR/LF/CRLF, empty/trailing lines, long strings/overflow, non-ASCII text, surrogate pairs, and font fallback. Verify measured layout against the shared drawing layout rather than hard-coded pixel values for a particular installed font.
16. Measurement before/after Font/SetFont/ResetFont, at 96/120/144/192 DPI, outside OnPaint, with both AutoRedraw modes, and on hidden/zero-size controls. Confirm no pixel changes, events, invalidation, or document dirtying; check native NULL out arguments and script arity/type errors.
17. Script MeasureText returns usable integer Width/Height and an independent snapshot. Native ExportPng and script binary bytes agree; retain/read the binary after Canvas destruction. Test wrong-thread and post-destruction calls on the new interface, and COM identity/AddRef/Release through IQuadooCanvas, IQuadooWinControl, and IQuadooComponent.
18. Destroy, resize, change mode/font, and pump nested paint/print messages from delegates. Exercise third-party ImageList callback reentrancy, allocation rollback, stale-surface rejection, zero-size restore, and balanced GDI object selection. Verify that normal draw calls in OnPaint do not schedule an endless repaint loop.
19. JSON persists configuration and event names only, with no pixels, HDC/HFONT handles, or measurement results. Verify single/multiple-selection inspector behavior and that Canvas has no container/child-host interface.
20. Include an ImageList test double implemented only against the published capability and snapshot interfaces. The Canvas/adapter must consume it without private-class inheritance or concrete-type tests.

### Acceptance scenario

A developer should be able to:

1. Place a Canvas and an ImageList onto a Form in Quadoo Studio.
2. Populate the ImageList using the existing editor.
3. Write QuadooScript that resolves a mapped system color, clears the Canvas, selects a font, measures arbitrary text with MeasureText, uses Width/Height to position drawing, writes the text, draws a few shapes, and places a specific indexed ImageList icon.
4. Run the application and observe correct, flicker-free painting, including after the window is covered and uncovered.
5. Call `ExportPng()` and receive a binary QuadooScript object holding the PNG representation of the Canvas, ready for writing to disk or sending through an HTTP request.

## 13. Deliverables and implementation order

Recommended order:

1. Inspect existing types, event and property infrastructure, ImageList native handle APIs, color conversion, DPI helpers, fonts, lodepng, and QuadooScript binary values.
2. Define and review new optional `IQuadooCanvas` interface and script-visible signatures **without changing existing ABIs**.
3. Implement Canvas control registration, designer representation, and JSON persistence.
4. Implement 32-bit DIB lifecycle, draw primitives, clear, invalidation, AutoRedraw, and double buffering.
5. Add checked Canvas color validation around the existing ResolveQuadooColor export and expose the operation to QuadooScript.
6. Implement shared font selection, text layout, text drawing, and required MeasureText native/script behavior, including DPI and multiline tests.
7. Implement index-based ImageList drawing with correct alpha and DPI.
8. Reuse QFI PNG encoding for a native stream snapshot, then wrap it in the existing QuadooScript binary object from the adapter.
9. Add tests, sample Application project, documentation, XP verification, and final regression builds.

Provide a concise implementation report containing: changed files; public interface additions; script-exposed methods and exact signatures; retained/deferred properties; tests performed; build outcomes; and any limitations or follow-up work.

**Do not expand this request into a full PictureBox container, an image editor, or a replacement of the current Image control.** The focus is a useful, stable drawing surface with ImageList rendering, system-color resolution, font selection, and PNG binary export.


## 14. Repository implementation map

Paths below are relative to platform/QuadooForm unless a different root is stated. These are inspected reuse points, not names of new facilities to invent.

| Concern | Existing implementation to reuse |
|---|---|
| Concrete native window wrapper and COM map | QuadooForm/ProgressBar.h, Runtime.h, and Windows.cpp: TWindowInterface<CWinControl, IQuadooWinControl>, BEGIN_UNK_MAP, existing Message/NativeCreated/NativeDestroyed hooks |
| Per-type registration and descriptors | QuadooForm/Registry.cpp and Metadata.cpp: a Canvas TypeSpec based on NativeWindowType(), plus its own property/event definitions and factory |
| Font value validation and DPI recreation | QuadooForm/Metadata.cpp and Windows.cpp: inherited Font value and RecreateFont; do not maintain a competing font store |
| DIB/DC lifetime and transactional allocation | Imaging/ImageSurface.h/.cpp: QFI::CImageSurface::Create/Clear/Swap |
| PNG encoding/decoding and bounded allocations | Imaging/ImageResource.h and PngCodec.cpp: QFI::EncodePNG/DecodePNG, MaxDimension/MaxPixels/MaxBytes, and existing fault injection |
| Color resolution and named valid system indexes | Published/QuadooForm.h, QuadooForm/QuadooForm.cpp, and ColorNames.h |
| Native ImageList lifetime contract | Published/QuadooForm.h: IQuadooImageList::Acquire and IQuadooImageListSnapshot::GetHandle/GetSize; StatusImages.cpp shows snapshot consumption |
| Script dispatch, unwrapping, JSON-shaped results | ScriptAdapter/RuntimeObjects.cpp, TreeObjects.cpp, and ListViewObjects.cpp; QVMConvertFromJSON already wraps JSON values |
| Binary factory and stream ownership | platform/QuadooScript/Published/QuadooVM.h: QVMCreateBinaryFile; shared/library/core/MemoryStream.h and LockableStream.h/.cpp. AQVMT/ApplicationTests already compile LockableStream.cpp |
| Palette artwork/resources | Icons/GenerateIcons.py, QuadooForm/PaletteResources.h, QuadooForm/QuadooForm.rc, and the shared Designer palette catalog |
| Shared editing and Studio embedding | Designer/Inspector.cpp, Selection.cpp, palette/outline infrastructure, and desktop/QuadooStudio integration tests |

Suggested source organization, subject to ordinary implementation judgment:

- QuadooForm/Canvas.h/.cpp for the concrete control, metadata, factory, state, and native interface.
- A small CanvasDrawing/CanvasPainting split if necessary to keep the drawing/text layout and paint lifecycle reviewable. Keep rendering helpers independent of HWND creation where possible so offscreen tests can exercise them.
- ScriptAdapter/CanvasBindings.h/.cpp for native capability dispatch, argument conversion, text-size objects, and PNG binary wrapping. A helper like the existing list bindings avoids adding a large drawing switch directly into RuntimeObjects.cpp.
- Harness/CanvasTests.h, offscreen coverage in TestInternals, DesignerTests/CanvasTests.h, and Studio/ApplicationTests additions.

Publish only the new IQuadooCanvas UUID, any necessary Canvas enums, and new descriptors. Keep native C++ interface identity consistent through all QI paths; use BEGIN_UNK_MAP and the existing adapters. Do not add a Kind/type-name filter, make the base CControl inherit IQuadooCanvas, or require third-party capabilities to inherit a private class.

Canvas must not implement IQuadooControlContainer or IQuadooChildWindowHost. Factory/type-name discovery, palette registration, normal layout/selection, inspector editing, event binding, and source-member generation should work through the existing generic paths. Persist no renderer state in global tables.

## 15. Script boundary and lifecycle checklist for implementation

- Preserve m_srNativeHandler forwarding precedence for external components. Built-in Canvas remains VM-independent; its adapter queries IQuadooCanvas before handling Canvas-specific methods. No private CCanvas pointer is required.
- Reuse the reverse argument ordering used in TreeObjects.cpp: source argument i is pqvArgs[cArgs - i - 1]. Verify asymmetric drawing coordinates in tests so an accidentally reversed argument list cannot pass.
- Validate arity and exact string/Boolean/object types. Use VariantInteger for integer arguments and check the full I8 value before narrowing coordinates, indexes, flags, or colors; report DISP_E_BADPARAMCOUNT, DISP_E_TYPEMISMATCH, or DISP_E_OVERFLOW as appropriate. Initialize result variants and release partially built results on failure.
- Create the MeasureText result using the existing JSON-object-to-QVARIANT path, with Width/Height integer fields. It is not a registered component, property collection, or generated script class member.
- Use a per-instance paint-depth/draw-depth guard and a surface generation identifier. Keep the active paint target private and valid only within its owning scope. Reentrant measurement is read-only; reentrant mutation or surface replacement during an in-flight draw returns EInvalidState. Internal resize/DPI changes can record pending work and apply it after the scope releases its target.
- Drawing during OnCreate is allowed in persistent mode once the native control and font are initialized. Application callbacks must remain suppressed in Design/Preview through the normal construction-context rules; do not add a second script-binding mechanism for OnPaint.
- Clear active targets before native teardown. A retained native/script Canvas reference reports EDestroyed after destruction, while already returned measurement objects and PNG binaries remain valid snapshots. Remove any callbacks/observers actually registered and release fonts, DIBs, DCs, and owned streams without leaving selected objects behind.
- Changing Font, BackColor, drawing flags, or event names is a normal descriptor-backed property change. Drawing pixels, measuring text, and exporting PNG are runtime operations and must not generate property-change notifications or mark Studio's form document dirty.
- For input events, reuse inherited OnEnter/OnLeave, focus, keyboard, and PopupMenu behavior. If adding OnMouseDown/OnMouseUp/OnMouseMove in the initial control, define their argument descriptors explicitly, use physical client coordinates (including negative captured coordinates), and reuse Win32 button/key masks. Do not silently claim mouse-event descriptors already exist on every CWinControl.

## 16. Build and validation handoff

Implementation must update both QuadooForm/QuadooForm.vcproj and tests/platform/QuadooForm/TestInternals/QuadooFormNativeTests.vcproj for native sources. New script binding sources must be included in platform/QuadooScript/AQVMT/AQVMT.vcproj and tests/desktop/QuadooStudio/ApplicationTests/QuadooApplicationTests.vcproj. Include the 20x20 Canvas PNG and XP-compatible ICO as normal palette resources and update palette tests that count factories/images. The shared Designer/Studio integration should not need a separate Canvas property editor or resource DLL.

Capture current suite baselines before implementing, and distinguish unrelated existing failures from Canvas failures. Add focused selectors following the existing StatusBar/ImageButton pattern; the following focused selectors are implemented:

```powershell
# Offscreen rendering/text/font/PNG tests; no HWND creation or message loop.
tests\results\bin\Debug\QuadooFormNativeTests.exe --canvas-rendering-only
# Native HWND, layout, events, paint/print reentrancy, and teardown.
tests\results\bin\Debug\QuadooFormHarness.exe --canvas-only --trace
# Shared Designer and Studio palette/properties/source/JSON integration.
tests\results\bin\Debug\QuadooFormDesignerTests.exe --canvas-only --trace
tests\results\bin\Debug\QuadooStudioIntegrationTests.exe --canvas-only --trace
```

Repeat in Release. Add real QuadooScript coverage through ApplicationTests using the existing fixture/resource loader, including MeasureText's returned object and PNG binary ownership; do not validate only C++ adapter calls. Provide a bounded test-canvas.ps1 runner with a headless/offscreen-only option, and inspect each test entry point before classifying it as safe on a locked workstation. Offscreen GDI/font tests can require the Windows graphics subsystem even though they create no windows; document that distinction.

The existing build-application-lifetime.cmd accepts Debug/Release and Win32 and builds affected projects individually. Reuse existing SimbeyCore.dll, JSON.dll, and compiler/VM dependency DLLs; do not rebuild those dependencies. Run appropriate existing Image, ImageList, TreeView, Button, and Form regressions after the focused checks pass, without repeatedly rerunning unchanged suites.

Windows XP acceptance must include the actual native control, font fallback, repeated font replacement, masked/alpha ImageList composition, repaint/resize, and decoded PNG output. Modern Windows tests and a 5.0 PE subsystem header alone do not demonstrate XP VM execution. Record ReactOS results separately, or state that environment was unavailable.

Remaining implementation-time decisions are the unused UUID/resource IDs, final private file split, and any additional mouse-event scope. Required MeasureText semantics, font authority, coordinate units, PNG layering, and the behavior of the two painting modes are defined above. Any material deviation should be reported before changing the published contract.
