# Standalone designer validation

Phase A milestone (2026-10-15): VS2008 Win32 Debug and Release runtime, harness,
independent extension DLL and private native tests compile with zero warnings/errors.
Both native harness configurations pass 6,816 assertions across 23 groups; private
tests pass 83 assertions. Release tests run in target\QuadooFormDesignerValidation
beside the previously rebuilt corrected SimbeyCore.dll, avoiding the locked normal
Release dependency. No running application was stopped or replaced.

New runtime coverage includes complete factory enumeration, optional placement and
movement/resize metadata, both images of a DIB ICO container, owned native Form icons,
invalid icon-value rollback, icon clearing and deterministic persistence, fixed and
sizable borders with preserved client size, button flags and topmost style, shared
Form scrolling and focus scrolling, stored-coordinate preservation at 96/144/192 DPI,
modal buttons in nested containers, cancelled close restoring the result, mrNone,
nonmodal clicks, independent top-level Preview, and suppressed Preview callbacks.
The existing capability/COM identity suite includes the new optional interfaces.

The build script now propagates negative native process exit codes as failures.
Synthetic painted-control input uses IQuadooChildWindowHost, matching real input
delivery to the shared scrolling content HWND.

Windows XP and physical theme/monitor transitions require follow-up on actual hosts;
XP validation was not performed in this session. The compiler is VC9 with SDK 6.0A. Icon loading
copies selected RT_ICON bits into aligned storage before CreateIconFromResourceEx,
and accepts DIB rather than PNG ICO images for XP compatibility.

## Phase B validation

All seven QuadooForm runtime, harness, extension, private-test, designer-core,
standalone-host and designer-test projects received clean Win32 Debug and Release
rebuilds with zero warnings and errors. Subsequent focused changes also compiled
cleanly. Final runtime suites pass 6,837 assertions across 23 groups and 83 private
checks in each configuration. The independent designer suite passes 559 assertions
in each configuration (totals can vary with bounded resource warm-up). It tests:

- New Form/Frame, fresh root identity, nested placement, GUID-preserving Save/Open,
  Unicode text, handler names, embedded icon values and deterministic round trips.
- Invalid JSON and failed native factory construction preserve the prior document,
  tree and serialization. Duplicate-name edits roll back; failed saves retain dirty
  state/path. Both shipped example files are loaded by the actual editor.
- Tree/canvas selection, inactive-page activation without persisted activation or
  dirty-state changes, saved-page GUID/reindexing after deletion, rejected child
  types, and factory-driven nested TabPage placement.
- 96/144/192 DPI and 50/100/150/200% zoom without JSON geometry changes; all eight
  grip render/hit/drag paths at those zooms; disabled ComboBox grips, AutoSize and
  docking restrictions, minimum bounds, and no mutation from clicking docked content.
- Independent ExternalMeter DLL enumeration in the toolbox, hierarchy, custom
  Level editing, OnPulse handler persistence/recreation, layout and serialization.
  No private runtime inheritance or concrete casts are used.
- Independent top-level modal Preview from unsaved JSON, disabled/restored owner,
  ModalResult completion, destroyed Preview HWND, unchanged editor selection,
  geometry, serialization and dirty flag, and unsupported Frame Preview.
- Repeated editor/document/icon replacement and two Previews per lifecycle. Every
  captured HWND is destroyed; factory live counts and DLL object counts reach zero;
  the module can unregister/unload. Three identical settled samples within at most
  eight warm-up cycles establish a frozen GDI/USER baseline for 20 measured cycles.
  No allowance or measured-phase rebasing was added. Native failure-injection and
  resource-monitor rollback/leak tests also pass unchanged.

Manual GUI validation used the computer-use skill. New Form chrome, all eight
visible grips after placement/dragging, native Button caption defaults, the
descriptor ModalResult dialog, unsaved modal Preview returning IDOK, Save As, New
Frame without artificial chrome and disabled Preview were demonstrated. The
repaint-order correction was verified visually, not just by WM_PRINTCLIENT tests.
The host's initial size is bounded to the desktop work area so the status bar fits.

Run interactive GUI inspection and GUI resource suites separately. An idle external
UI Automation inspection session initially caused late process-wide accessibility
adapter allocations. Disconnecting that session allowed both strict suites to pass;
resource limits were not weakened. The designer suite also explicitly initializes
MSAA accessibility while controls are alive during its bounded warm-up.

Release runs are in `target\QuadooFormDesignerValidation`, beside the corrected
Release SimbeyCore dependency copied from the earlier isolated build. The normal
Release SimbeyCore DLL remains locked by a user application and was not replaced.
This staging folder also contains the Release designer, tests and example files.

ABI review compared every existing interface declaration against Subversion BASE:
all 35 are unchanged. New optional IIDs end 0025-0028; two new exports use ordinals
12/13, with 5-11 preserved. Published-header include probes pass with QuadooVM in
either order using its normal Windows prerequisites. Import inspection confirms no
script dependency or direct imports of newer DPI APIs. All seven VC9 project source
paths exist. Source checks found no STL, C++ exception handling or SetParent calls
in the designer sources.

