# QuadooForm Designer

The Phase A/B standalone editor uses the native QuadooForm runtime and an
embeddable VC9 C++ editor library. No Quadoo Studio or QuadooScript installation
is required. The runtime supplies visual-control and nonvisual-component
factories, including ImageList. Component/API details are in
[ComponentExtensibility.md](ComponentExtensibility.md).

Image is a windowless visual control with embedded PNG/ICO resources, JPEG/BMP
import, sizing/alignment/filtering options, alpha and opacity. Its Picture editor
provides Browse, Clear and a thumbnail. See [ImageControlImplementation.md](ImageControlImplementation.md)
for defaults, limits and validation, and open `DesignerExamples/ImageGallery.qform.json`
to try the supported display modes.

ImageList is a nonvisual collection of embedded BMP/ICO/PNG imports normalized
to PNG sources. Double-click it in the component tray or edit its Images property
to open the indexed thumbnail editor. Add/replace, strip import, drag reordering,
cell sizing, color depth, and Apply/Cancel use a temporary collection. See
[ImageListImplementation.md](ImageListImplementation.md) and
`DesignerExamples/ImageLists.qform.json`. Its focused tests run with
`test-imagelist.ps1`; they do not run the existing validation suites.

## Build and start

Use the existing JSON and SimbeyCore dependencies in the chosen configuration,
then run from the repository root (rebuild those dependencies only if absent):

```bat
tests\platform\QuadooForm\build-designer.cmd Debug --test
tests\platform\QuadooForm\build-designer.cmd Release --test
target\Debug\QuadooFormDesigner.exe
target\Debug\QuadooFormDesigner.exe platform\QuadooForm\DesignerExamples\Settings.qform.json
```

`QuadooForm.sln` includes the runtime, harness, independent test DLL, private
tests, designer core, standalone host and designer tests. The designer projects
are Win32, use Visual Studio 2008 / SDK 6.0A, and disable C++ exceptions. Runtime
dependencies beside the executable are QuadooForm.dll,
JSON.dll and SimbeyCore.dll,
plus the VC9 CRT. `QuadooFormDesignerCore.lib` is in `target\lib\Debug` or
`target\lib\Release`. The script copies the example definitions into the output
directory's `DesignerExamples` folder. Validation details and the local Release
dependency staging directory are described in `DesignerValidation.md`.

## Editing a definition

File > New creates a Form or a Frame. Forms display a simulated caption and
border outside their stored client rectangle; Frames display their client area.
Forms and Frames are absent from the toolbox.

Choose a visual Toolbox entry, then click or drag a rectangle on the workspace. Placement
uses the nearest compatible logical container under the pointer. To build tabs,
place a TabControl, add TabPage children to it, then place controls inside a page.
Select inactive pages or their descendants in Objects to expose them. This view
selection does not change the saved ActivePageIndex; edit that property explicitly
to change the initial runtime page.

Click a visual control or select its Objects entry. Drag its body to move it in the
same container; drag its eight grips to resize it. Grey grips identify unavailable
axes, including ComboBox height, AutoSize dimensions and docked stretch axes.
Min/max bounds apply. Root grips change client dimensions; the simulated window
frame is excluded. Workspace scroll, zoom, splitters and selection are editor
state. View offers 50/75/100/150/200% zoom, grid, optional 8-unit snap and pane reset.
The current zoom is always shown in its own status-bar field. Commands are in the
standard menus; there is no toolbar below the menu bar.
Hold Shift while clicking to add sibling controls to the selection. A Shift-click
in another container leaves the selection unchanged. A plain click selects only
that control; dragging an already selected control moves the entire group. Ctrl
plus an arrow key nudges the whole selection by one logical pixel. Group movement
preserves relative spacing, including when grid snapping is enabled. Containers
move with their children; their children's relative coordinates stay unchanged.

The inspector shows compatible properties and events shared by the selection.
Different values appear as `(multiple values)`; accepting a new value applies it
to every selected control in one transaction. Name is read-only for groups because
component names must remain unique. Resize grips and specialized collection/menu/tab
order editors operate on a single selection.

Cut, Copy and Delete operate on the entire selection. Groups use a JSON clipboard
array in creation order, including each selected container's children. Paste
assigns fresh identities and unique names and selects the inserted siblings.
With a group selected, paste targets its common parent container.

