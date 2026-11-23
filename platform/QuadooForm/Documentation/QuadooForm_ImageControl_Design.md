# QuadooForm Image Control — Design and Implementation Proposal

**Status:** Implementation specification for Codex  
**Target:** QuadooForm native runtime and standalone/embedded visual designer  
**Compatibility:** Visual Studio 2008 (VC9), Win32, Windows XP and later; retain existing QuadooForm ABI wherever practical  
**Scope:** New `Image` control, reusable image resource/painting support, serialization, property editing, and tests

## 1. Purpose and guiding principles

Implement a versatile `Image` control that is more capable than Win32 `STATIC` (`SS_BITMAP`/`SS_ICON`) and the traditional VCL `TImage`. It must be a **windowless painted control**, not a standalone HWND, and must participate in the existing QuadooForm component model: factories, descriptors, logical bounds, visibility, design/preview/runtime modes, hit testing, events, DPI-aware layout, lifecycle, and JSON serialization.

The design deliberately separates:

1. **Source image resource:** preserved ICO container or normalized PNG data.
2. **Size mode:** how the image is scaled to the control rectangle.
3. **Alignment:** which portion of the source is positioned or retained when not filling exactly.
4. **Interpolation:** quality and algorithm used to resize it.
5. **Compositing:** per-pixel alpha, optional global opacity, and transparent/opaque control background.

Do **not** introduce `ColorKeyEnabled`, `ColorKey`, `ColorKeyTolerance`, or any equivalent color-key feature in this task. Do not replace source transparency with a magic-color convention.

## 2. Required public interface and control hierarchy

The new control must expose **`IQuadooStaticControl`**, inheriting from `IQuadooControl`, with the existing declaration **unchanged**:

```cpp
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0003")) IQuadooStaticControl : IQuadooControl
{
    // RECT is in physical device pixels for the current paint operation.
    STDMETHOD(Paint)(HDC hdc, __in const RECT* prcPhysicalBounds) = 0;
    STDMETHOD(HitTest)(INT xLogical, INT yLogical, __out BOOL* pfHit) = 0;
};
```

Implement a separate concrete class (e.g., `QF::CImage`) using the framework's current windowless/static-control implementation conventions. Register its factory under **`"Image"`** and expose `IQuadooStaticControl` through `QueryInterface`. It must **not** expose `IQuadooWinControl`, create an image HWND, call `SetParent`, or rely on the Win32 static-image styles for painting.

* `Paint` receives a destination `HDC` and **physical device-pixel** rectangle; do not reinterpret that rectangle as 96-DPI logical coordinates. The control's stored geometry remains in 96-DPI logical units.
* `HitTest` receives **logical** coordinates, consistent with the published interface. Initially, implement rectangular bounds hit testing; pixel-alpha-sensitive hit testing may be added later.
* Use the existing parent-painted child traversal, invalidation, event dispatch, and native-descriptor mechanisms. Do not introduce a `Kind` enum, hard-coded global control-property tables, or concrete-class casts required for generic layout/serialization.
* Retain existing interfaces, IIDs, vtables, exports, and their ordinals. Any optional new public interface must be clearly justified and additive; property-descriptor discoverability is preferred for ordinary image configuration.

## 3. Image loading, representation, and ownership

### 3.1 Supported input formats

The designer's Picture editor must accept:

| File input | Runtime/persisted resource representation |
|---|---|
| `.ico` | Original validated ICO container, **not flattened** to a single image |
| `.png` | PNG (may normalize to a canonical PNG representation) |
| `.jpg`, `.jpeg` | Decode to pixels, then re-encode as PNG |
| `.bmp` | Decode to pixels, then re-encode as PNG |

The designer may use existing repository loaders or a suitable Windows imaging API compatible with the approved minimum OS. **lodepng is a PNG encoder/decoder, not a JPEG/BMP decoder.** Inspect repository image-loading facilities and reuse them. Avoid adding an unapproved newer-Windows dependency; GDI+ may be suitable for JPEG/BMP loading on XP if existing repository code does not already solve it.