## Material follow-up

Windows XP execution and real physical DPI/theme changes were not performed in
this pass. Explicit DPI tests do not substitute for these. No VM was configured or
modified for testing. The standalone host is system-DPI aware, and chrome is a
best-effort system/theme simulation rather than exact DWM rendering.

A factory that cannot construct safely in Design mode currently fails load while
preserving the active document; automatic substitute placeholders are not supplied.
Factories can provide their own safe Design representation. External metadata not
advertised through public descriptors still requires factory/host persistence
support, as documented for the runtime. Optional movement/resize capabilities are
conservative when absent. Studio/source synchronization, undo/redo, clipboard,
multi-selection and additional controls remain outside the Phase B milestone.

## Toolbar and repaint follow-up

The toolbar and zoom combo were removed. View retains the zoom commands and a
dedicated status-bar field displays the current zoom. The editor begins directly
below the standard menu. No runtime/public ABI or document-format changes apply.

Pane placement is batched with sibling clipping and SWP_NOCOPYBITS; exposed editor
background is explicitly painted. The workspace buffers its native children and
transparent overlay with WS_EX_COMPOSITED, while the inspector keeps its own native
list-view buffer. Designer background/chrome uses a memory bitmap and final blit.
Tree/inspector rebuilds batch redraws and drag messages queue composited repainting.
Buffering the entire editor was visually checked and caused an inspector list-view
buffer conflict; the final implementation restricts child compositing to the workspace.

The expanded designer suite has 587 assertions in Debug and Release, including
eight alternating small/large pane layouts, cleared stale pixels at the inspector
boundary, unchanged document/selection/dirty state, and twenty drag messages
coalescing into at most two canvas paints after dispatch (none synchronously).
Resource cycles now resize the editor twice per workload in addition to Preview,
icon replacement and teardown, retaining the same frozen GDI/USER limits.

Manual GUI checks confirmed clean maximize/restore, no toolbar, current status zoom
after View changes, and visible native controls/grips after placement and dragging.
Both configurations build with VC9 and zero warnings/errors. Release tests use the
same isolated dependency folder described above. XP execution remains pending.

## Inline editing and container tab order

The inspector now edits single-line String properties and Event handler names in
the value column. Tests cover Unicode, Enter/Escape, Tab navigation, text Delete
without component deletion, committing before selection/tab changes, duplicate-name
rollback, retaining rejected drafts, repositioning on resize, event clearing, and
deferred external refresh on cancellation. F4 retains the dialog editor; existing
multiline String values also retain it without losing line breaks.

Memo is appended to the published property-type enum as 17. All existing enum
values, vtables, structure layouts, IIDs and exports are unchanged. Native tests
cover empty defaults, typed String access, Unicode/newlines, invalid-value rejection
and JSON reload. The independent public-interface test DLL advertises a Memo
Legend property; the designer edits it in the multiline dialog and verifies its
value and descriptor after serialization/reconstruction.

Tab-order coverage includes zero-based appends, independent Panel/GroupBox child
sequences, labels without TabOrder, next-after-highest values, rejected out-of-range
construction, selected-container/parent scope, nested and empty dialog lists,
Up/Down boundary state, OK/Cancel, stable registration order, invalid permutations,
stale document containers, JSON reload and a property-observer failure that occurs
after a setter has changed its value. Failed apply restores values and dirty state.

Win32 Debug and Release build with VC9 and zero warnings/errors. Full native suites
pass 6,837 assertions across 23 groups in each configuration; private metadata tests
pass 88. Designer tests pass 769 assertions per configuration, including repeated
inline EDIT creation, multiline Memo dialogs, tab-order dialogs, Preview and
complete native/extension teardown. Resource warm-up explicitly exercises the live
EDIT accessibility adapter on each bounded warm-up cycle and pumps while the UI
is alive, allowing asynchronous accessibility/font initialization to finish before
the baseline freezes. Dialogs paint before the test timer closes them. The eight
maximum warm-up cycles, frozen GDI/USER limits and 20 measured cycles are unchanged.

Manual UI verification demonstrated inline Caption entry and Enter commit, default
TabOrder 0/1 for new Buttons, Form > Tab Order, Move Down and OK updating the selected
Button's TabOrder. The saved verification document retains child registration order.
Manual observation and resource tests ran separately. Release validation remains in
the isolated folder with the corrected SimbeyCore dependency. XP execution was not
performed in this iteration.

## PNG icon importing

The icon dialog accepts PNG alongside ICO and converts PNG pixels entirely in
memory to uncompressed 32-bit ICO data. The existing runtime icon validator,
SimbeyCore Base64 encoding, JSON format and published ABI remain unchanged. The
decoder is namespaced within the designer to avoid symbol/allocator collisions
with an embedding host's own LodePNG build. Shared third-party sources are unchanged.

