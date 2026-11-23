# ImageList component and editor

Implemented October 24, 2026. ImageList is a real nonvisual component registered
by QuadooForm.dll. It uses the existing component factories, descriptors,
ownership, immutable GUIDs, lifetime tracking, and JSON serializer. It exposes
no visual-control interfaces and creates no control HWND. Forms and Frames both
support it through their ordinary component collections.

No published runtime interfaces, GUIDs, exports, or vtables were changed. The
shared editor and imaging helpers are private C++ implementation details. This
iteration creates no native HIMAGELIST and adds no consumers or script APIs.

## Properties and limits

| Property | Default | Behavior |
|---|---|---|
| Name | Generated | Ordinary editable component name |
| Width | 16 | Cell width, independent of Height |
| Height | 16 | Cell height, independent of Width |
| ColorDepth | Color24 | Color4, Color8, Color16, Color24, Color32 |
| Masked | true | Independent of ColorDepth |
| ImageCount | 0 | Read-only, derived from Images, not serialized |
| Images | Empty array | Dedicated collection editor |

Dimensions are limited to 1–256 pixels per axis and collections to 256 images.
Existing image limits also apply: 4096 pixels per source axis, 4,194,304 aggregate
source pixels, and 8 MiB of aggregate encoded PNG bytes. Files exceeding the
existing 8 MiB import limit are rejected before decoding. Batch imports enforce
collection limits while staging each entry and publish only after the entire
batch succeeds. A replacement must produce exactly one entry.

## Sources, sizing, and transparency

Each entry stores one original-size PNG source plus its sizing mode. No cell
bitmap or thumbnail is serialized. PNG imports retain their source bytes. BMPs
and the selected ICO image are normalized through the existing image codecs,
lodepng, and SimbeyCore Base64 facilities. Consequently, dimension changes always
regenerate from the retained source rather than from a previously resized cell.

ICO import selects a suitable resolution for the current dimensions, preferring
an appropriate larger entry over enlargement of a smaller one. Both classic
DIB-based and PNG-compressed entries are supported. Destination-XOR effects
cannot be represented losslessly as PNG and are rejected with
ERROR_NOT_SUPPORTED. An ICO becomes one PNG entry, rather than retaining its
whole multi-resolution container.

Sizing is stored as `Crop`, `Stretch`, `Center`, or `Contain`; missing sizing
metadata defaults to Contain without adding a field on an unchanged round trip.
Crop preserves scale at the top-left. Center preserves scale and uses integer
centering with transparent margins or clipping. Stretch fills the cell. Contain
preserves aspect ratio and centers the result. Resampling reuses the existing
premultiplied-alpha renderer.

Color32 retains per-pixel alpha with either Masked setting. For lower depths,
Masked converts alpha below 128 to transparent and all remaining pixels to
opaque colors. Without Masked, pixels are composited on white. Color4 uses the
nearest Windows 16-color palette entry, Color8 uses a 3-3-2 palette, Color16 uses
RGB565, and Color24 retains full RGB. These conversions affect generated cell
pixels only; the serialized source retains its original color and alpha.

BMPs are opaque unless the explicit top-left color-key option is selected. An
exact-multiple BMP strip prompts before splitting into cells in row-major order.
PNG and ICO images are never automatically split. Checking Top-left color as
mask immediately masks matching colors in the current collection and also
affects subsequent imports. Unchecking it restores the retained source pixels.
Apply/OK persist the setting; Cancel discards changes since the last Apply.

Masking is nondestructive: the PNG source is unchanged, and optional `colorKey`
boolean metadata enables a derived masked rendering. A split BMP also retains
its original strip background in optional `maskColor` metadata, an RGB/COLORREF
integer from 0 to 0xffffff. All cells use that original strip key, even when an
individual cell starts with a foreground pixel. Without maskColor, the original
PNG's top-left color is used. Existing entries without these fields remain
valid and round-trip unchanged. Mixed per-entry settings are preserved on open;
an explicit checkbox change applies to the whole collection.

The key is applied before resampling. Temporary masked PNGs retain explicit RGBA,
bypassing automatic grayscale/color-key optimization in the bundled codec so
opaque black foreground pixels are preserved. The source PNG and its original
alpha remain available when the option is disabled. This cannot recover pixels
already made transparent by the earlier destructive import implementation.

## Editing and host integration

