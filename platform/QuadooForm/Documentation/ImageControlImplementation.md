# QuadooForm Image control

Implemented against `QuadooForm_ImageControl_Design.md` with VC9, no STL or C++
exceptions. `Image` is a separately registered, windowless `IQuadooStaticControl`.
It owns no HWND and uses the existing parent painting, hit testing, click events,
layout, factories, observers and JSON serializer. No script host is required.

## Public contract

All 51 existing interface declarations, IIDs, vtables, exports and export ordinals
are unchanged. `QuadooProperty::Picture` is appended as value 18 after Memo; all
older property values retain their numbers. This identifies a generic picture
editor for built-in and independent factories. Hosts that want to edit this new
property type should rebuild against the updated header. No new public interface
or custom-factory requirement was added.

| Property | Default / behavior |
|---|---|
| Picture | null; embedded PNG or original complete ICO |
| SizeMode | Contain; Normal, Stretch, Contain, Cover, Tile |
| ImageAlignment | Center; all nine positions from TopLeft to BottomRight |
| Interpolation | HighQuality; Nearest, Bilinear, Bicubic, HighQuality |
| Transparent | true; otherwise fill BackColor before compositing |
| Opacity | 255; integer 0 through 255 |
| AutoSize | false; natural source dimensions when enabled |
| TileOffsetX / TileOffsetY | 0; signed logical offsets, -1,000,000 through 1,000,000 |
| ImageWidth / ImageHeight | read-only natural dimensions, zero when empty |
| HasAlpha | read-only; any usable entry contains transparency/mask effects |
| ImageFormat | read-only None, PNG or ICO |
| ImageSize | read-only embedded byte count, excluding Base64 expansion |

Default requested bounds are 100 by 100 logical units. Diagnostics are computed,
observable after Picture replacement/clear, and excluded from serialization.
Writes to read-only descriptors are rejected, including initial JSON values.

Picture is null or an object containing exactly `format`, `encoding`, and `data`.
Formats are `png` and `ico`, encoding is `base64`. SimbeyCore performs Base64
conversion; canonical padding and content are validated. Mutable JSON returned
through the property API is independent of stored state. Invalid replacements
leave the previous Picture, diagnostics and rendered resource intact.

## Imaging and DPI

`Imaging/` is private reusable resource/codec/rendering support with no control
or HWND ownership. Decoded painting pixels are top-down premultiplied BGRA8;
PNG encoding/import uses straight RGBA8. `AlphaBlend` applies global opacity once.
Filtering in premultiplied space prevents transparent RGB from creating fringes.

Nearest, bilinear and Catmull-Rom bicubic are distinct CPU filters. HighQuality
uses exact area coverage for reduction and bicubic for enlargement. Filtering
operates on encoded channel values; it does not perform linear-light/gamma
conversion. Cubic output is clamped to preserve premultiplied-alpha invariants.
Source bytes are never repeatedly resized or overwritten.

Paint rectangles are physical pixels. Natural dimensions, AutoSize, Normal,
Tile and tile offsets use a 96-DPI baseline, scaled by the effective control DPI.
Contain/Cover preserve aspect ratio; alignment positions letterboxes or crops.
Stretch preserves the stored alignment but ignores it visually. Tile ignores
alignment and uses explicit offsets, including negative offsets. Painting never
changes requested geometry. Clipping to the control bounds is always enabled.

For ICO, natural size is the usable entry closest to 32 by 32, preferring entries
at least that large. This stable design size drives diagnostics and AutoSize.
Each paint separately selects the closest entry at least as large as the desired
physical output where available, breaking equal-size ties by bit depth and stable
directory order. Tests exercise 16/32/48/256 entries at 96/144/192 DPI.

ICO PNG entries use the internal PNG decoder, including on XP. Standard DIB
entries support BITMAPINFOHEADER/BI_RGB at 1/4/8/16/24/32 bits, palettes, legacy
AND/XOR transparency and inversion, and 32-bit alpha. Valid 32-bit alpha takes
precedence over the legacy AND mask. Other DIB headers/compression variants are
retained but skipped if another usable entry exists; an unusable ICO is rejected.
Malformed supported entries invalidate the resource. Original ICO bytes and all
entries survive save/reopen unchanged.

One final raster/DC/DIB cache per resource is keyed by size, DPI, sizing,
alignment, filtering and tile origin. Replacement cache construction is staged;
allocation failures retain the usable old cache. Opacity and background are
applied during composition and need no new raster. Cached handles are released
on Picture replacement, clear and destruction.

## Imports, editor and limits

The descriptor-driven Picture dialog provides Browse, Clear, and a checkerboard
thumbnail. It accepts ICO, PNG, JPG/JPEG and BMP using `CChooseFile`. PNG and ICO
bytes are validated and retained. JPEG/BMP import uses XP-compatible GDI+ over
an in-memory stream and the bundled lodepng encoder. Valid JPEG EXIF orientation
is normalized. Other metadata is not retained; normalized PNG can be larger than
the original JPEG. No source path or file handle remains in the resource.