Both Win32 Debug and Release build with VS2008 and zero warnings/errors. The
focused `QuadooFormDesignerTests.exe --png-only` suite passes 106 assertions in
each configuration. It covers RGBA, palette/tRNS, grayscale, 16-bit samples,
Adam7, bottom-up BGRA and AND masks, native partial-alpha rendering, fully
transparent icons, proportional reduction, four ICO sizes, alpha-aware averaging,
existing ICO pass-through, truncated headers/chunks, invalid CRCs, short/long
inflated image data, input bounds, file loading, JSON reconstruction after deleting
the source PNG, and preservation of saved state on failed imports. Fixtures are
generated independently with Python struct/zlib and embedded in PngFixtures.h;
Python is not required to build or execute the native tests.

The shared temporary-file test helper now explicitly returns S_OK on success;
previously its uninitialized HRESULT could cause spurious Release test failures.

The complete native suites pass 6,837 assertions and private tests pass 88 in both
configurations. The final complete Debug designer run passes 927 assertions,
including Preview/resource teardown. Release passes its functional groups,
including the new PNG groups, but its existing strict resource group reports
late GDI growth from 24 to 25 objects, with USER counts unchanged. Earlier Debug
runs also reproduced this intermittent failure, as did the unchanged pre-PNG
Release binaries. Diagnostic runs are retained in
target/Designer-png-baseline-tests.log and the Debug/Release test logs. No resource
limits, warm-up bounds, measured cycles or leak checks were relaxed. The resource
workload now also imports PNG icons before Preview/teardown.

Manual computer-use checks confirmed the ICO/PNG file filter, successful conversion
of a 512 x 256 PNG, transparent icon preview, an explicit malformed-PNG error with
the previous preview intact, and application to the form title bar. The saved QA
form contains a Base64 ICO with four DIB representations, independent of the PNG
path. Release validation uses the existing isolated corrected SimbeyCore dependency.
Windows XP execution was not performed.

## Nonvisual components, placeholders and Timer (2026-10-16)

The VS2008 Win32 Debug and Release builds report zero errors/warnings and the
complete designer suite passes 1,750 assertions in each. This includes the existing document,
selection, zoom/layout, buffered painting, inline editing, tab order, PNG and
strict Preview/resource groups. No resource allowances, warm-up bounds or
measured cycles were relaxed. The earlier intermittent resource failure recorded
above did not occur in either configuration's final run for this milestone.

New checks cover Form/Frame nonvisual insertion, tray/tree/inspector selection,
Timer insertion through the actual toolbox command, string/event/property editing,
deterministic JSON, separate namespaces, and independent extension DLL components.
Missing and explicitly unsupported visual/nonvisual factories preserve original
GUIDs, events, unknown fields and nested subtrees; typed editing and reopening
after factory availability are verified. Genuine allocation/construction failures
preserve the active document. Missing-factory Preview fails without editing the
document. COM identity/reference balance covers both placeholder implementations.

Real OS chooser and designer/Preview checks are recorded in
ComponentExtensibility.md. Release binaries and dependencies are staged together
in target/QuadooFormDesignerValidation. Windows XP execution, physical DPI changes
and other OS/theme combinations remain untested. Logs are
target/Components-Debug-designer.log and target/Components-Release-designer.log.

## Active TabPage placement correction

The designer's hit test now checks a windowed control's actual native visibility
in addition to its requested Visible property. Inactive TabPages intentionally
retain Visible=true while their HWND is hidden. Previously their overlapping
bounds could win the reverse-order hit test, causing pointer selection or toolbox
placement to choose the inactive page and then activate it when selecting the new
control. Hidden native windows are now excluded before choosing a hit/parent;
nonwindowed controls retain their existing parent-host visibility and clip checks.

The native regression constructs a TabControl with two TabPages, clicks the first
header through the designer overlay, selects a toolbox entry and clicks/drags into
the visible page. It checks the active index, actual parent, child counts and new
selection. It also tests blank-page pointer selection, placement in the second
page and placement back in the first while the hidden page has a child. Both Form
and Frame documents run at 96 DPI, 144 DPI and 150% zoom, using Button and Label.
The focused mode is QuadooFormDesignerTests.exe --tabs-only (74 assertions).
The initial regression failed against the old hit test before applying the fix.
No runtime control behavior, public ABI or serialization format changed.

VS2008 Win32 Debug and Release builds complete with zero errors/warnings. Both
complete designer suites pass 1,824 assertions, including the unchanged strict
Preview/resource checks. Logs are target/TabPlacement-{Debug,Release}-{build,
designer}.log; the pre-fix failure is target/TabPlacement-before-fix.log. The
rebuilt standalone executables are in target/Debug and target/Release; the isolated
Release validation folder was updated too. Windows XP execution was not rerun.

## Keyboard properties and visual Memo

