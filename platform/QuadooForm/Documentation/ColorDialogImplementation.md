# ColorDialog and the Quadoo color picker

The picker, its form JSON, and its 16/32px DIB form icon now belong to
QuadooForm.dll. There is no QuadooFormDesigner.dll or DesignerDialogs project.
Both the standalone designer and Quadoo Studio use the same runtime entry point.

## Native API and component

`ShowQuadooColorPicker(HWND, COLORREF, COLORREF*)` is exported at ordinal 16.
S_OK returns the accepted raw Quadoo Color; S_FALSE means Cancel, Escape, or
caption close. Cancellation/errors preserve the output. An optional owner must
belong to the calling thread and is normalized to its top-level window.

The built-in nonvisual `ColorDialog` factory exposes `IQuadooColorDialog`
(IID ending 0035), whose `Execute(HWND, BOOL*)` follows the existing file-dialog
contract. Its serializable `Color` property has type QuadooProperty::Color and
defaults to black. A NULL owner uses the component's form/frame root window.
Execution is prohibited in Design/Preview mode, after destruction, on another
thread, or recursively. The component has inherited OnCreate/OnDestroy events
and OnAccept/OnCancel events. Acceptance commits Color before OnAccept and
notifies property observers only when it changes. Callback/observer failures
restore the preceding Color; cancellation preserves it. Retained interfaces keep
the factory alive, including after Destroy, until the last reference is released.

Existing public interfaces and export ordinals are unchanged. The new interface
and export are additive. `Icons/ColorDialog.ico` is an XP-compatible 20x20 DIB
palette icon, obtained through the same IQuadooPaletteIcon factory contract as
all other visual/nonvisual components.

## Form resource

`QuadooForm/Dialogs/ColorPicker.qform.json` is RCDATA IDR_COLOR_PICKER (201).
IDI_COLOR_PICKER (202) is also a normal icon resource. JSON embeds the same ICO;
the existing SimbeyCore Base64 path loads it. The fixed-border form has centered
OK/Cancel buttons, a scrolling content area, and an editable current-color
swatch that opens the standard RGB chooser. Hex entry accepts #RRGGBB (or six
unprefixed digits) and 0x800000nn for canonical system colors. Invalid input
disables OK without replacing the current color. Selecting a palette swatch
recovers from invalid input. The description retains the full system name.

All 141 Vista swatches are authored into the form definition with 18x18 logical
bounds in 21 columns and 7 rows. The smaller current-color preview and its
text fields sit to the right of the grid. System categories have an eight-pixel
gap between rows. Neutral shades run light to dark, followed by hue, then brightness within
each hue. Aliases are retained. Each of the six system categories is one Fixed
ColorSwatch containing that category's colors and labels. Form labels omit
COLOR_; ColorNames.h retains the complete canonical names. Long cell labels use
the control's existing ellipsis behavior; the selected color's full name appears
above the categories. Raw system references survive selection and serialization.

GenerateAssets.py is an optional authoring tool: it reads VistaColors.cpp as a
reference and writes the form and test fixtures. Neither production nor test
binaries include/link VistaColors.cpp or generate palette controls at runtime.
Normal VC9 builds require no Python/Pillow. Explicit resource dependencies track
the JSON and every ICO. The resource can be opened directly in the designer.

## Scrolling and validation

Native wheel, line, and page requests clamp negative positions to zero before
calling SetScrollPosition; UpdateScrollbars already clamps the upper boundary.
The public API still rejects explicitly negative positions. Boundary input no
longer records E_INVALIDARG or aborts the modal dialog.

The native suite checks metadata, COM identity, thread affinity, JSON round trips,
design/preview suppression, destruction, and factory retention. Designer tests
add palette/tray placement, property editing, persistence, and deletion. Focused
`QuadooFormDesignerTests.exe --picker-only` exercises all palette entries, hex,
modal/keyboard routing, nested chooser cancellation, wheel/scroll boundaries,
component acceptance/cancel, recursive execution, callback/observer rollback,
NULL-owner resolution, destruction from OnAccept, and exact HWND/HICON cleanup
plus repeated-cycle GDI accounting. `--show-picker` opens a manual preview.
