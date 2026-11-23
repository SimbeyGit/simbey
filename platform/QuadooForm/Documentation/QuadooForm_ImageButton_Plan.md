# QuadooForm ImageButton implementation plan

Status: implemented, 2026-10-27. The scheduled implementation covers all four
stages. The overnight run was restricted to headless tests; the user subsequently
authorized Win32 GUI validation, now recorded in ImageButtonImplementation.md
with the modeless button-completion fix. Actual Windows XP VM validation remains
pending. See that report for the additive interface, results and remaining checks.

## 1. Proposed control

Register a visual native control named `ImageButton`, implemented as
`QF::CImageButton : public CButton`. It behaves like the existing Button and
displays a picture to the left of, or above, its caption. The image and caption
form one centered group. It keeps a real Win32 BUTTON HWND and native input,
focus, capture, accessibility and click behavior.

The new control inherits the Button metadata, lifecycle, logical event binding,
Default/Cancel behavior, ModalResult, PopupMenu, TabOrder, layout and script
Click/SetFocus methods. It has its own factory, property descriptors and palette
icon. It introduces no control-kind switches in shared framework code.

This adopts the useful image/caption layout idea from TBitBtn. Embarcadero's
[Layout documentation](https://docwiki.embarcadero.com/Libraries/Athens/en/Vcl.Buttons.TBitBtn.Layout)
describes placement around the caption. This initial control supports the two
requested placements, Left and Top, and uses independent embedded pictures for
normal and disabled states.

## 2. Existing code to reuse

| Area | Existing implementation | Planned use |
| --- | --- | --- |
| Button behavior | `QuadooForm/Runtime.h::CButton`, `Windows.cpp::CButton::Command`, `Keyboard.cpp` | Inherit click handling and modal completion; preserve mouse and keyboard semantics. |
| Metadata | `QuadooForm/Metadata.cpp::ButtonType`, `PropertySpec`, composed TypeSpec descriptors | Derive ImageButton metadata from ButtonType; add only image-specific properties. |
| Images | `Imaging/ImageResource.h`, `ImageResource.cpp`, `ImageRender.cpp`, `ImageImport.cpp` | Reuse embedded resource validation, PNG/ICO decoding, alpha, scaling, ICO entry selection, import and bounded allocation. |
| Property editing | `Designer/Inspector.cpp`, generic `QuadooProperty::Picture` editor | Browse, preview, replace and clear either picture without a control-specific editor. |
| Persistence and scripts | Generic JSON serializer and `ScriptAdapter/RuntimeObjects.cpp` | Descriptor-based property access and existing IQuadooButton Click dispatch. |
| Palette artwork | `Icons/GenerateIcons.py`, `PaletteResources.h`, `QuadooForm.rc`, `IQuadooPaletteIcon` | Deliver the new icon through the same capability used by both designers and external factories. |
| Rendering tests | `Harness`, `TestInternals`, `TestImages/ImageTestSupport.h` | Test actual painting, native input, failure rollback and resource cleanup. |

No new image format, Base64 implementation, image editor DLL or script compiler
integration is needed. SimbeyCore's existing Base64 facilities remain in use.

## 3. Properties and events

Recommended new properties:

| Property | Type | Default | Behavior |
| --- | --- | --- | --- |
| Picture | Picture | null | Main image, using the existing embedded PNG/ICO representation. |
| DisabledPicture | Picture | null | Explicit image displayed while disabled; takes precedence over automatic grayscale. |
| ImagePosition | Enum | Left | Left or Top, relative to Caption. |
| ImageWidth | Int | 16 | Image-cell width in logical pixels at 96 DPI; range 1..256. |
| ImageHeight | Int | 16 | Image-cell height in logical pixels at 96 DPI; range 1..256. |
| Spacing | Int | 4 | Gap between the image and caption, in 96-DPI logical pixels; range 0..1024. |
| Interpolation | Enum | HighQuality | Reuse Nearest, Bilinear, Bicubic and HighQuality rendering semantics. |

ImageWidth and ImageHeight define a bounding cell. Preserve the source aspect
ratio and center the image within that cell. The upper bounds keep a full image
cell within the existing four-million-pixel raster budget even at the runtime's
maximum supported 768 DPI. A caller can use a smaller cell for a large source
image without modifying the source data.

All new properties are serializable. ImagePosition, size and spacing edits
invalidate content layout and painting; they do not alter the requested button
bounds. Reuse the ordinary Button's initial 100 x 28 logical bounds. A Top layout
normally needs a taller button, illustrated in the examples.

Retain Caption, Font, ForeColor, BackColor, Visible, Enabled, Default, Cancel,
ModalResult, PopupMenu, TabOrder, Align, Anchors, Margins and size constraints
under their existing contracts. Do not add a second Text alias or duplicate
button events. Inherit OnClick, keyboard/focus/OnEnter/OnLeave and lifecycle
events through metadata composition.

Picture state is selected as follows:

| State | Image |
| --- | --- |
| Enabled, Picture present | Picture, unchanged. |
| Enabled, Picture absent | No glyph. |
| Disabled, DisabledPicture present | DisabledPicture, unchanged. |
| Disabled, no DisabledPicture, Picture present | Automatically generated grayscale rendering of Picture. |
| Both pictures absent | Caption-only button. |

The two image properties are independent: assigning DisabledPicture first is
allowed. Reserve the configured image cell whenever either picture is assigned,
so enabling/disabling the button never moves its caption. An unassigned image
for the current state leaves that cell empty. With neither picture assigned,
remove the cell and gap and center the caption normally.

No separate grayscale-enable property is needed for this pass. Assigning an
explicit DisabledPicture is the override, and clearing it restores the fallback.

## 4. Native rendering and framework integration

### 4.1 Recommended drawing approach

Use a native BUTTON with BS_OWNERDRAW, with one shared renderer for design mode,
Preview and application runtime. Use the active Windows BUTTON theme to draw
standard button chrome; use GDI DrawFrameControl/system colors when theming is
unavailable or disabled. Draw the caption and image into the resulting content
rectangle. Keep native button input processing instead of rebuilding mouse,
capture, Space-key or mnemonic behavior.

Microsoft documents parent-directed [WM_DRAWITEM](https://learn.microsoft.com/en-us/windows/win32/controls/wm-drawitem)
for owner-drawn buttons. The existing runtime forwards WM_COMMAND and WM_NOTIFY
to control wrappers, but does not yet forward WM_DRAWITEM. Add generic forwarding
by DRAWITEMSTRUCT.hwndItem to IQuadooMessageHandler; never cast a registered
control to CImageButton. Menu drawing must remain separate from control drawing.
Document the same forwarding obligation for independent native containers and
test the public-interface forwarding path.

Avoid relying on BCM_SETIMAGELIST as the sole renderer: its
[documented contract](https://learn.microsoft.com/en-us/windows/win32/controls/bcm-setimagelist)
requires Comctl32 version 6. Button
[custom draw](https://learn.microsoft.com/en-us/windows/win32/controls/nm-customdraw)
also depends on a version-6 activation context. A single owner-draw path makes
caption layout and disabled-image behavior consistent for classic and themed
hosts. This is a design choice; the compatibility tests will validate it on XP.

Use only XP-era theme functions, with graceful fallback. Microsoft's
[Windows XP theme overview](https://learn.microsoft.com/en-us/archive/msdn-magazine/2001/november/windows-xp-overview-take-advantage-of-new-windows-xp-features-in-your-apps-today)
describes theme drawing for custom controls. Do not infer historical XP API
availability from the minimum-OS tables on today's Learn pages; check the local
SDK and actual XP behavior. Avoid newer-only theme/DPI/buffered-paint imports.

### 4.2 General default-button presentation capability

The current CForm::UpdateDefaultButton inspects BUTTON styles and changes them
between BS_PUSHBUTTON and BS_DEFPUSHBUTTON. An owner-drawn button cannot use
that type-style transition: [BS_OWNERDRAW is a distinct button type](https://learn.microsoft.com/en-us/windows/win32/controls/button-styles).

Introduce one optional published capability, tentatively:

```cpp
interface IQuadooButtonPresentation : IUnknown
{
	STDMETHOD(SetDefaultAppearance)(BOOL fDefault) = 0;
};
```

Allocate a new unique IID during implementation. This is an additive interface;
do not add methods to IQuadooButton, IQuadooWinControl or IQuadooFormKeyboard.
The parameter is a transient visual decision, not a write to the Default
property, and is not serialized or exposed as a new script property.

The form continues to choose the effective default button with the existing
logical rules, then queries the presentation capability. CButton implements the
current native BM_SETSTYLE behavior. CImageButton overrides the presentation
method to retain BS_OWNERDRAW and repaint its default border. Controls without
the capability retain their own presentation policy; logical button activation
still works through IQuadooButton.

This removes the native class/style decision from the shared default-button
path instead of adding an ImageButton-specific exception. Independent button
wrappers can implement the same published interface. Use BEGIN_UNK_MAP and
verify canonical IUnknown identity and balanced references.

The ImageButton wrapper must also preserve its owner-draw type when native
dialog processing requests a default-style change, and report appropriate
WM_GETDLGCODE button/default status. These operations belong in the wrapper.
It needs no IQuadooKeyboardInput exemption: its keys use normal button routing.

### 4.3 Appearance and layout

Render and verify normal, hot, pressed, disabled, focused and default states.
Use native theme content margins where available, a classic pressed-content
offset where appropriate, and the standard focus cue. Respect WM_QUERYUISTATE
for focus rectangles and mnemonic underlining. Keep the real HWND caption for
native accessibility and mnemonic processing.

Measure the caption with the current Font. Center the image/caption group
horizontally and vertically inside the usable content rectangle. Left places
the image cell before the single-line caption; Top stacks it above the caption.
With an empty caption, center the image and omit the gap. Constrain the cell to
available space, preserve image aspect ratio, ellipsize a long caption, and
clip all output to the button. Very small bounds must never produce negative
rectangles or paint beyond the control.

Use native theme/system disabled text colors and resolve Quadoo colors only
when a COLORREF is needed. Follow the existing Button's color policy; themed
button chrome remains controlled by the active Windows theme.

Paint through a small offscreen buffer and copy the finished result once.
Preserve/restore the HDC state. Handle WM_PRINTCLIENT and parent printing so
designer buffering, previews and tests receive the same rendering. Repaint on
image, caption, font, enabled state, focus, hot/pressed/default state, bounds,
DPI, theme, system-color and UI-cue changes. Close/reopen theme handles as needed.

## 5. Image handling, automatic grayscale and lifetime

Both Picture values use the existing generic resource editor. PNG and ICO remain
self-contained embedded resources; JPEG/BMP import follows the current conversion
to PNG. Retain complete ICO containers and select an appropriate entry for the
physical glyph size. Do not store file paths as runtime dependencies.

Reuse the existing 8 MiB encoded, 4096-dimension, four-million-decoded-pixel and
render-work limits for each resource. Stage decoding and any required derived
data before committing a property replacement. Bad input or allocation failure
preserves the previous JSON value, usable resources and appearance. Clearing is
an ordinary nullable Picture edit.

Add a reusable private grayscale raster operation under Imaging. For normal
alpha images, convert premultiplied color channels using deterministic luminance,
for example `(77 * R + 150 * G + 29 * B + 128) >> 8`, retaining alpha exactly.
Apply the same resulting value to all three color channels. Zero-alpha pixels
remain transparent, and the premultiplied channel <= alpha invariant remains
valid. Do not implement this by changing opacity or drawing a gray overlay.

Select the requested ICO resolution and resample before deriving the disabled
raster. Never modify the stored normal image or replace an ICO with a flattened
generated PNG. The derived image exists only in a bounded runtime cache; it is
not a value for DisabledPicture and is never serialized.

The current RenderPixels helper uses fixed 96-DPI/HighQuality rasterization and
rejects destination-dependent XOR ICO output. It cannot simply be called as-is
for all button images. Extend the private imaging path to accept the requested
DPI/interpolation and generate the grayscale variant without losing ICO entry
selection. Keep existing callers' defaults unchanged.

For destination-dependent legacy XOR pixels, use the existing background-aware
compositor and a coverage mask: compose onto the already-drawn button face,
then grayscale only affected image pixels. Leave transparent/background pixels
untouched. This exceptional path must not create a gray rectangular patch or
silently discard inversion. Its tests must use colored backgrounds as well as
the classic gray face. Background/theme/state changes invalidate this cache.

Keep normal and disabled rendering caches bounded and independent enough that
repeated enable/disable does not repeatedly decode the source. Cache keys cover
source generation, physical output size, DPI, interpolation and transformation;
the XOR path additionally depends on the current composed background. An
explicit DisabledPicture takes precedence and is never automatically grayscaled.

All sizes and Spacing persist in logical 96-DPI units. Apply MulDiv scaling only
at the native drawing boundary, including initial creation. Do not write scaled
dimensions back to JSON. Test 96, 144 and 192 DPI and a live DPI transition.

Release decoded resources, derived rasters, buffers, themes and GDI handles on
replacement/clear/destruction, including after failed construction or external
HWND destruction. Native drawing may occur during construction, before the child
is registered; stage resources before exposing the HWND and cover that phase in
tests. Image changes and repainting never raise OnClick.

## 6. Designer, Studio and scripts

The registered factory automatically makes ImageButton available to both hosts.
Use the existing Picture editor for Picture and DisabledPicture, and the existing
inline enum editor for ImagePosition and Interpolation. Property edits use the
normal document/source transaction and selection-property abstraction. Do not
special-case the control in the inspector or source generator.

Keep exactly one generated script member for the ImageButton. Caption, picture
objects, orientation, sizes and Enabled use the existing descriptor-driven script
property path; Click uses IQuadooButton. Verify these paths in AQVMT without
changing QuadooScript. Preview must use the same renderer and preserve existing
modal-button behavior while suppressing application callbacks as it does today.

Create `Icons/ImageButton.png` and `Icons/ImageButton.ico`, each with a transparent
20 x 20 palette image. Use the existing authored icon style: a recognizable
push-button outline containing a small colored picture and short caption marks.
It must be distinct from Button and Image at actual size. The ICO uses the
existing XP-compatible uncompressed DIB plus mask convention.

Extend GenerateIcons.py, allocate a palette resource ID, include the ICON in
QuadooForm.dll, and set the factory's palette resource through IQuadooPaletteIcon.
Update relevant project/resource dependency lists and the palette preview. Both
Studio and the standalone designer obtain it through the provider; neither host
adds an ImageButton-specific icon-loading branch. Artwork creation is part of
implementation, not this planning pass.

Illustrative definition; the Base64 placeholder is documentation, not fixture data:

```json
{
  "type": "ImageButton",
  "name": "SaveButton",
  "properties": {
    "Width": 112,
    "Height": 56,
    "Caption": "&Save",
    "Picture": { "format": "png", "encoding": "base64", "data": "<PNG bytes>" },
    "DisabledPicture": null,
    "ImagePosition": "Top",
    "ImageWidth": 20,
    "ImageHeight": 20,
    "Spacing": 4,
    "Interpolation": "HighQuality"
  },
  "events": { "OnClick": "SaveButton_Click" }
}
```

## 7. Implementation sequence

1. **Button presentation and native drawing plumbing.** Add the optional
   default-presentation interface and implementations, generic WM_DRAWITEM
   forwarding, and focused tests using an independently implemented presentation
   provider. Verify ordinary Button behavior stays unchanged. Establish the
   themed/classic owner-drawn BUTTON path in the existing harness; do not create
   another designer or executable just for the control.
2. **ImageButton runtime and grayscale.** Register CImageButton and metadata;
   implement embedded images, layout, DPI, state painting, transactional resource
   replacement, grayscale/XOR handling, printing and cleanup. Add focused native
   and private tests, including image failure injection and JSON round trips.
3. **Designer, artwork and script integration.** Exercise both property editors,
   selection/multiple selection, clipboard and source synchronization; create
   the 20 x 20 icon and add resources/project dependencies. Add a sample form
   showing both orientations, a custom disabled image and automatic grayscale.
   Verify script properties, Click, Enabled and ModalResult through AQVMT.
4. **Validation and delivery.** Build affected projects with VS2008 in Win32
   Debug and Release. Run focused image-button tests, related image/keyboard/menu
   suites, full native/private/script suites and focused Designer/Studio tests.
   Run broad GUI suites where the environment permits, reporting existing
   fixture failures separately. Validate classic and themed XP behavior on the
   actual VM; local modern-Windows tests do not substitute for that result.
   For the scheduled October 27 implementation, run only the audited headless
   ImageButton path and compile-only script path. All native-HWND, GUI and VM
   execution remains deferred until the user can test interactively.

## 8. Acceptance checks

- Left/Top layouts, empty images/captions, different image aspect ratios, long
  captions, small bounds and stable caption position across enable/disable.
- Native mouse and Space activation, mnemonic, Tab traversal, focused/default
  appearance, Enter/Escape through nested containers/Frames, PopupMenu, exactly
  one OnClick and unchanged ModalResult completion/cancellation semantics.
- Grayscale color-channel equality and unchanged alpha for opaque and translucent
  PNGs; custom disabled-image precedence; multi-resolution and legacy ICO cases;
  no background patches, transparency halos or source-byte changes.
- Visible updates in design mode, Preview and application runtime; no application
  callbacks from designer painting or property edits; parent disable/re-enable
  chooses the correct image even if the child's logical Enabled value is true.
- Logical JSON dimensions at different DPIs, correct initial physical image
  size, repeated load/save, clipboard copy/paste, stable IDs, and no extra script
  members for pictures or presentation state.
- Failed imports, decode/cache allocation failures, observer/source-transaction
  rejection and failed native creation leave prior observable state intact.
- Canonical COM identity, proper QueryInterface capabilities and factory lifetime;
  independent containers/presentation providers use only published interfaces.
- Repeated replacement, toggling, resizing, theme/DPI changes and destruction
  return GDI/USER counts and factory live counts to the established baseline.
- Palette icon is a distinct, transparent 20 x 20 image returned with normal
  ownership semantics from QuadooForm.dll in both hosts.

## 9. Decisions and scope

Recommended decisions are fully specified above: ImageButton naming,
Picture/DisabledPicture properties, Left/Top ImagePosition, a 16 x 16 logical
image cell, 4-pixel spacing, automatic grayscale, and themed owner drawing with
classic fallback. No further product decisions are required to begin.

The optional default-presentation interface is the only proposed public ABI
addition; existing IIDs, vtables, structures, exports and ordinals remain intact.
Ordinary image configuration stays discoverable through property descriptors.

This pass does not add image-list references, hot/pressed image properties,
TBitBtn Kind presets, sprite-strip/NumGlyphs import, split/dropdown buttons,
toolbar buttons, or a new image editor. It preserves VS2008 and Windows XP
compatibility, repository formatting, custom containers and error handling,
and introduces no STL, exceptions or QuadooScript dependency in QuadooForm.dll.
Do not rebuild SimbeyCore or the script compiler/VM dependency DLLs during
implementation validation.