Form.KeyPreview and Button.Default/Cancel use descriptor-driven editing. Memo is
factory-enumerated under Visual Controls with multiline Text editing and both
resize axes. Designer tests insert Memo through the toolbox, exercise all grips,
edit its multiline Text, save/reload IDs/properties/events, and verify the stored
Form Visible=false stays separate from its visible design representation. Frame
has no KeyPreview property. Design-mode preprocessing suppresses application
behavior. Real modal Preview inserts a newline in Memo and completes through
Default from a single-line TextBox, even with unavailable script-handler names;
the design document remains unchanged.

The focused designer --keyboard-only suite passes 110 assertions. Complete Debug
and Release designer suites pass 1,934 each; builds report zero errors/warnings.
A Release run also reproduced the known late-GDI intermittency: warm-up
GDI=23/USER=5, then GDI=25/USER=5. A fresh unchanged run passed all checks. The
failure is target/KeyboardMemo-Release-designer-resource-failure.log and the pass
is target/KeyboardMemo-Release-designer.log. The older unchanged pre-PNG baseline
reproduced similar growth in target/Designer-png-baseline-tests.log. No resource
allowances, warm-up bounds or measured-cycle counts were changed. This resource
intermittency remains unresolved rather than being hidden by the passing rerun.

The new no-script sample is DesignerExamples/KeyboardMemo.qform.json. Host/API
requirements, native results and remaining XP/IME/layout testing limits are in
KeyboardAndMemoImplementation.md. SimbeyCore was reused unchanged in both builds.

## Icon Toolbox and shared component tray

Both panes are native list views using one editor-owned image list. All 18
built-in factories provide ordinary DLL icon resources through IQuadooPaletteIcon
(0034); the independent extension DLL provides distinct visual/nonvisual icons
through the same published interface. Existing 50 interface declarations and
export ordinals are unchanged. There is no legacy extension migration work.

Debug builds have zero errors/warnings and the final full designer suite passes
2,672 assertions. Release builds also have zero errors/warnings, and their new
palette test group passes before the later resource check described below. Both
focused --palette-only suites pass 740 assertions. The final full Release designer
run fails after 2,298 assertions at the final Preview resource check (GDI 27 to 28,
USER=5 unchanged). The
new --palette-only mode covers resource/handle ownership, COM identity, factory
thread affinity, shared indexes, input, selection/rename/delete, placeholders,
DPI staging/rollback and repeated DLL/editor teardown with 20 measured cycles.
Native keyboard selection and completed-click notifications are tested separately;
actual mouse/key gestures were checked in the live designer using computer-use.

Strict resource checks remain unchanged. A Debug designer run saw late GDI growth
from 26 to 28 with USER=40 unchanged; its fresh rerun passed. Release full runs
also saw late GDI growth from 26 to 28 with USER=5 unchanged. Release native runs
saw USER growth from 23 to 24 with GDI=19 unchanged in the choice-control group.
The final normal Release native run passed 7,769 assertions. An isolated build
of the pre-change Subversion runtime and extension reproduced
USER 23 to 24 in its earlier sizing/focus group (GDI=15 unchanged), confirming
that the USER increase also occurs without the palette implementation. The older
pre-PNG and keyboard/Memo logs already reproduced the late designer GDI increase.
These failures remain evidence, rather than being erased or accommodated by
larger thresholds/warm-up limits. Final run results are in the implementation note.

Logs are target/PaletteIcons-{Debug,Release}-{build,native,private,designer}.log.
Failures are retained in the corresponding *-resource-failure.log and
*-resource-repeat.log files. The isolated baseline is under
target/PaletteIconsBaseline, with target/PaletteIcons-Baseline-{build,native}.log.
ABI/header/asset/dependency audits passed; both SimbeyCore DLL hashes are unchanged.
Actual XP and physical monitor/theme changes were not tested in this pass.

## Palette scroll retention

After insertion, Pointer mode is restored without scrolling the palette. The
initial editor layout still shows Pointer. The regression failed against the
previous code, then passed for repeated visual/nonvisual insertion in both Forms
and Frames at 96/144 DPI, checking top index, scrollbar position and exact item
geometry. Debug/Release builds have zero errors/warnings; their focused palette,
TabPage placement and Memo workflow suites pass 852, 74 and 109 assertions each.
Logs are target/PaletteScroll-{Debug,Release}-{build,palette,tabs,memo}.log and
target/PaletteScroll-before-fix.log. Full suites and XP were not rerun for this
focused UI correction. SimbeyCore was reused without rebuilding or replacing it.

The nonvisual follow-up exposed a gap in that first regression: a direct NM_CLICK
notification followed by draining the current queue does not cover the list
view's deferred focus scrolling. Queued native mouse-down/up messages and a
message pump that includes the double-click timer interval reproduced the
remaining jump (target/PaletteFocus-before.log). ResetTool now changes selection
only, preserving the last tool's focused row so native input does not later
reveal Pointer. Initial Pointer visibility still uses the initial layout path.