Imported JPEG orientation should be normalized using EXIF orientation **if** the selected decoder exposes it reliably; do not promise preservation of JPEG metadata. Conversion to PNG preserves decoded image pixels, not the original JPEG compressed bytes; document possible resulting file-size increase.

### 3.2 Canonical in-memory image

Establish reusable internal image-resource utilities rather than embedding decoding and resampling in the `CImage::Paint` method. At minimum, distinguish:

* **Serialized bytes:** ICO or PNG in the JSON resource.
* **Decoded pixels:** a known channel order and alpha representation (e.g., straight RGBA8 for decoding / serialization and premultiplied BGRA8 for GDI painting).
* **ICO entry metadata:** source entry dimensions, bit depth, and whether it is PNG-compressed or DIB-based.
* **Cached rasterizations:** decoded/resampled image for a requested size, DPI, interpolation, and optional orientation/flip settings.

Cache at the image-instance or reusable resource layer with a bounded strategy. Invalidate caches when the image, scaling settings, transformations, or relevant DPI/size changes. Cache ownership must not retain destroyed controls or leak GDI handles.

Keep the original ICO container bytes intact on load/save (including multiple resolutions). For raster inputs, save the normalized PNG, not a copy of the path to the original file. Forms must remain self-contained when image files are moved or deleted.

### 3.3 Validation and failure semantics

Validate data **before replacing** an existing Picture:

* Reject malformed/truncated ICOs, invalid PNGs, unsupported compression/entry variants with clear diagnostics, absurd dimensions, decoded-size overflow, and excessive resource sizes.
* Support ICO entries encoded as PNG as well as standard DIB-based entries, subject to decoder capability; preserve the entire ICO even if some entries cannot be displayed, but do not accept an unusable ICO silently.
* Explicitly cap compressed bytes, width/height, total decoded bytes, and work needed for repeated scaling; choose sensible values and document them, rather than accepting unbounded allocation from JSON.
* Treat `null` or an explicit Clear Picture action as an empty image; a failed import must retain the previous image/property value and native visual state.
* Do not invoke application callbacks during design-time loading, and do not hold file handles after importing.

Use existing `RSTRING`, JSON, ownership/reference-counting, and SimbeyCore Base64 conventions.

## 4. Picture JSON property

Use the established resource representation for both image types:

**Normalized raster image:**

```json
"Picture": {
  "format": "png",
  "encoding": "base64",
  "data": "<base64 of PNG bytes>"
}
```

**Icon:**

```json
"Picture": {
  "format": "ico",
  "encoding": "base64",
  "data": "<base64 of original ICO bytes>"
}
```

Empty image: `"Picture": null` (or absent for the default, in accordance with existing serializer conventions).

`<base64 ...>` is documentation notation, not literal file content. Actual generated JSON must contain the encoded bytes.

* Use SimbeyCore's Base64 functions already adopted for Form icons. Reuse or generalize existing ICO helpers where appropriate; avoid duplicating icon parsing unnecessarily.
* The `Picture` property must work through generic property descriptors and return appropriate deep copies of mutable JSON values.
* Serialize deterministically; repeated load/save without edits must not convert ICO to PNG, shift property state, or need the original import path.
* The designer may show an import-file path for convenience in session-only UI, but the path must not be required to recreate the resource or become a runtime dependency.

## 5. SizeMode and alignment

Define a single enum property **`SizeMode`**, default **`Contain`**:

| Value | Behavior |
|---|---|
| `Normal` | Display intrinsic image size in image pixels at a defined 1:1 pixel policy, aligned using `ImageAlignment`; clip to control bounds |
| `Stretch` | Fill the destination rectangle, scaling X and Y independently; aspect ratio may distort |
| `Contain` | Uniformly scale to show the whole image within the destination; letterbox as needed |
| `Cover` | Uniformly scale to cover the destination; clip the excess and use alignment to determine the crop |
| `Tile` | Repeat the source at natural size; clip at control bounds; optionally offset the tile origin |

There is **no separate `Center` size mode**. The centered, unscaled behavior is `Normal` + `ImageAlignment=Center`.

Define **`ImageAlignment`** with nine positions, default **`Center`**:

`TopLeft`, `TopCenter`, `TopRight`, `MiddleLeft`, `Center`, `MiddleRight`, `BottomLeft`, `BottomCenter`, `BottomRight`.

For `Normal`, position the natural-size image inside the available rectangle, including negative offsets if it is larger. For `Contain`, align within any unused space. For `Cover`, align the crop (e.g. `TopCenter` preserves the top-center of the source). For `Stretch`, alignment has no visible effect; preserve its stored value. For `Tile`, alignment can be ignored in favor of explicit tile offsets, but define that behavior clearly.

**Coordinate and DPI rule:** QuadooForm stores bounds in 96-DPI logical units; `Paint`'s rectangle is physical pixels. Define and consistently apply the policy for how source pixels map into physical pixels at non-96 DPI (particularly `Normal`, `Tile`, and `AutoSize`). Recommendation: natural pixel size is understood at 96-DPI baseline and scales with the control's effective DPI, while the selected ICO entry is chosen for the desired physical output. Document this and test 96/144/192 DPI, so the control does not change physical size erratically when the monitor DPI changes.

**Tiles:** optionally expose `TileOffsetX` and `TileOffsetY` as signed 96-DPI logical properties, default zero, to move the tiling origin independently of the control position. Clip all tiles to control bounds and ensure negative offsets work.

## 6. Interpolation and rendering quality

Define **`Interpolation`** (default `HighQuality`):

* `Nearest`: no filtering; preferred for pixel art and crisp sprites.
* `Bilinear`: fast, smoother scaled images.
* `Bicubic`: smoother high-quality general-purpose scaling.
* `HighQuality`: best available quality, including area-based or multistage downscaling where appropriate.

Implement with XP-compatible GDI+, a tested CPU resampler, or a combination. Do not silently map all modes to the same GDI `StretchBlt` quality. If the initial renderer cannot meaningfully distinguish `Bicubic` and `HighQuality`, either implement the distinction or document the limitation before declaring the feature complete.

Optional **`PixelSnap`** (Boolean, default false): round the computed destination rect/transforms to physical pixel boundaries for crisp nearest-neighbor behavior. Since Win32 raster drawing already operates on integer device pixels, specify exactly which intermediate geometry is snapped rather than adding an inert property.

Ensure that scaling and compositing are implemented without resizing and overwriting the stored source data on every paint.

## 7. Per-pixel alpha, background, and opacity

Expose:

| Property | Type | Default | Meaning |
|---|---|---|---|
| `Transparent` | Boolean | `true` | Composite source alpha over existing parent-painted content |
| `Opacity` | Integer, 0–255 | `255` | Global opacity multiplied by each source pixel's alpha |
| `BackColor` | Existing color property | Existing default/inherited | Fill the control background first when `Transparent=false` |

* `Transparent=true`: do **not** fill an opaque rectangle behind the image. Correctly blend per-pixel PNG/ICO alpha over the parent's already-painted content; transparent areas show the parent background.
* `Transparent=false`: first paint the appropriate `BackColor` within bounds, then composite image pixels according to their alpha. An image with transparent pixels reveals `BackColor`.
* `Opacity=0`: render no image pixels; an opaque background fill still happens if `Transparent=false`.
* `Opacity=255`: original per-pixel alpha unchanged.
* `AlphaBlend` with `AC_SRC_ALPHA` expects **premultiplied BGRA** source data. Multiply RGB channels by per-pixel alpha before compositing, then apply global opacity (`SourceConstantAlpha` or equivalent), avoiding double multiplication.
* Fully transparent PNG pixels must not leave RGB fringes/halos after scaling; resampling in premultiplied alpha space is preferred.
* Clipping, overlap, redraw ordering, and invalidation must work with the framework's painted children. Do not rely on a nonexistent independent transparent HWND or assume that overlapping separate HWND siblings can be made transparent by only drawing into a memory DC.
* The control should follow existing painted-control mouse/event behavior; no pointer capture solely for imaging.

Color-key transparency is intentionally **out of scope**.

## 8. ICO behavior