ImageList has a standard 20×20 palette icon in QuadooForm.dll, with source assets
under Icons. The existing shared palette/tray image list loads it through the
same published factory mechanism as every other component. Studio uses its
ordinary component tree and generated-member synchronization for placement,
renaming, and deletion.

Open the editor through the tray double-click, the Images property, the component
context menu, or Form > Image List Editor. The command is enabled when an
ImageList is selected in either host.

Menu Editor is disabled for ImageList and other unrelated nonvisual components,
including when another component on the form supplies a MainMenu or PopupMenu.

The native Win32 dialog includes indexed thumbnails, a larger checkerboard
preview, source/cell metadata, dimension/depth/mask settings, and import sizing.
Add supports multiple BMP/ICO/PNG files. Replace, Delete, confirmed Clear All,
Move Left/Right, and cross-row drag reordering operate on its temporary snapshot.
Reordering preserves the selected image and immediately renumbers its index.
Deleting selects the next surviving image at that index, or the previous image
when the last entry is deleted.

Apply validates every generated cell and submits one staged definition through
the existing document/source transaction. It establishes a new baseline. OK
does the same and closes; Cancel discards edits since opening or the last Apply.
Opening and accepting an unchanged collection does not dirty the document.
Failed imports, conversions, and rejected source transactions preserve the live
document. Save and Save As use the normal form/frame persistence path.

The example [DesignerExamples/ImageLists.qform.json](DesignerExamples/ImageLists.qform.json)
contains the same 16 normalized sources shown below, originally imported from
distinct BMP, PNG, and ICO fixtures.

![Image List Editor with sixteen indexed images](Screenshots/ImageListEditor.png)

## Validation

Only the new ImageList tests were run for this task, as requested. VS2008 builds
succeeded for QuadooForm, the designer core, standalone designer, Quadoo Studio,
and their affected native/designer/Studio test executables in all four
configurations. SimbeyCore was not rebuilt.

| Configuration | Native ImageList | Designer ImageList | Studio ImageList |
|---|---:|---:|---:|
| Debug Win32 | 25 assertions | 358 assertions | 14 assertions |
| Release Win32 | 25 assertions | 358 assertions | 14 assertions |
| Debug x64 | 25 assertions | 358 assertions | 14 assertions |
| Release x64 | 25 assertions | 358 assertions | 14 assertions |

The focused tests cover component/factory lifetime and capabilities, independent
dimensions, all depths, source retention, alpha/masks, BMP/PNG/both ICO encodings,
best ICO resolution, strips, batch rollback, replacement, deletion, clearing,
cross-row moves and drag selection, unchanged OK, Apply/Cancel, source-edit
rejection, allocation failure, malformed/missing Base64, invalid metadata,
excessive counts, oversized files, deterministic disk/JSON reconstruction,
stable GUIDs, dirty state, GDI/USER resource counts, tray/property entry points,
Studio commands/tree, and generated-member persistence. The designer test writes
ImageListEditor.png and ImageListExample.qform next to its executable.

The masking regressions also cover toggling after import, immediate owner-drawn
preview changes, reopening with a saved mask enabled, Cancel without Apply,
restoring pixels after Apply/reopen, allocation rollback, strip-wide background
keys, preserved black foreground pixels, and unrelated Menu Editor availability
in both hosts. The optional reported-bitmap case was run against
`D:\dev\MailRes\core\bmp\msoe\16x16.bmp`: 61 cells, with exactly the 7,453 cyan
background pixels made transparent and no extra foreground pixels removed.

Run only these new tests from the repository root:

```powershell
tests\platform\QuadooForm\test-imagelist.ps1
tests\platform\QuadooForm\test-imagelist.ps1 -Configuration Debug -Platform Win32
```

Or invoke the existing executables with the focused flag:

```bat
QuadooFormHarness.exe --image-list-only
QuadooFormDesignerTests.exe --image-list-only
QuadooStudioIntegrationTests.exe --image-list-only
```

The additional reported-bitmap regression accepts that file as an argument:

```bat
QuadooFormDesignerTests.exe --image-list-only D:\dev\MailRes\core\bmp\msoe\16x16.bmp
```

Windows XP VM execution was not available in this session. The code uses the
VC9 toolset and XP-era Win32/GDI/common-control APIs. For VM verification, copy
the Win32 output and its existing runtime/VC9 CRT dependencies, retaining the
ImageListFixtures folder beside QuadooFormDesignerTests.exe, and run the focused
commands above on XP. These desktop test results do not substitute for that VM
check.