The expanded regression covers OpenFile, SaveFile, repeated Timer insertion and
subsequent Up/Down keyboard insertion, preserving exact viewport geometry and
checking focus, Pointer selection, type and single insertion. Forms and Frames
are tested at 96/144 DPI. Debug/Release builds have zero errors/warnings, and the
focused palette suites each pass 1,068 assertions. Existing TabPage and component
suites pass 74 and 874 assertions respectively in each build. Logs are
target/PaletteFocus-{Debug,Release}-{build,palette,tabs,components}.log. Full
suites and XP were not rerun for this correction; SimbeyCore hashes are unchanged.

## Image control and generic Picture editor

Win32/x64 Debug/Release full designer suites pass 3,065 assertions each, including
palette icons, independent factories, native resources and the new Image workflow.
The corresponding public runtime and Studio integration suites pass 7,895 and 87.
After tightening PNG ancillary-chunk CRC validation, all four configurations were
rebuilt without warnings; private tests pass 665 assertions, and focused Image
designer/runtime and existing PNG editor suites pass 31/126/106 respectively.
Initial Debug runs with stale palette-count expectations are retained as failures;
the corrected successful logs use designer-final. Resource thresholds are unchanged.

Manual Release x64 checks verified the self-contained Image Gallery, windowless
surface selection/tree synchronization, Picture thumbnail and JPEG Browse,
orientation normalization, Cancel preserving the original clean document, and
native modal Preview/Close. The example file was not modified. See
ImageControlImplementation.md for properties, limits, optional deferrals, complete
test sequencing and target/ImageControl-* log names. SimbeyCore was reused unchanged;
actual XP and physical monitor transitions remain untested.

## Color swatches and parent color copies

The inspector uses cell custom drawing for all Color descriptors, preserving raw
numeric values and resolving system colors only for swatches/picker initialization.
Newly inserted visual controls copy raw BackColor/ForeColor from their immediate
container. The standard color picker remains in use. Full contract and validation
are in QuadooColorsImplementation.md.

All four builds have zero errors/warnings. Focused designer suites pass 98 assertions
per configuration, including native swatch pixels, container copies, Cancel/tag
retention and RGB commit. Full Debug designer suites pass, and Win32 Release passes
on an unchanged rerun. Initial Release runs hit existing palette USER growth;
x64 Release's rerun reached existing Preview GDI growth (26 to 28). All failures
are retained separately from passing focused suites. Limits/cycle counts were not
relaxed; full-suite resource instability remains unresolved. Previous baseline
reports and the new pre-color native baseline provide independent old-resource
failure evidence. SimbeyCore is unchanged. Native UI inspection confirmed swatches,
selection painting and the standard RGB dialog; no document was saved or system
settings changed. Actual XP and physical theme changes remain untested.

## Ctrl+arrow nudging (2026-10-17)

Ctrl+Left/Right/Up/Down moves a selected movable visual control by one design
pixel from the canvas or component tree, including Studio's shared tree. The
position-only operation uses the document's bounds transaction, ignores grid
snapping and zoom/DPI scaling, respects public movement capabilities, and retains
requested dimensions when MinWidth/MaxWidth clamp the effective size. Inline
property fields, palette navigation, inactive designers and source editing retain
their normal keyboard handling. Active mouse gestures and nonvisual components
are not nudged.

All four VC9 Win32/x64 Debug/Release builds finish with zero compiler errors or
warnings. Each final configuration passes 189 focused nudge assertions, the full
3,404-assertion designer suite and all 96 Studio integration assertions. Coverage
includes four directions at multiple DPI/zoom settings, snapping, repeated keys,
parent-relative/container movement, negative coordinates, constrained effective
sizes, inspector/dirty updates, serialization, root/docked/TabPage restrictions,
nonvisual selection, drag cancellation, inline editing and shared-tree routing.
Logs are in target/DesignerNudge-final-* and target/DesignerNudge-build-final-*.
The runtime/published interfaces are unchanged, and all four SimbeyCore DLL hashes
match their pre-task values. Actual Windows XP execution was not performed.

## Quadoo Form color picker (2026-10-17)

Color properties now open the same native Quadoo Form dialog in the standalone
designer and Studio. QuadooFormDesigner.dll contains its JSON RCDATA and 16/32px
DIB icon. The form uses ColorSwatch for the 18x18 Vista grid, editable preview and
all thirty named system colors. Long names have wider cells; a scrolling content
area keeps centered OK/Cancel buttons accessible on small displays. The frame has
a Single border without sizing, minimize or maximize controls.

The picker retains raw system tags, supports live #RRGGBB/raw-system hex entry,
rejects invalid/incomplete values without replacing the preview, and uses the
existing editable ColorSwatch for the nested standard RGB picker. Cancel, caption
close and Escape leave the caller's value unchanged. Enter uses the default OK
button. Both owner disabling/restoration and initial hex-field focus are checked.