* If `Picture.format` is `ico`, treat it as a multi-resolution icon resource rather than selecting and permanently flattening one entry at load time.
* For each paint, choose the best available embedded resolution based on desired physical display dimensions and DPI. Favor an appropriately sized or larger entry for downscaling rather than unnecessary upscaling. Avoid changing entry unpredictably between repeated paints of unchanged geometry.
* Decode and draw both DIB-style and PNG-compressed ICO entries on supported platforms; the underlying resource remains unchanged in JSON.
* Include optional advanced properties **`IconSizeMode`** (`Auto`, `Small`, `Large`, `Custom`) and **`IconWidth` / `IconHeight`** for `Custom`, if they can be added without muddying the normal SizeMode contract. Define how these select an icon entry/resolution versus the final destination scaling. In the simplest first pass, `Auto` alone is sufficient, with other selections documented as deferred if necessary.
* Optional `IconFrameIndex` for forcing a specific icon entry can be deferred. If exposed, validate it and specify how changing image resources invalidates the old index.
* Preserve alpha and the ICO AND mask semantics for legacy icons.

## 9. Geometry, AutoSize, and read-only diagnostics

* Support the existing common **`AutoSize`** mechanism. For a raster image, preferred size is its intrinsic image dimensions. For ICO, preferred size is the selected/icon-design size under the documented DPI policy. In the absence of a Picture, retain sensible default bounds.
* Measurement must cooperate with `Align`, Anchors, Margins, Padding, minimum/maximum constraints, and the framework's requested/effective bounds model. Docking may override an AutoSize measurement on the stretched axis; AutoSize must not repeatedly overwrite stored bounds during layout.
* Expose derived, read-only descriptors when meaningful:
  * `ImageWidth` — natural width of the displayed source (ICO semantics documented).
  * `ImageHeight` — natural height.
  * `HasAlpha` — decoded image includes transparency (including icon mask/alpha behavior).
  * `ImageFormat` — `None`, `PNG`, or `ICO` for the stored resource.
  * `ImageSize` — byte count of embedded serialized image resource (not base64 character count).
* These diagnostics must not themselves be stored as independent JSON values; compute them from the Picture and make them observable if the existing property-observer system supports it.

## 10. Optional transformations and hit testing

If implementation complexity permits after the core works, support:

* `FlipHorizontal` (false)
* `FlipVertical` (false)
* `Rotation` (`0`, `90`, `180`, `270` degrees)
* `ClipToBounds` (true; recommendation: always clip to bounds for the initial version)

Specify order of operations and determine whether `ImageWidth`/`ImageHeight` and AutoSize reflect dimensions after a 90°/270° rotation. Keep rendering cache keys consistent with transformations.

Optional **second-phase**, not a prerequisite for the initial control:

* `HitTestMode`: `Bounds` (default) versus `Alpha`.
* `AlphaThreshold`: 0–255 threshold for alpha hit tests after size mode, crop, opacity, alignment, flip, and rotation.

The first release must implement `HitTest(INT xLogical, INT yLogical, BOOL*)` correctly for bounds, including coordinate convention and disabled/hidden control behavior as used by QuadooForm's painted-control dispatch. Do not claim alpha hit testing unless it is actually implemented and tested.

## 11. Visual designer integration

* Add **Image** to the dynamically discovered Toolbox/palette with a distinct approximately 20×20 icon, alongside other visual controls. It should not be categorized as a nonvisual component.
* It must be creatable from the palette and selectable from both the design surface and Studio/standalone Objects tree.
* Support the existing eight resize grips and designer-driven movement, honoring any metadata-based movement/resizing limitations. `SizeMode` changes how the image renders **inside** the bounds; they must not change the actual control dimensions unless AutoSize is engaged.
* Add `Picture` to the property inspector with **Browse…** / **Clear** actions, supporting `.ico`, `.png`, `.jpg`, `.jpeg`, and `.bmp` through `CChooseFile`/the editor's existing file-dialog utilities.
* Show an appropriate thumbnail/metadata in the property editor if easy, but preserve existing descriptor-driven editor architecture and do not hard-code a custom factory type in the generic designer whenever a generic image property editor is feasible.
* Importing/replacing/clearing a Picture must correctly mark the document dirty, update the canvas without reopening the file, support rollback on invalid input, and preserve the selection and control GUID.
* All image display modes, alignment, alpha, and opacity must appear identically in designer and modal Preview to the greatest extent possible; Preview constructs a separate genuine top-level runtime Form with its own object graph.
* No application event handler should fire merely because the user is changing the Picture in Design mode.
* If the designer's Properties inspector currently lacks enum-choice editors, improve the descriptor-driven enum presentation rather than exposing raw numbers for SizeMode, ImageAlignment, and Interpolation.