The shared designer core supplies this editor to standalone and embedded hosts.
Image is dynamically discovered in the palette with a 20-pixel DLL icon. Selection,
movement, eight resize grips, dirty state, Objects tree and genuine modal Preview
use existing generic paths. Changing Picture in Design mode invokes no script
callbacks. The existing Form Icon editor retains its separate ICO semantics.

Limits are 8 MiB per compressed input/normalized output, 4096 pixels per source
or output dimension, 4,194,304 source/output pixels, 64 ICO entries and 4,194,304
aggregate ICO-entry pixels. A raster rebuild is limited to 67,108,864 estimated
filter taps, counting only covered pixels and both legacy XOR planes where used.
The codec/pixel allocator caps individual allocations at 80 MiB and concurrent
tracked allocations at 96 MiB per linked imaging module. JSON/stream objects and
GDI allocations have their own ownership and are not included in that heap budget.
Designer document input/output is capped at 64 MiB to allow Base64 resources.
Requests exceeding limits fail with an HRESULT rather than an unchecked size or
unbounded allocation. PNG validation additionally checks complete IHDR/IEND,
chunk bounds, CRCs and exact inflated scanline size, including Adam7.

Transforms, PixelSnap, forced ICO frame/size selection, alpha-sensitive hit tests
and shared image-list components are deferred. HitTest is rectangular and follows
the existing hidden/disabled painted-control rules. No color-key feature was added.

## Validation

All four configurations built successfully with VC9 and zero errors/warnings:

| Configuration | Full public runtime | Private tests | Full designer | Studio integration |
|---|---:|---:|---:|---:|
| Win32 Debug | 7,895 passed | 665 passed | 3,065 passed | 87 passed |
| Win32 Release | 7,895 passed | 665 passed | 3,065 passed | 87 passed |
| x64 Debug | 7,895 passed | 665 passed | 3,065 passed | 87 passed |
| x64 Release | 7,895 passed | 665 passed | 3,065 passed | 87 passed |

The full runtime suite ran with `--continue-on-failure` and reported no failures.
The independent extension-DLL tests remain green. The full private suite initially
passed 664 assertions. A final check rejecting bad CRCs in ignored PNG ancillary
chunks added one regression assertion; after that change, all four configurations
were rebuilt and the entire private suite passed 665. The affected public Image,
designer Image, and existing PNG-editor suites were rerun in each configuration,
passing 126, 31, and 106 assertions respectively. The full public/designer/Studio
runs precede that final, stricter ancillary-CRC guard.

Coverage includes COM identity/no image HWND, descriptors and diagnostic observers,
alpha/opacity/background overlap, all alignments, distinct resamplers, ICO entry
selection and original-byte persistence, import/orientation/source deletion,
DPI/AutoSize/docking, reentrant destruction, allocation/paint/stream failures,
replacement rollback, and strict repeated-replacement GDI-handle counts. The
designer tests drive palette insertion, selection, resize grips, generic enum and
Picture dialogs, Cancel/Clear/dirty state, save/reopen/GUID stability and native
Preview pixels. Existing suites retain their original resource thresholds.

Initial full Debug designer runs caught old palette/image-list count expectations
after adding Image. Those expectations were updated; failing logs are retained
separately from successful runs. No product behavior was changed to satisfy those
checks. An earlier x64 build exposed size-conversion warnings in the bundled C
codec. The wrapper's bounded allocator makes those legacy codec sizes safe;
warning 4267 is disabled only around that third-party source inclusion. Final
and CRC builds have zero warnings.

Logs are under `target/ImageControl-{Debug,Release}-{Win32,x64}-*.log`:
`native` is the full public run; Debug `designer-final` and Release `designer`
are the full designer runs; `studio` records embedding regressions. `crc-build`,
`crc-private`, `crc-native-image`, `crc-designer-image`, and `crc-designer-png`
record final rebuilding and affected-suite validation. Earlier failed expectation
logs are Debug `designer`, and earlier compilation diagnostics remain in the
initial build logs. No failing resource count was hidden or relaxed.

Manual Release x64 checks using the computer-use skill verified the complete
gallery at 75% zoom, surface selection and Objects synchronization, alpha
thumbnail, JPEG Browse/import with orientation, Cancel restoring the original
picture without dirtying the document, and native modal Preview/Close returning
S_OK. The test instance was closed without changing the example file.

All four SimbeyCore DLL hashes match the pre-build hashes. SimbeyCore was neither
rebuilt nor replaced. No Studio or QuadooScript source changes were made for
this control; the existing shared-core hosts were rebuilt for validation.
Actual Windows XP execution and physical monitor transitions require a suitable
machine; automated tests exercise effective 96/144/192 DPI on this Windows host.

The self-contained interactive example is
`DesignerExamples/ImageGallery.qform.json`: forest photograph in Cover, authored
alpha PNG in Contain, multi-resolution ICO in Normal, and nearest-filtered pixel
art over contrasting backgrounds. The photograph uses an existing repository
asset. `TestImages/GenerateFixtures.py` regenerates the example and deterministic
PNG/ICO/JPEG/BMP fixtures using Pillow as an authoring dependency only; normal C++
builds do not need Python or Pillow.