Double-click a Properties value, or select its row and press F2 or Enter, to edit
it. Single-line String properties (including Name, Caption and Text) and event
handler names edit directly in the value column. Enter commits, Escape cancels,
and Tab/Shift+Tab commit and advance between inline fields. Leaving the field
commits before changing selection, saving or opening another document. Rejected
values remain available for correction or Escape; failed edits do not dirty the
document. Delete edits text while a field has focus. F4 opens the dialog editor.
Memo descriptors always use a multiline dialog; existing String values containing
line breaks also retain that editor, preserving older multiline content.

Color value cells show a swatch beside the stored color. Defaults retain system
color references, so they resolve against the current Windows colors when drawn.
The Quadoo Form color picker offers an 18x18 Vista palette, categorized system
colors, an editable preview and hex entry. Click the preview to open the standard
RGB picker. Hex accepts `#RRGGBB` or a raw `0x800000nn` system color. Cancel preserves the
original reference and OK stores the selected RGB or system-color reference. Newly placed visual controls
copy BackColor and ForeColor from their immediate container. Later parent edits
do not rewrite existing children. See [QuadooColorsImplementation.md](QuadooColorsImplementation.md)
for the encoded value format and the native ResolveQuadooColor helper.

Booleans and enum choices use lists;
flags use checkboxes; fonts use the system dialog and colors use the Quadoo Form
picker. Items use a JSON array
of strings, preserving empty and duplicate entries; initial values come from the
existing IRStringArray snapshot API. Compound values use JSON. Runtime-only and
read-only entries cannot be edited. The Events tab edits saved handler names;
standalone Design and Preview suppress application callbacks.

New controls with TabOrder receive one more than the highest value among their
direct siblings, starting at zero in an empty container. Panels, GroupBoxes,
TabPages and other containers each have their own independent child sequence.
Existing values are retained when opening a document or deleting a control.
Form > Tab Order opens the selected container, or the nearest container parent
of a selected non-container. Its list contains only direct children that expose
an editable TabOrder. Move Up/Move Down rearrange the list; OK assigns consecutive
values starting at zero. Cancel leaves the document untouched. Reordering changes
TabOrder and native tab traversal while preserving JSON child registration order.

Form.Icon opens a browse/preview/remove dialog. Choose an uncompressed DIB ICO
file (1 MiB maximum, up to 64 images, 1/4/8/24/32-bit) or a PNG image. PNGs are
decoded with the bundled LodePNG library and converted entirely in memory to
32-bit DIB ICO images, preserving transparency. Rectangular images are centered
on a transparent square; larger images are reduced proportionally to at most
256 x 256. Smaller 16/32/48-pixel representations are included when the source is
large enough, using alpha-aware area averaging. Source PNG limits are 8 MiB,
4096 pixels per dimension, and 4,194,304 total pixels. Invalid or unsupported
images show an error and preserve the current icon. Cancel leaves the form alone.

The resulting ICO, including all sizes, is embedded as Base64 using SimbeyCore.
The original file is unnecessary afterward. Removing the icon saves null and
uses the OS fallback. The runtime's existing ICO-only JSON contract is unchanged;
ICO files whose internal image payload is PNG-compressed remain unsupported on
the XP-compatible runtime. Select the source PNG to convert it to supported ICO.

Save and Save As write UTF-8 with BOM. Open also recognizes UTF-8/UTF-16 BOMs and
treats BOM-less text as UTF-8. Suggested extensions are `.qform.json` and
`.qframe.json`; ordinary `.json` is accepted. Input is limited to 8 MiB. Failed
loads keep the current document; failed saves retain path and dirty state. A
temporary file beside the destination protects the previous file until the
replacement succeeds. New/Open/Exit prompt Save / Don't Save / Cancel for edits.

F5 or Form > Preview creates an independent, genuine modal Form from current
in-memory JSON. It accepts native input and built-in behavior, including item
selection, scrolling and Button.ModalResult. Closing it restores the editor and
reports its result/HRESULT in the status bar. Preview is disabled for Frame roots.
The Settings sample has OK/Cancel buttons; Profile.qframe.json is a Frame example.

## Embedding and factory contract

`Designer/Designer.h` is an internal C++ API, not a new DLL ABI. A host owns a
`QFD::CDesignerEditor`, calls Create(parent, optionalRegistry), sizes its child HWND,
and uses NewDocument/OpenDocument/SaveDocument, selection/property operations and
PreviewForm. Hold a reference until the child window is destroyed, then release
it. The editor posts `QFD::ChangedMessage` to its parent for title/status/menu
updates. The standalone host owns file dialogs, prompts, preferences and menus.
SetDPI relays a host scale; zoom remains separate from stored 96-DPI geometry.

