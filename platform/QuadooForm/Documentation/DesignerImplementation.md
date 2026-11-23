# Designer implementation contract

Phases A and B only. Quadoo Studio, QuadooScript, clipboard/reparenting, undo/redo,
ListView and image-list controls remain outside this work.

## Verified prerequisites and additive ABI

The existing public interfaces already provide properties/events, immutable GUIDs,
factory construction, design-mode child HWND hosting, layout, serialization and
`IQuadooForm::ShowModal(LRESULT*)`. Runtime/Design/Preview suppress application
callbacks appropriately; Preview currently uses child styles and needs top-level
Form creation. Form styles/icons/scrolling and Button.ModalResult are missing.

Add optional interfaces without changing existing vtables: design capabilities
(movement/resize axes), factory catalog enumeration, factory placement eligibility,
and modal completion. New icon-value helper exports create validated ICO JSON and
load an owned HICON from that same representation. Existing exports retain ordinals.
Factory/instance descriptor indexes remain type-local. Ordinary Form and Button
property indexes are preserved and additions follow them; the combined collection
places events after ordinary properties, so event-property indexes shift. Consumers
must resolve descriptors by name rather than cache numeric indexes across versions.
External controls need no private implementation base. Existing vtables, structure
layouts, interface IIDs and export ordinals 5 through 11 are unchanged. New optional
IIDs end 0025 (design capabilities), 0026 (modal completion), 0027 (factory catalog),
and 0028 (design placement). Icon helper exports use new ordinals 12 and 13.

Form defaults preserve current behavior: BorderStyle=Sizeable, BorderIcons=15
(SystemMenu/Minimize/Maximize/Close), WindowState=Normal, FormStyle=Normal,
AutoScroll=false, Icon=null. Button.ModalResult is a serializable Long, default 0;
Form.ModalResult remains runtime-only Long. OnClick precedes modal completion;
cancelled close restores the previous pending result. Nonmodal clicks do not close.

Icon JSON is null or `{ "format":"ico", "encoding":"base64", "data":"..." }`.
SimbeyCore ScEncodeBase64A/ScDecodeBase64A perform encoding/decoding. Accept bounded,
validated DIB ICO images supported on XP; reject PNG/compressed images in this pass.
Preserve the complete ICO container and all images in JSON. Returned HICONs are
owned by the caller; runtime owns both native Form icon sizes.

## File utilities and encoding

Verified CChooseFile::Initialize(), OpenSingleFile(HWND, title, filter),
SaveFile(HWND, title, initialName, filter), and GetFile(0).
Verified Text::LoadFromFile(PCWSTR,PWSTR*,INT*,UINT) and
Text::SaveToFile(PCWSTR,INT,PCWSTR). Text buffers use __delete_array.
Save writes UTF-8 with BOM; load uses CP_UTF8 for BOM-less input and recognizes
UTF-8/UTF-16 BOMs. Extensions: .qform.json and .qframe.json, with generic JSON
accepted by Open. Saves stage a temporary file beside the destination before
replacement; document path/dirty state commit only after success.

Forms and Frames appear only under File > New, never in the palette. The editor
does not add checks or special handling for nested Form/Frame definitions.

## Editor architecture

A reusable native editor component owns the document, design child host, artificial
Form chrome, tree/palette/inspector and interaction overlay. The standalone program
owns menus, status bar, file dialogs, dirty prompts, layout preferences and Preview.
Persist geometry in 96-DPI units, independently of canvas zoom and workspace scroll.
Use public runtime interfaces throughout; selection tracks GUID identity. Preview
clones current unsaved JSON and uses an independent top-level modal Preview Form.

Document loads build an independent hidden candidate session before replacing the
current one. Property edits retain old values and a serialized fallback; bounds
edits do likewise. Invalid name, property, placement and load tests verify rollback.
Tree selection of an inactive page is view state, tracked separately by page GUID;
serialization restores the configured active page and accounts for deletion/reindexing.
Observers retain a detachable proxy rather than the editor, and session teardown
unsubscribes observers before destroying controls. The independent test DLL exercises
custom descriptors/events without any runtime implementation inheritance.

The interaction window receives design input and paints after hosted native HWNDs,
keeping all eight grips visible. Gestures commit requested-bound deltas only at
mouse-up; clicking a docked control cannot copy its effective layout into JSON.
Snapshots, status, inspector, tree and canvas all derive from public control state.

The inspector uses one reusable subclassed EDIT child over a list-view value cell
for single-line String/Event values. Drafts retain the component GUID and descriptor
name, so refreshes and selection changes cannot redirect an edit to another
component or property index. External refreshes are deferred until the draft ends.
The host delegates inline keyboard handling before its component accelerators.
Memo descriptors and pre-existing multiline strings use the existing dialog editor.

TabOrder.cpp enumerates public direct-child properties, stably sorts their saved
values, and uses the same metadata to append newly placed controls. The modal
reorder list keeps its changes local until OK. Apply validates an exact permutation,
checks container membership in the current document, and batches layout while
assigning consecutive values. Failed setters/layout restore all original values
and dirty state, with a serialized document fallback if restoration fails. Neither
this path nor the dialog changes child registration order or uses private runtime
classes.

The workspace uses XP-compatible child-window compositing to buffer native controls
and the transparent selection overlay together. Designer background/chrome painting
uses a compatible memory DC/bitmap and one final blit; all temporary GDI objects are
released after painting. The inspector retains its separate native list-view buffer.
Tree/inspector rebuilds suppress intermediate redraws. Dragging queues invalidations
instead of forcing synchronous background/native/overlay paints per mouse message.
Pane layout is deferred as a batch with sibling clipping and SWP_NOCOPYBITS, followed
by full invalidation. The editor paints exposed splitter/background areas explicitly,
preventing old inspector text from surviving maximize/restore. Zoom is read directly
from the editor into a dedicated status pane, including during Preview/error status.

IconImport.cpp accepts ICO or PNG file bytes through one content-based import
path. PNG pixels become bottom-up 32-bit BGRA DIBs with an AND transparency mask
and ordinary ICO directory entries. Area averaging operates on premultiplied
colors before writing straight-alpha ICO pixels, avoiding dark/colored fringes.
The existing QuadooCreateIconValue helper validates and Base64-encodes the complete
ICO through SimbeyCore. The dialog stages both JSON and HICON before replacing its
preview; failed imports preserve the previous icon and Cancel never edits the form.

PngDecode.cpp compiles the repository's LodePNG decoder in a private namespace
with bounded Win32 heap allocation and no encoder or ancillary text handling. It checks image bounds
before decoding and validates exact inflated scanline lengths, including Adam7
padding, before the legacy decoder's pixel conversion. It also checks the complete
33-byte IHDR before the bundled inspect routine reads its CRC. No shared third-party
source, runtime ABI, JSON format, or runtime DLL dependency changes are required.

See ComponentExtensibility.md for the subsequent nonvisual/placeholder milestone.
Root component stores, component factories, lossless design fallback and Timer
are additive contracts. The inspector and selection model now operate on components;
canvas geometry continues to require actual visual interfaces. Tray symbols never
enter visual layout or runtime child arrays. Missing/explicitly unsupported factories
can be represented in a candidate session; genuine errors still reject the candidate.