## 12. Reusable graphics infrastructure

Create an internal resource/renderer abstraction suitable for later reuse by:

* Image buttons and other control background images.
* Future image-list components for TreeView, ListView, Toolbar, or ComboBoxEx.
* Other native UI components that must paint PNG/ICO imagery at varying DPI.

Keep the generic image decoder/resampler independent from the `CImage` component where practical. This **does not** mean implementing shared `IQuadooImageList` or advanced image-aware controls now.

Adhere to existing repository conventions: custom allocation/container classes, `RSTRING`, SimbeyCore, JSON interfaces, HRESULT errors, COM reference ownership, VS2008 syntax, no STL or C++ exceptions. Do not replace existing image infrastructure if it already satisfies the needed behavior.

## 13. Example full JSON definition

```json
{
  "type": "Image",
  "name": "imgLogo",
  "properties": {
    "Left": 16,
    "Top": 20,
    "Width": 240,
    "Height": 160,
    "Visible": true,
    "AutoSize": false,
    "SizeMode": "Contain",
    "ImageAlignment": "Center",
    "Interpolation": "HighQuality",
    "Transparent": true,
    "Opacity": 255,
    "Picture": {
      "format": "png",
      "encoding": "base64",
      "data": "<actual Base64 PNG content>"
    }
  },
  "events": {
    "OnClick": "imgLogo_OnClick"
  }
}
```

The real framework serializer should also include its established `schemaVersion`, persistent component `id`, and any required metadata. This example focuses on the Image-specific properties and event; do not treat the sample as a replacement for the existing full-schema requirements.

**ICO use:** change `Picture.format` to `"ico"` and supply the original ICO bytes. Keep all embedded resolutions.

## 14. Validation and acceptance tests

Extend both the automated native harness and interactive designer examples. At minimum test:

1. **Interface/capabilities:** `QueryInterface(IQuadooStaticControl)` succeeds; `IQuadooWinControl` fails; `IQuadooControl` identity and canonical `IUnknown` remain consistent. No image HWND is created.
2. **Factory/metadata:** factory enumeration, shared base descriptors, defaults, read-only derived values, property notifications, instance/factory lifetime, registration, and JSON serialization/deserialization work without `Kind` special cases.
3. **Import/persistence:** ICO with multiple entries survives byte-for-byte serialization; PNG round-trips; JPEG/BMP import produces valid PNG; resource remains usable after deleting/moving the source file; Base64 corruption and unsupported entries fail without mutating existing Picture.
4. **Scaling:** every `SizeMode` at multiple rectangle shapes; no distortion for Contain/Cover; expected crop alignment; Normal and Tile positioning; negative tile offsets if implemented.
5. **Alignment:** all nine alignments produce the correct placement/crop, including images larger than the control.
6. **Alpha:** fully opaque, fully transparent, partially transparent pixels; `Transparent=true/false`; `Opacity=0/128/255`; pixels near transparent edges without visible halos; parent background/invalidation correctly composited.
7. **Interpolation:** visibly and numerically distinct nearest versus smooth filtering, with meaningful test cases for bilinear/bicubic/high-quality downscaling.
8. **ICO selection:** 16/32/48/256 multi-resolution entries, DIB and PNG-encoded icon entries, expected physical-size selection across DPI values, preserved original bytes.
9. **DPI/layout:** 96/144/192 logical-to-physical calculations, AutoSize, Align, Anchors, nested panels, scrolling, and non-drifting requested bounds. Test painting without modifying stored geometry.
10. **Lifecycle:** creation/destruction, changing and clearing Picture, repeated decode/repaint/resize, destruction during event callback, no leaked GDI/User handles, no factory live-instance retention after release.
11. **Designer:** select/import/clear/resize/move/preview/save/reopen, dirty-state updates, Properties inspector and Objects tree synchronization; PNG/ICO resources survive save/reopen.
12. **Custom controls:** no changes to the public custom-factory contract are required for the Image class, and existing independently built extension-DLL tests remain green.
13. **Windows XP:** run Debug and Release harnesses on XP when available. No mandatory APIs newer than XP or toolchain changes. If modern PNG-compressed ICO decoding needs an internal decoder, use one instead of depending on newer shell facilities.
14. **Failure injection:** allocation, decoding, image replacement, serialization, descriptor validation, and paint-time failures do not leave partially committed property values, dangling pointers, or unusable HWND/container state.