Factories are enumerated through optional IQuadooFactoryCatalog. Optional
IQuadooDesignFactory advertises parent eligibility; ordinary factory construction
remains authoritative and failed creation rolls back. Optional
IQuadooDesignCapabilities advertises movement/resize axes; absent capabilities
are conservative (no gesture editing), while descriptor property editing remains
available. Independent painted controls and HWND controls use the same public
tree, property, event, layout and serializer paths. No private runtime classes are
included by the designer. The automated suite uses the independent ExternalMeter
DLL to verify this contract.

External factories honor Design mode and suppress application callbacks. Missing
factories now produce lossless design representations. A registered factory can
explicitly return EDesignUnsupported after rollback to request a typed representation;
other failures still preserve the current document. Visual representations have
dashed borders and reason tooltips/status; missing nonvisual objects stay in the
tray. Original types, GUIDs, events, nested definitions and unknown fields survive
saving. Reopen after registering a factory to resolve it. Preview requires the real
factories and displays a construction diagnostic if one is missing or fails.

The Nonvisual Components toolbox category creates OpenFile, SaveFile or Timer in
the Form/Frame root, independent of the selected container. Select the resulting
tray glyph or Objects > Components entry to edit properties/events. The tray scrolls
separately and has no runtime bounds, resize grips or tab order. Selection never
executes dialogs. OpenFile/SaveFile expose Title, Filter (VCL `label|pattern` pairs),
FilterIndex, InitialDir, DefaultExt, FileName, Options, OnAccept and OnCancel.
Timer exposes Enabled (default true), Interval (default 1000 milliseconds) and
OnTimer. Interval zero disables ticking. Events bind before root initialization;
Design and Preview do not tick or invoke application handlers.

For a native interactive example with real file pickers and a ticking label, run
`QuadooFormHarness.exe --show-components`. Selecting a save path performs no file
I/O. The JSON-only dialog example is DesignerExamples/Dialogs.qform.json.

## Keyboard and Memo

In the designer, Ctrl+Left/Right/Up/Down moves the selected visual control by one
design pixel. This works on the canvas and in the component tree, including
Studio's shared tree. Nudging bypasses grid snapping and is independent of zoom
and DPI. Docked controls and other controls that disallow movement retain their
positions. Property text editors, the palette and source editors keep their usual
arrow-key behavior. Moves update the inspector, status and document dirty state
through the same bounds transaction used for mouse gestures.

Form exposes KeyPreview (default false), and Button exposes independent Default
and Cancel flags (both false). Memo is a multiline EDIT in Visual Controls, with
Text, ReadOnly, WordWrap, ScrollBars and WantTabs. Its Text uses the multiline
property editor; it permits horizontal and vertical resizing. In Preview, Enter
edits a Memo, Tab navigates by default, and Enter/Escape activate eligible buttons
through the existing ModalResult behavior. The sample
DesignerExamples/KeyboardMemo.qform.json exercises these paths without scripts.
See KeyboardAndMemoImplementation.md for host keyboard routing and precedence.

## Palette icons

The Toolbox and component tray use icons from the same factory-level
IQuadooPaletteIcon contract for built-in and external types. The source artwork
is 20x20 under Icons/; built-in resources are embedded in QuadooForm.dll. Both
views share one editor-owned image list, scaled with UI DPI independently of
canvas zoom. There are no palette divider rows. See
[PaletteIconsImplementation.md](PaletteIconsImplementation.md) for resource,
ownership, input and validation details.

## ColorSwatch

The windowless ColorSwatch control supports Fixed palettes, editable single colors,
selection highlighting and optional labels. Colors and Labels use the existing
JSON collection editors; Mode and Selected are ordinary inspector properties.
Open DesignerExamples/ColorSwatches.qform.json to try it in Preview. See
[ColorSwatchImplementation.md](ColorSwatchImplementation.md) for property and event
semantics, snapshot ownership and validation results.

## Scope and follow-up

This milestone has single selection, same-parent gestures, native property
dialogs and modal Preview. Undo/redo, clipboard/reconstructive moves, multi-select,
source generation, Studio integration and additional controls remain deferred.
System/theme chrome is best effort; Preview is the authoritative OS appearance.
The host is system-DPI aware; real per-monitor/theme changes and Windows XP need
manual follow-up. Unknown design metadata is preserved where the runtime's Design
template supports it; independent factories must advertise persisted values.
