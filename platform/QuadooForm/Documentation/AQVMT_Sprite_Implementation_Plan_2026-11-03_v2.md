# QuadooForm Canvas — AQVMT Sprite Extension

## Purpose and source of truth

Implement an **AQVMT/script-adapter-only Sprite object** associated with an existing QuadooForm Canvas. The authoring syntax must be:

```javascript
property var Drawing;

var oSprite = Drawing.CreateSprite();
oSprite.LoadFile("C:\\Images\\Player.png");
oSprite.Draw(100, 50);
oSprite(100, 50);   // exact semantic equivalent; optimized default invocation
```

Use the current repository implementation as the authority. In particular, review `CanvasImplementation.md` and the actual `QuadooForm/CanvasDrawing.h/.cpp`, `QuadooForm/Canvas.h`, `ScriptAdapter/CanvasBindings.h/.cpp`, `ScriptAdapter/RuntimeObjects.cpp`, the existing QuadooScript object invocation/binary APIs, and QFI imaging code. This proposal extends the **implemented** Canvas; do not replace it with a new drawing framework.

### Current implementation contracts to preserve

- QuadooForm.dll registers an HWND-backed, non-container Canvas with an optional published `IQuadooCanvas` capability. It has **no dependency on QuadooScript**.
- `CCanvasSurface` owns a `QFI::CImageSurface` whose drawing resources form a top-down BGRA DIB. The existing published `IQuadooCanvas` interface now has `GetSurface()`, returning handles and a borrowed `SIF_SURFACE*`.
- `AutoRedraw=True` retains opaque pixels; with `AutoRedraw=False`, mutating drawing is permitted **only within this Canvas's OnPaint**. Drawing coordinates are **physical client pixels**, not 96-DPI layout units.
- Canvas drawing, rendering, and surface access are **UI-thread-affine**. Preserve existing paint/resize/destruction reentrancy guards and the `Form.Async` / `AsyncWait` marshalling model.
- Existing imaging limits include 4096 pixels per dimension and 4,194,304 total pixels; PNG export has an 8 MiB encoded limit. Check whether sprite decode limits should reuse these bounds or separately constrain aggregate sprite memory.
- Existing Canvas exports are opaque RGBA PNG snapshots; **sprite sources may nevertheless carry partial or full alpha**. Compositing must keep destination Canvas alpha opaque.
- Use the **newly updated** `IQuadooCanvas::GetSurface()` declaration as the source of truth. Do not add another interface or change unrelated Canvas methods, descriptors, or JSON. Because this new method changes the interface vtable, check that all consumers are rebuilt consistently (and update interface version/UUID conventions as appropriate to the repository); do not claim the previous interface ABI remains unchanged. No Sprite becomes a serialized form component.

## 1. Ownership and architectural separation

The Sprite is an **AQVMT/script-adapter-only object**, implemented in a dedicated class (e.g. `CSpriteObject`). Do not add sprite behavior to `CComponentObject::Invoke()`. The existing component adapter should recognize `Canvas.CreateSprite()` and construct the specialized object, or delegate construction to a focused factory/binding helper.

An individual Sprite:

1. Is created by a specific Canvas (`Drawing.CreateSprite()`).
2. Holds a **strong `IQuadooCanvas*` reference**, obtained by querying the Canvas's existing published capability, using normal COM-style `AddRef`/`Release`. **Do not retain or query `CCanvasSurface` or an unrelated surface `IUnknown`** as the Sprite's owner.
3. Holds its **own** decoded source-image buffer, dimensions, and optional source metadata.
4. Is never registered with QuadooForm.dll, the nonvisual component tray, form JSON, or any Canvas component collection.
5. Is **not** retained by Canvas: ownership is one-way `CSpriteObject -> IQuadooCanvas -> native Canvas implementation`, without a Sprite/Canvas cycle.
6. Releases its decoded image and `IQuadooCanvas` reference when destroyed.

Retaining `IQuadooCanvas` guarantees the lifetime of the interface implementation, **not** the continued existence of the control HWND, the current DIB, or unchanged surface pointers. A Sprite must call `GetSurface()` afresh for every `Draw()`; it must not cache a returned `SIF_SURFACE*`, `BYTE*`, `HDC`, or `HBITMAP` across draw calls. If the visual Canvas has been destroyed or its drawing target is unavailable, drawing must fail safely through the existing adapter error conventions.

## 2. Use the existing IQuadooCanvas::GetSurface() contract and shared SIF surface header