The required COLOR_SCROLLBAR entry exposed JSON's widening of -2147483648 to a
LongInteger. Native Color normalization and the ColorSwatch's private palette
normalization now range-check and canonicalize signed 32-bit numbers. Tests cover
construction, property replacement, rejection rollback, actual JSON text round
trips and separately named Color descriptors. Existing exports, ordinals, IIDs,
vtables, property-type numbers and JSON numeric encoding are unchanged.

All four VC9 builds finish with zero errors/warnings. Final focused picker suites
pass **1,053 assertions per configuration** (target/ColorPicker-final-picker-*).
They exercise every Vista entry, all named system entries, exact native swatch
pixels, valid/invalid hex, nested picker initialization/accept/cancel, invalid-OK
blocking, keyboard acceptance/cancellation, resources and returned raw values.
The x64 inspector-color suites also pass 138 assertions each, including Cancel's
dirty/tag preservation and accepted RGB changes.

The stress test verifies destruction of every native picker child HWND and both
private icon handles. After three warm-ups, twenty measured cycles retain a strict
GDI baseline, including the windowless control fonts/brushes. Initial versions
using aggregate USER counts encountered delayed Windows accessibility allocations;
these logs remain in target/ColorPicker-focused*, ColorPicker-verified-* and the
first full-run logs. Direct owned USER-handle checks distinguish dialog cleanup
from those unrelated process caches. Existing shared GUI-resource monitor tests,
thresholds and measured-cycle counts were not modified.

Full suites were run in all four configurations. The final full-run results
(target/ColorPicker-rerun-*) were:

| Configuration | Native harness | Private metadata/storage | Designer | Studio integration |
|---|---|---|---|---|
| Win32 Debug | 8,074 passed | 674 passed | 4,103 passed | 96 passed |
| Win32 Release | 8,062 passed | 674 passed | 4,129 passed | 96 passed |
| x64 Debug | 8,062 passed | 674 passed | Existing Preview GDI growth, 27 to 28 | 96 passed |
| x64 Release | Existing choice-control USER growth, 24 to 25; other groups passed | 674 passed | Existing Preview GDI growth, 26 to 28 | 96 passed |

Those x64 full-designer failures occur before the new picker tests. The picker
and inspector-color groups were therefore also run directly in x64. The final
owned-resource stress assertions were added after the full runs and verified
independently in all four focused suites; production code did not change during
that test adjustment. First-run failures and all unchanged reruns are retained,
including the earlier sizing/choice USER and Preview GDI instability. These
existing aggregate resource checks remain unresolved; no additional controls or
QuadooScript changes were made for this task.

Native UI inspection confirmed the palette, preview, categorized swatches and
embedded icon. All four SimbeyCore DLL hashes match their pre-task values; it was
not rebuilt. Actual Windows XP execution and physical theme/DPI changes were not
performed. Dialog source, resource/build details and manual preview instructions
are now in ColorDialogImplementation.md (the picker subsequently moved to QuadooForm.dll).


## 2026-10-18: runtime ColorDialog and authored color-picker palette

The picker implementation and resources now live in QuadooForm.dll. The obsolete
DesignerDialogs project, source directory, import libraries and DLL outputs were
removed. Both solutions/build scripts and both designer hosts use the runtime
export. dumpbin confirms that QuadooFormDesigner.exe and QuadooStudio.exe import
QuadooForm.dll, with no QuadooFormDesigner.dll dependency. Existing exports retain
their ordinals; ShowQuadooColorPicker is added at 16, and IQuadooColorDialog (0035)
is additive. See ColorDialogImplementation.md for the component/API contract.

The built-in nonvisual ColorDialog has a serializable Color property, Execute,
OnAccept/OnCancel and inherited lifecycle events, plus its own standard 20x20 ICO
resource. Its inspector editor uses the same runtime picker. All 141 Vista swatches
are explicit 18x18 controls in the form resource, ordered neutral/light-to-dark
then hue. VistaColors.cpp is an authoring reference only, not linked into either
production or tests. Six system strips contain all 30 canonical colors; their
JSON labels omit COLOR_, while source names and the current-color description
retain it. Manual UI inspection confirmed all category rows fit vertically on a
normal display. The controller accounts for the theme/DPI nonclient frame and
retains scrolling when the monitor cannot fit the complete form. Long caption
labels retain the existing ellipsis behavior; the full selected name appears
above the strips.

Native wheel/line/page input now clamps negative offsets before calling the
public scrolling API. Negative explicit SetScrollPosition arguments still fail.
Regression tests scroll past both ends, wait for another real modal-loop cycle,
and verify that the picker stays open and preserves its value/owner state.

All four VC9 builds completed with zero compiler/linker warnings or errors.
JSON/resource/icon checks, unique published interface IDs, solution/project XML,
DLL export/dependency checks, and all four unchanged SimbeyCore SHA-256 hashes
were verified. SimbeyCore, JSON, CodeEdit and QuadooScript were not rebuilt.

Final relevant results (logs under target/):