Include at least one interactive designer example combining a photograph in `Cover`, an alpha PNG in `Contain`, an ICO in `Normal`, and a pixel-art image in `Nearest`, preferably over contrasting parent backgrounds.

## 15. Suggested implementation phases

**Phase A — Image resource utility and serialization.** Audit existing repository image facilities, implement validated ICO/PNG resource handling, JPG/BMP import, Base64 and lodepng serialization, ownership, and failure rollback.

**Phase B — Windowless Image control.** Implement `CImage : ... IQuadooStaticControl` using the existing runtime's static-control patterns, descriptors, size/alignment modes, alpha compositing, and hit testing. Add reusable scaling/caching as required.

**Phase C — Designer integration.** Palette icon, Picture property editor, file browsing, property grid enums, design-surface editing, modal Preview, and JSON round-trip behavior.

**Phase D — Quality and optional enhancements.** Complete interpolation and DPI/ICO behavior, expose read-only diagnostics, add transformation/tile options where approved, and run performance/resource/XP validation.

Do not begin TreeView, ListView, image-list components, Quadoo Studio integration, or QuadooScript host work as part of this task unless changes to existing infrastructure are strictly necessary for the Image control.

## 16. Specific Codex assignment

> Implement the QuadooForm **Image** visual control described in this document. It must implement the existing `IQuadooStaticControl` interface exactly as published and must not own an HWND. Add Image to the designer Toolbox, with descriptor-driven properties for Picture, SizeMode, nine-position ImageAlignment, Interpolation, Transparent, Opacity, and AutoSize. Import ICO, PNG, JPEG and BMP; preserve ICO bytes and multi-resolution entries; normalize other images to PNG using lodepng; serialize both forms as Base64 through SimbeyCore. Use a reusable rendering/decoding layer with correct per-pixel alpha and premultiplied BGRA compositing. Support Normal/Stretch/Contain/Cover/Tile sizing, clipping, appropriate interpolation, layout/DPI behavior, modal Preview, and robust rollback.
>
> Do not add any color-key features. Do not create an Image HWND or resurrect a central `Kind` enumeration. Keep existing public ABI and VS2008/Windows XP support. Implement the tests in Section 14 and report any unimplemented optional features explicitly. Before adding a new public interface or changing ABI, explain the need and compatibility consequences. Build and test Debug/Release, including XP where available.

---

### Repository-specific details Codex must inspect before coding

* The current published `QuadooForm.h`, especially `IQuadooStaticControl`, the factory/descriptor contracts and the existing Label/painted-child implementation.
* Current `CChooseFile`/designer Picture editor code and existing Form ICO serialization/helpers.
* Available `lodepng` version, the existing JPEG/BMP import path or GDI+ wrappers, and existing alpha blending/resampling utilities.
* Existing property type/enum conventions, including any current `QuadooProperty::Type::Memo` handling; it is **not** a requirement for the Image control.
* The current runtime resource lifetime and C++ test-harness extension mechanisms.

If repository findings conflict with this specification, retain the explicitly agreed behavior and present any required tradeoffs or public-contract changes before implementing them. Do not silently replace image byte preservation, windowless painting, or the parent-rendering model.