The user has **already added** this method to `IQuadooCanvas`:

```cpp
STDMETHOD(GetSurface)(
    __deref_out HDC* phdcSurface,
    __deref_out HBITMAP* phDIB,
    __deref_out SIF_SURFACE** ppsifSurface);
```

**This is the intended access path.** Do not propose a new `IQuadooCanvasSurfaceAccess` interface, a custom `QFI_IMAGE_SURFACE_DATA` POD, an `IUnknown`-to-`CCanvasSurface` cast, or a duplicate data structure. `SIF_SURFACE` is supplied by the **SIF project as a shared header-only definition**; include the authoritative header where necessary. **No SIF library linkage or added dependency is required for this iteration.** Check the actual fields, pixel organization, row stride, ownership, and layout in that header rather than guessing their names or binary format.

Implementation tasks:

1. Verify the existing `IQuadooCanvas::GetSurface()` declaration and find or implement its corresponding native Canvas implementation. It must return the **current** Canvas drawing surface and an appropriate HRESULT, using the established Canvas thread/lifecycle checks. The Sprite calls it through its retained `IQuadooCanvas*`.
2. Treat all three outputs as **borrowed** values. The Sprite does not release/delete the returned `HDC`, `HBITMAP`, or `SIF_SURFACE*` and does not own the pixels inside the structure. The returned image data remains owned by QuadooForm's Canvas implementation.
3. Check whether the Canvas's existing backing surface is actually available for direct writable pixel access (e.g. `AutoRedraw=False` outside a paint callback, zero-size allocations, or a disconnected/destroyed HWND). Return failure or follow the existing Canvas rules; never return stale handles or a dangling structure.
4. Define and enforce the **validity window** of each returned pointer. Because `GetSurface()` as declared does not return a lock, scope token, or paired release function, the Sprite must **not assume it pins the DIB** against resize, mode switches, destruction, or reentrant callbacks. Make the direct drawing operation synchronous on the Canvas UI thread; avoid invoking script callbacks or anything that pumps messages between `GetSurface()` and the last pixel write. Review the existing Canvas drawing guards and, if necessary, extend them internally to protect the duration of a native Sprite drawing call without changing the supplied interface signature. If a safe validity guarantee cannot be achieved using this method alone, flag that exact limitation to the user rather than silently treating the borrowed pointer as permanent.
5. Validate all out-parameters and HRESULTs. `GetSurface()` must initialize outputs to null on failure, and the adapter must reject null/invalid returned handles or pixel pointers before using them.
6. Preserve coherence between CPU pixel writes and GDI drawing (`GdiFlush`, alpha channel repair, and repaint/invalidation policy) using the existing Canvas operations. **The `GetSurface()` call does not itself necessarily notify Canvas that pixels changed.** Reuse `IQuadooCanvas::Invalidate()` or the established Canvas dirty/presentation path after the draw, without triggering nested paint.
7. The Sprite never exposes the `SIF_SURFACE*` or other native pointers to QuadooScript code. This is an internal C++ bridge between the scripting adapter and QuadooForm.

### Minimal usage sketch (illustrative; use actual SIF_SURFACE members)

```cpp
// m_pCanvas is retained by CSpriteObject with AddRef.
HDC hdcSurface = NULL;
HBITMAP hDIB = NULL;
SIF_SURFACE* pSurface = NULL;
HRESULT hr = m_pCanvas->GetSurface(&hdcSurface, &hDIB, &pSurface);
if(FAILED(hr))
    return hr;
if(NULL == hdcSurface || NULL == hDIB || NULL == pSurface)
    return E_UNEXPECTED;

// Validate the current surface dimensions/format and clip.
// Composite onto borrowed SIF_SURFACE pixels synchronously.
// Do not keep hdcSurface, hDIB, pSurface, or its pixel pointer.
// Notify/present changes through the established Canvas path.
```

This plan **does not require** changing `CImageSurface` inheritance or copying its original members into a newly published POD. Only use its implementation internals as necessary to make the preexisting `GetSurface()` return the SIF-defined view safely.

## 3. Sprite script API

Keep the public API small and convenient. The following names are proposed; adjust exact binary-object argument conventions to existing QuadooScript APIs rather than creating an incompatible binary type.