| Configuration | Native ColorDialog | Picker dialogs | Designer component workflows | Palette/tray | Full designer |
| --- | --- | --- | --- | --- | --- |
| Win32 Debug | 103 passed | 1,803 passed | 877 passed | 1,170 passed | stopped in old Preview resource check |
| x64 Debug | 103 passed | passed in full suite | passed in full suite | passed in full suite | 5,286 passed |
| Win32 Release | 103 passed | 1,803 passed | 877 passed | functional checks passed; old USER counter failed | stopped in old palette resource check |
| x64 Release | passed in full native run | 1,803 passed | 877 passed | 1,170 passed | stopped in old palette resource check |

The picker coverage includes every authored Vista/system entry, initial raw
system tags including COLOR_SCROLLBAR/INT_MIN, hex validation, standard chooser
accept/cancel, keyboard/modal owner behavior, wheel/scroll boundaries, callback
and observer failure rollback, recursive execution, NULL-owner resolution,
destruction from OnAccept, exact child-HWND/HICON destruction, and repeated-cycle
GDI accounting. Native metadata tests cover COM identity/AddRef/Release through
all seven supported interfaces, unsupported visual QI, properties, JSON round
trips, wrong-thread/destruction guards, design/preview suppression and factory
retention/unregistration. Designer workflows verify placement, property editing,
serialization/reload and deletion of ColorDialog.

All existing suites were run in Debug/Release for Win32/x64. The private runtime
suite passed 674 assertions and Studio integration passed 96 assertions in each
configuration (ColorDialog-full-*-QuadooFormNativeTests.log and
ColorDialog-full-*-QuadooStudioIntegrationTests.log). The complete Win32 Release
native run passed 8,167 assertions. Continuing full native runs to exercise every
group also passed the new ColorDialog group in all configurations.

Remaining failures match the already-recorded Windows GUI counter instability;
no thresholds or existing resource checks were relaxed:

- Win32 Debug native sizing/focus: USER 6 -> 7; new ColorDialog group passed
  (ColorDialog-final-Debug-Win32-QuadooFormHarness.log, 7,963 assertions).
- x64 Debug native choice resources: USER 24 -> 26; new ColorDialog group passed
  (ColorDialog-final-Debug-x64-QuadooFormHarness.log, 8,106 assertions).
- x64 Release native tab resources: USER 24 -> 25; new ColorDialog group passed
  (ColorDialog-verified-Release-x64-native.log, 7,967 assertions).
- Win32 Debug designer Preview resources: GDI 26 -> 28
  (ColorDialog-verified-Debug-Win32-designer.log).
- Win32 Release designer palette resources: USER 23 -> 24, also reproduced by
  its focused palette run (ColorDialog-verified-Release-Win32-{designer,palette}.log).
- x64 Release designer palette resources: USER 23 -> 24; a subsequent focused
  palette run passed all 1,170 assertions
  (ColorDialog-verified-Release-x64-{designer,palette}.log).

Final picker/component results use ColorDialog-verified-* logs; earlier logs
from development attempts are retained separately. Windows XP execution still
requires the user's VM; the binaries were built with the existing VC9/XP settings.


## 2026-10-18: 20-column color-picker reflow

The authored Vista palette now uses 20 columns and 8 rows, retaining the same
141 colors, order and 18x18 swatches. Its panel is 436x172. The current-color
preview is 400x80, and it and its text fields move right to X=488. System-color
categories begin at Y=224 with a 56-pixel pitch, leaving a four-pixel gap after
each heading/swatch block. The existing overall form dimensions are retained.
Layout rectangle checks confirm no overlaps. Generated test coordinates reflect
the new grid and preview location.

Only QuadooForm.dll and the designer test executable were rebuilt. All four VC9
builds had no errors or warnings. As requested, only --picker-only was run:
Debug/Release, Win32/x64 all passed 1,803 assertions each. This covers every Vista
and system swatch, custom/RGB/hex selection, modal/keyboard and wheel behavior,
component events/rollback, and dialog resource lifetime. Logs are
ColorPicker20-<configuration>-<platform>-picker.log under target/. Shared
SimbeyCore binaries remain byte-for-byte unchanged.


## 2026-10-18: 21-column color-picker reflow

The Vista palette now uses 21 columns and 7 rows (458x150), with the same 141
colors, order and 18x18 tiles. The preview and its labels shift right 22 pixels
to X=510; the preview width becomes 378, retaining its right edge and height.
System categories begin at Y=202 and use a 60-pixel pitch, doubling the gap
between heading/swatch blocks from four to eight pixels. The last block ends
at Y=554, leaving ten pixels before the footer. Form dimensions remain unchanged.
Generated layout fixtures and click/pixel coordinates follow the new positions;
rectangle validation found no overlaps. Only the runtime resources and color
dialog tests are affected.

All four targeted VC9 builds completed without errors/warnings. Only
QuadooFormDesignerTests.exe --picker-only was run: Debug/Release, Win32/x64 each
passed all 1,803 assertions. Logs are target/ColorPicker21-<configuration>-<platform>-
picker.log. SimbeyCore binaries remain unchanged.