| Method/property | Script behavior |
|---|---|
| `Drawing.CreateSprite()` | Returns a new Sprite bound to `Drawing`; initially has no image |
| `sprite.LoadFile(path)` | Loads a PNG, BMP or ICO source image from disk; replaces prior sprite image atomically |
| `sprite.LoadPng(binary)` | Decodes PNG bytes supplied through an existing QuadooScript binary object |
| `sprite.LoadPixels(width, height, binary[, format[, stride]])` | Loads raw pixel bytes from an existing binary object; see formats below |
| `sprite.Draw(x, y)` | Alpha-composites the source image at top-left `(x,y)` on its bound Canvas |
| `sprite(x, y)` | **Exactly equivalent to `sprite.Draw(x,y)`**, dispatched through the optimized *default invocation* route |
| `sprite.Width`, `sprite.Height` | Read-only pixel dimensions of loaded sprite; zero when empty |
| `sprite.IsLoaded` | Read-only Boolean |
| `sprite.Clear()` | Releases the current sprite image, retaining the Canvas association |

The user specifically requests **loading both from files and memory**, including PNG file bytes and raw RGB bytes. `LoadPng` and `LoadPixels` are the minimum memory pathways. Reuse the existing image loading/ICO selection logic as appropriate, and avoid introducing new image codec dependencies. A generic `LoadImage(binary, format)` overload is acceptable *in addition* if it fits the current API better, but keep the explicit raw-pixel path and document its layout.

### Raw pixel specification

Prefer explicitly named, unambiguous formats:

- `RGB24`: byte triplets **R,G,B** in left-to-right, top-to-bottom row order. Alpha defaults to 255.
- `RGBA32`: byte quadruplets **R,G,B,A** in the same order; preserve 8-bit alpha.
- Optionally `BGRA32` for callers working with Win32 DIB data, provided the name makes byte order explicit.

Use **top-down** row order. When omitted, `stride` is the tightly packed row size (`width * bytesPerPixel`). If explicit positive stride is supported, it must be at least the packed row size; define and validate any row padding. Reject negative stride in the first iteration unless bottom-up source support is explicitly designed. Check overflow on width, height, row-byte size, `stride * height`, and input-buffer length before allocation or read. Do not accept a pointer into a temporary script binary after the loader returns: make an owned decoded copy.

Consider supporting `LoadImageList(images,index)` later, but it is **not required** for this iteration; the existing Canvas already has `DrawImageList`.

### Error behavior

- Loading an invalid file, truncated PNG, unsupported ICO, inconsistent pixel count, excessive dimensions, or allocation failure returns the normal QuadooScript error through the existing adapter conventions.
- A failed load leaves the **previous valid image intact**; do not discard old sprite data until replacement succeeds.
- `Draw` on an empty Sprite should report a clear invalid-state error (rather than silently do nothing), unless the current script API has a stronger established convention.
- Negative X/Y are allowed for partial clipping; reject integer overflow and coordinates outside Canvas's existing +/-1,000,000 drawing bounds.
- Drawing entirely outside the Canvas should be a successful no-op, not an out-of-bounds memory access.

## 4. Default invocation must be the fast path

The language syntax:

```javascript
oSprite(100, 50);
```

must call the same native drawing implementation as:

```javascript
oSprite.Draw(100, 50);
```

Implement the default-call route in the Sprite object's dedicated native dispatch implementation (the exact `IQuadooObject`/default-member hook should be discovered from the VM), **not** by rewriting it into a scripted property lookup for `"Draw"`, performing a string comparison, or constructing temporary method-name objects on every draw.

Desired hot path after parameter validation:

`AQVMT default object invocation -> CSpriteObject::InvokeDefault/fast method ID -> shared SpriteDraw() -> `IQuadooCanvas::GetSurface()` -> clipped alpha blend -> return`

The explicit `Draw` member can resolve to that same native helper. Preserve the VM's normal argument evaluation, error reporting, and reference semantics. Fast path means **low dispatch overhead**, not bypassing necessary argument/bounds checks or UI-thread safeguards.

Inspect how the VM dispatches callable objects and indexed/default methods; if callable `IQuadooObject` instances require a small language-runtime modification, isolate it and test existing default invocation behavior for regressions.

## 5. Drawing algorithm, transparency, and speed