## 2026-10-18: Context menus, creation order and clipboard commands

Implemented in the shared editor, standalone host and Studio menus/shared tree.
See DesignerCommands.md for command behavior and clipboard/transaction details.
The published QuadooForm DLL ABI is unchanged. All final VS2008 Debug/Release,
Win32/x64 builds completed with no errors or warnings. The previously running
standalone Release Win32 executable initially required an alternate output; after
it closed, the normal target/Release/QuadooFormDesigner.exe was rebuilt too.
SimbeyCore DLL hashes remain unchanged in all four configurations.

Final --commands-only: 54 assertions passed in each of the four configurations.
This includes canvas/tree/keyboard/tray popups and checkbox/container states;
creation and tree ordering; copy/paste of complete subtrees and mixed JSON arrays;
fresh IDs, collision-free names, events and properties; active versus persisted
tab pages; independent extension factories; rejected paste/reorder rollback;
inline text shortcuts; and factory cleanup. A persistent clipboard lock cannot
remove a cut source. A short ownership handoff succeeds with a bounded retry
(nine 10 ms delays at most). Tests save and restore materialized clipboard data.
Studio integration: 108 assertions passed in each configuration, covering menu
states, shared-tree order/selection, keyboard routing and inactive designer state.

The earlier full Debug Win32 and Debug x64 runs each passed 5,334 assertions
(with the initial 50-assertion command suite, before the final retry/extra checks).
Full Release runs were attempted and rechecked without changing their checks:

- Win32 recheck reached the Preview resource test, then failed its strict GDI
  baseline: 28 to 30 objects, USER unchanged at 5 (2 measured cycles). These
  counters have exhibited the same Windows accessibility/cache behavior in
  previous validation passes. An earlier attempt stopped at palette focus.
- x64 recheck stopped at PaletteTests.h's existing focus/caret assertion:
  GetFocus() == tools and focused item == row. Another attempt reached the new
  commands but failed to acquire the clipboard; the bounded retry now covers
  transient ownership contention while retaining persistent-lock failure checks.

Additional final Release Win32/x64 scoped regressions all passed: --tabs-only
74 assertions, --nudge-only 189, --components-only 877, in each platform. Full
Release suites remain non-green because of the above GUI checks; their thresholds
and behavior were left intact.

One intermediate focused Debug x64 run failed the combined pasted Enabled /
TabOrder assertion. Five immediate repeats and the final four-configuration
matrix passed. Failure-only diagnostics now print both values if it recurs; no
reproducible cause was identified.

Logs are target/DesignerCommands-<configuration>-<platform>-build.log,
-tests.log, -studio.log and -full.log. Release rechecks are -full-recheck.log;
scoped Release logs use -tabs.log, -nudge.log and -components.log.

## Design-time packages (2026-10-18)

Both hosts now share the package catalog, design-only public-interface factories,
Win32 Packages dialog and compiled resources from Packages/. All four VS2008
builds are clean. The final package suites pass 318 assertions and Studio package
suites pass 18 in each Debug/Release Win32/x64 configuration. Full native/private
suites pass 8,167/674; full designer/Studio attempts stop at the unchanged
ClipboardTests.h snapshot failure, 0x8007054F (1,610/42 assertions). The remaining
independent designer groups pass in all configurations. Strict resource checks
remain unchanged, and SimbeyCore binaries were not rebuilt or changed. See
[Packages/Validation.md](Packages/Validation.md) for coverage and logs.

## Sibling multi-selection (2026-10-25)

The standalone designer and Studio share GUID-based sibling selection, group
dragging/nudging, clipboard arrays, group deletion and the CSelectionProperties
descriptor intersection. Common properties retain the primary descriptor indices;
mixed values are explicit, incompatible enums/event signatures are excluded, and
group names are read-only. Group mutations validate a candidate document and use
one source transaction. Rejected edits preserve the document and selection.

VS2008 Win32 Debug/Release builds of the core, standalone designer, Studio and both
test executables have zero errors/warnings. Both full designer suites passed
21,720 assertions; both full Studio suites passed 545 assertions. Final focused
checks after tab-page property hardening passed in each configuration:

| Suite | Assertions |
| --- | ---: |
| Sibling selection, gestures, common/mixed properties and rollback | 58 |
| Real clipboard cut/copy/paste with containers and fresh identities | 14 |
| Tab-page selection and placement | 74 |
| TreeView designer | 14 |
| ImageList designer | 358 |
| Studio selection, source edits, rollback and save/reopen | 16 |
| Studio TreeView | 13 |
| Studio ImageList | 14 |

The clipboard-specific selection tests run in a private window station, preserving
the user's clipboard, including any delayed shell formats. Tab-page checks cover
both visible and persisted ActivePageIndex changes and subsequent group edits.
Logs are target/MultiSelection-*.log. The published runtime ABI is unchanged;
all four existing SimbeyCore DLL hashes match their baseline. Windows XP was not
run in this validation pass; builds use the existing XP-compatible VC9 toolset.