- Source sprite image uses a decoded **top-down 32-bit buffer** capable of storing transparency. Be explicit whether this internal source is straight-alpha RGBA, straight-alpha BGRA, or premultiplied BGRA; never mix conventions implicitly.
- Destination Canvas is currently an **opaque BGRA DIB**. Every composited destination pixel must remain alpha 255. For each source pixel with alpha `a`, composite RGB with `src*a + dst*(255-a)` using a correctly rounded, bounded integer calculation (or an equivalent correct GDI alpha-blending operation).
- Source alpha 0 leaves the destination unchanged; alpha 255 overwrites its RGB; intermediate alpha blends. Do not use color-key transparency unless separately introduced as an explicit option.
- Clip the source against the destination rect before touching pixels. Respect partial offscreen drawing and existing physical-pixel coordinate conventions.
- Preserve image content on Canvas repaints when AutoRedraw=True; with AutoRedraw=False, allow sprite drawing only in the Canvas's OnPaint scope, consistent with `IQuadooCanvas` drawing rules.
- Call `IQuadooCanvas::GetSurface()` for every draw and use the borrowed `SIF_SURFACE` only synchronously for that draw, with the Canvas lifetime/reentrancy rules respected. Changes must be presented/invalidate efficiently; no per-pixel `InvalidateRect` or reentrant OnPaint.
- GDI writes to the DIB and direct CPU writes must be coherently ordered. Use existing `GdiFlush`/alpha-repair patterns before or after direct pixel access as required.
- When available, prefer a clipped, optimized native memory loop for common 32-bit source/destination cases (avoiding GDI per-pixel calls). Measure against a realistic many-sprite test. Only consider `AlphaBlend` where it is both faster and reliably correct on XP/classic controls.
- Sprites are drawn at their intrinsic pixel dimensions in the first iteration: **no scaling, rotation, transforms, animation engine, z-order manager, automatic repaint scheduling, or collision engine**. These can be added later without changing sprite ownership.

A Sprite's `Draw()` is an immediate operation that mutates the Canvas's current target. It is **not** a retained scene object: Canvas does not remember Sprite placements or redraw them automatically after `Clear()` or when `AutoRedraw=False`. The script must redraw in OnPaint when in transient mode.

## 6. Concurrency and lifecycle

- All Canvas-mutating Sprite operations must execute on the owning **UI thread**. Loading images may also remain UI-thread-affine in this iteration to keep the interface simple. To load remotely, fetch/decode outside the UI thread if appropriate and then marshal the update/draw with `Form.Async` or `AsyncWait`.
- A Sprite can outlive its script-visible Canvas component wrapper, but must not dereference a destroyed HWND or disconnected drawing target. Its retained `IQuadooCanvas` reference preserves the interface object lifetime, not a still-live drawing surface.
- Repeated create/load/draw/clear/destroy cycles must not leak COM references, GDI handles, HBITMAPs, decoded memory, or hidden script roots.
- Do not have the Canvas retain lists of Sprite objects or invoke methods on them; this avoids cycles and keeps QuadooForm.dll completely unaware of Sprites.
- Existing Form destruction, `AutoRedraw` changes, DPI changes, and nested drawing/OnPaint must be tested with live Sprites; guard and fail safely where appropriate.
- Avoid any surface memory write while its active DIB is being destroyed/reallocated. Every `GetSurface()` output is borrowed and must be used only during the protected synchronous draw, then discarded.

## 7. Integration boundaries

**Native QuadooForm.dll:** Implement/verify the already-added `IQuadooCanvas::GetSurface()` against the current Canvas backing surface and lifecycle protections; include the SIF header-only structure. Do not introduce another surface-access interface, a new POD layout, Sprite type knowledge, or Sprite registration.

**AQVMT / script adapter:** Implement `CSpriteObject`, Sprite member binding, default object call dispatch, `Canvas.CreateSprite()` creation, loader entry points, and script-visible errors/properties. Use a **dedicated class** for Sprite invocation, not a growing switch inside `CComponentObject::Invoke()`.

**Imaging/QFI:** Reuse lodepng, existing PNG/BMP/ICO decoding, the SIF header definition, and Canvas surface utilities. Keep source image memory owned by Sprite and destination backing memory owned by Canvas. No SIF library linkage.

**Designer/Studio:** No new Toolbox component, property descriptor, JSON member, or editor. Sprites are dynamic script objects. Add a runnable example using a designer-created Canvas and runtime Sprite instances; they should not appear as child components in the object tree.

## 8. Demonstration application

Add a focused Canvas/Sprite example to the existing `DesignerExamples/CanvasApplication` or a separate small Application fixture, in a way consistent with the repository's project format.

Illustrative script (verify file API, handler names, and language syntax against the actual VM):

```javascript
property var Drawing;
property var Player;

function OnCreate()
{
    Player = Drawing.CreateSprite();
    Player.LoadFile("player.png");

    Drawing.Clear(Drawing.ResolveColor(0x80000005)); // Example: tagged COLOR_WINDOW
    Player.Draw(20, 20);
    Player(80, 20);       // Same image using default invocation
}
```

Include a **memory-only** path that feeds a small known PNG binary to `LoadPng`, and a raw `RGB24` or `RGBA32` buffer to `LoadPixels`, without reliance on optional third-party services. Add a sample with partial transparency and a sprite clipped at a negative coordinate. Verify final Canvas pixels by `ExportPng()` and decode the PNG with the existing test helper. Where necessary, call `Drawing.Invalidate()` according to existing Canvas rules.

## 9. Tests and acceptance criteria

Extend existing focused Canvas tests with native and actual AQVMT script tests in both Win32 Debug and Release:

1. `Drawing.CreateSprite()` returns a distinct callable Sprite object bound to the correct Canvas.
2. `oSprite.Draw(x,y)` and `oSprite(x,y)` produce byte-identical results for the same initial Canvas pixels.
3. Benchmark default invocation versus explicit member invocation and confirm the former avoids method-name lookup/string allocation in its native dispatch path; report measurements, but do not require an arbitrary speed multiplier.
4. Loading PNG from file and from binary yields identical decoded sprite pixels.
5. Loading a BMP and ICO preserves expected colors and transparency, including alpha-bearing ICO entries.
6. Loading top-down RGB24 and RGBA32 binary buffers produces correct channel ordering, width, height, and alpha.
7. Source alpha 0, 1, 127, 254, and 255 blend correctly into multiple opaque destination colors; no fringe/halos; output destination alpha remains 255.
8. Partial clipping on every boundary and completely offscreen coordinates; no out-of-range reads or writes.
9. An invalid replacement image leaves the previous sprite usable.
10. Repeated draws and sprite creation/destruction do not leak memory, objects, or GDI resources.
11. Canvas resize/DPI changes and AutoRedraw transitions with a surviving Sprite do not leave stale `GetSurface()` outputs or pixel pointers; each draw obtains the new current surface.
12. Canvas Form/control destruction with live Sprite produces safe failure and eventual cleanup without reference cycles.
13. `AutoRedraw=True` persistence and `AutoRedraw=False` OnPaint-only behavior match existing Canvas semantics.
14. Calls from incorrect threads fail/marshal according to existing rules; callbacks never run accidentally on the worker thread.
15. Existing Canvas/ImageList/TreeView/ListView tests remain passing after the known `IQuadooCanvas::GetSurface()` interface addition; all implementers and callers use a consistent header/interface version.
16. Verify exported PNG output pixels after sequential Canvas and Sprite drawing operations.
17. Retain or isolate the previously documented unrelated StatusBar `Panels` test failure; don't silently attribute it to Sprites or claim the broader suite passes if it does not.

Use existing build runners (for example `tests\platform\QuadooForm\build-application-lifetime.cmd` and `tests\platform\QuadooForm\test-canvas.ps1`) and integrate new focused tests, avoiding unnecessary dependency rebuilds. Build both **VS2008 Win32 Debug and Release**. Test on XP/ReactOS when available; explicitly disclose if VM execution remains unverified.

## 10. Completion report

At completion, report:

- Script API and examples actually implemented, including exact raw pixel format, stride and byte-count rules.
- The concrete Sprite class and default-invocation routing; whether any VM opcode/interface changes were required.
- The SIF header and exact `IQuadooCanvas::GetSurface()` implementation, with its borrowed-pointer lifetime rules and no additional interface or POD.
- Ownership diagram (`Sprite -> IQuadooCanvas -> native Canvas implementation`; no reverse reference), disconnection behavior, and how each `GetSurface()` result is protected during a draw.
- Chosen compositing format and pixel-alpha rounding details; any performance measurements.
- Files changed and interface/ABI implications of the user-provided `GetSurface()` addition; no further surface-access interface additions.
- Test results, failed preexisting tests, and XP/ReactOS validation status.

**Acceptance milestone:** A QuadooScript application creates a Sprite through a designer-created Canvas, loads an alpha PNG from disk or binary (and raw RGB data from memory), draws it using either `sprite.Draw(x,y)` or `sprite(x,y)`, survives repaint/resize/lifecycle edge cases, and exports the expected composited Canvas image as PNG—all while Sprites retain `IQuadooCanvas`, retrieve borrowed `SIF_SURFACE` data through `GetSurface()` for every draw, and QuadooForm.dll remains unaware of AQVMT Sprite objects.
