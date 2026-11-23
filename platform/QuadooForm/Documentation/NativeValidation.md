# Native validation â€” 2026-10-15

The 2026-10-22 OnEnter/OnLeave and AQVMT QuadooPlugin follow-up is recorded in
[FocusAndPluginValidation.md](../../desktop/QuadooStudio/FocusAndPluginValidation.md),
including full native results, focused host/designer results and the current
clipboard-format blocker in full GUI suites.

Stages 1, 2, and 4 of the review sequence are implemented. Stage 3 (QuadooScript/Studio integration) remains deferred.

The native architecture refactor, focused hardening pass, choice controls, text-only ComboBox/ListBox, and TabControl/TabPage passes are complete. Existing published interfaces, structures and DLL exports are preserved. Additive public interfaces are IQuadooRadioButton (IID ending 0020), IQuadooComboBox (0021), IQuadooListBox (0022), IQuadooTabControl (0023), and IQuadooTabPage (0024). The item controls reuse the existing SimbeyCore IRStringArray interface; no collection or QuadooScript interface was introduced. The runtime uses shared native bases, fourteen concrete implementations, per-factory composed metadata, and dynamic instance property storage. `Kind`, `IControlInternal`, and `Internal()` are absent from the runtime.

| Check | Result |
| --- | --- |
| VS2008 Debug Win32 runtime, harness, extension DLL, internal tests | Passed; 0 errors, 0 warnings. |
| VS2008 Release Win32 runtime, harness, extension DLL, internal tests | Passed; 0 errors, 0 warnings. |
| Debug native harness | 6837 assertions passed across all twenty-three test groups on the local host. |
| Release native harness | 6837 assertions passed across all twenty-three test groups using the private rebuilt SimbeyCore DLL. |
| Debug/Release internal metadata, storage and rollback tests | 83 assertions passed in each configuration. |
| Active/forwarding header with published QuadooVM header, both include orders | VC9 syntax checks passed. |
| Release DLL imports | JSON, SimbeyCore, Win32, OLE, and the VC runtime; no QuadooScript VM dependency. |
| Project XML/source paths | All four native project files parsed and every source/header path exists. |
| Architecture and native documentation JSON examples | All three parsed successfully. |
| Native source convention scan | No STL usage, C++ exception handlers, or throw statements introduced. |

Commands:

```bat
tests\platform\QuadooForm\build-native.cmd Debug --test
tests\platform\QuadooForm\build-native.cmd Release --test
```

The normal `target\Release\SimbeyCore.dll` remains locked by a running application. Release SimbeyCore was rebuilt successfully with the existing output override into `target\QuadooFormCompareValidation`; the generated Release import library was restored to its normal location and all four QuadooForm Release projects were relinked successfully. The current Release harness, native tests, runtime, extension and JSON DLL were copied beside the isolated dependency, and both suites passed there. No running application was interrupted. That folder contains the corrected Release dependency and runnable validation binaries. Rebuild SimbeyCore Release normally after the application releases the DLL before running the updated harness from `target\Release`.

The test groups cover:

1. Lifecycle, native notifications, eager named binding, rename rollback, cancellation, and destruction during a callback.
2. Docking, anchors, logical/effective bounds, explicit 96/144/192 DPI scaling, observer batching, and removal during callbacks.
3. Dedicated scrolling content HWNDs, nested scrolling, two Frame instances with separate namespaces/reused design GUIDs, frame omission from Form JSON, and independent deterministic round trips.
4. Failed creation rollback, duplicate names/IDs, invalid schema/properties/types, missing callbacks before OnCreate, wrong-thread access, and external HWND destruction.
5. Modal result handling, owner reenable, cancellation, and WM_QUIT propagation. Quit tests pump ordinary queued messages before retrieving the synthesized quit, preserve positive/zero/negative exit codes, verify modal HWND destruction and cleared results, and fail if no quit was reposted.
6. Registry/factory lifetime checks, recovery from a failed custom factory, design-surface hosting, script callback suppression, preserved unknown design properties, and immutable property/default snapshots.
7. Margins, min/max sizing, container AutoSize, focus/tab order, and repeated native creation/destruction. Bounded warm-up requires three identical settled resource samples within eight workloads, then 20 cycles verify destroyed control HWNDs, zero surviving instances in every relevant factory, and no growth above the frozen process GDI/USER baseline. Resource-monitor failure injection verifies retained USER windows, timer-driven cleanup, persistent warm-up leaks, and a leak after the baseline is frozen.
8. An independently implemented ExternalMeter: custom property/event inspection and defaults, registration, construction/PostBind/initialization order, property notifications, successful and failed delegate replacement, writable event arguments, public painted-control input routing, dock/anchor/AutoSize/DPI layout, generic serialization and reconstruction, duplicate names/client alignment, modal callback error propagation, missing-handler rollback, design-mode suppression, destruction, busy factory rejection, and factory retention until final component release after registry/context release.
9. The interface capability matrix and canonical IUnknown identity for every built-in concrete type, preserved per-type property/event counts, consistent factory/instance descriptor order and flags, and default values matching new instances.
10. Cross-DLL failure injection for registration: identity, parent/state/name/ID, property and descriptor getters, optional interface errors, successful null/invalid QI outputs, null/oversized/duplicate/incompatible metadata, invalid enum domains, observer attachment before/after failure, reentrant subscription callbacks, failure after subscription, and even failed observer removal. Tests compare children, lookup identity, existing bounds, layout calls, serialized JSON, subscription counts, and reference counts before/after failure. They also verify retry, unregistration, factory/instance mismatch rollback, and invalid notification indexes/senders.
11. DLL lifetime beyond component destruction: retained event arguments retain the destroyed sender/factory; descriptor references prevent unloading; complete release allows unload and reload. The DLL uses only the published QuadooForm interfaces and shared repository utilities. QueryInterface tests cover every supported path, cross-query symmetry, canonical IUnknown, failed output clearing, null output pointers, and exact reference balance.
12. CheckBox/RadioButton native interaction: mouse down/up, BM_CLICK, Space, arrow navigation through IsDialogMessage, preserved native dialog codes, real tab traversal into GroupBox/nested containers, state/enum validation, two-state/three-state cycling, native BM_SETCHECK synchronization, one observer notification per change, no events for unchanged values, programmatic changes without OnClick, sibling exclusivity across nonadjacent controls, separate parent scopes, hidden/disabled navigation, TabOrder edits, reentrant selection rejection, reentrant checked-radio construction rollback, delegate failure after a committed state change, and destruction during OnChange.
13. GroupBox native frame painting, child HWND parenting, shared painted-child input, docking at 96/144/192 DPI, immutable stored bounds, caption/font/padding edits, explicit ancestor batching, runtime attachment/layout, an independent ExternalMeter DLL inside GroupBox, custom properties/events, deterministic JSON reconstruction, recursive destruction, factory busy rejection, and retention until the final reference is released after destruction.
14. Invalid choice-state/default/event definitions and failed child-subtree creation preserve the existing hierarchy and serialized JSON. Tests verify retries, initial BM_SETCHECK during OnCreate, nested layout edits during Form.OnCreate, construction of the interactive example, and native create/focus/DPI/destroy cycles using the same bounded warm-up and frozen-baseline policy. Twenty measured cycles verify destroyed HWNDs, no process GDI/USER growth, and zero surviving instances in each new built-in factory.

15. ComboBox/ListBox snapshots, copying from a separate IRStringArray implementation without Clone/GetArray/Compare, UTF-8/wide normalization, empty and duplicate strings, order preservation, selected-item text, unchanged setters, replacement with shorter/empty arrays, invalid array/index inputs, native programmatic/prefix/keyboard selection, native edit changes, editable-child key events with Handled, reentrant updates, delegate failure after commit, and destruction during both direct and native callbacks.
16. Descriptor-based Items JSON editing, invalid/null/non-string/NUL-containing values, deterministic full-form round trips, persisted free text, runtime editable/noneditable style replacement, popup-height editing, native colors, focus/visibility retention, observer batching, docking and physical sizes at 96/144/192 DPI. Invalid child definitions unwind their registered siblings; the original form remains unchanged. ComboBox selection callbacks also verify that native Text already matches the committed logical Text.
17. Repeated create/focus/edit/style/dropdown/destroy cycles verify logical and internal ComboBox edit/list HWND destruction, live factory counts before/after final references, busy factory rejection, and no process GDI/USER growth across 20 measured cycles after bounded warm-up.
18. TabControl/TabPage native mouse and keyboard navigation, OnChanging cancellation and failure, OnChange after committed visibility, silent programmatic selection, null/-1 and invalid indexes, native selection synchronization, hidden-header/all-page index mapping, hidden-page caption edits preserving selection, page reordering, remembered descendant focus, Ctrl+Tab/Shift+Ctrl+Tab through IsDialogMessage from TextBox and ComboBox edit children, reentrant selection rejection, and local page OnShow/OnHide.
19. Complete tab/page COM identity paths and capability/default metadata, child HWND parenting, page display-area layout at 96/144/192 DPI, unchanged stored bounds, first-layout anchor baselines, MultiLine/HotTrack edits, caption edits, independent ExternalMeter DLL children with docking/custom properties, deterministic full JSON reconstruction, requested Visible semantics, and coalesced ActivePageIndex observer notifications. The capability matrix now includes all fourteen built-in types.
20. Invalid page parents, non-page children, invalid initial indexes, duplicate page names and missing callbacks unwind without changing the existing form. Active-page destruction chooses a replacement; runtime page creation establishes the viewport before its anchored children initialize. Incoming-page destruction during OnChanging cancels stale native selection. Destroying the tab control during OnChanging and OnChange is safe. Twenty measured create/focus/select/reorder/DPI/destroy cycles verify all captured HWNDs are destroyed, factory live counts reach zero after final release, busy factory rejection, and no process GDI/USER growth after bounded warm-up.

21. Designer prerequisites: owned multi-image DIB ICO persistence and native Form icon display; corrupt, noncanonical, excessive Base64 and truncated/oversized ICO rejection; invalid icon rollback; border/buttons/topmost style changes retaining native identity and client allocation, including visible scrollbars; shared Form scrolling, focus scrolling and stored-coordinate invariance at 96/144/192 DPI.
22. Button modal completion through nested containers, nonmodal safety, mrNone, OnClick ordering, cancelled OnCloseQuery restoring the pending result, independent top-level modal Preview and suppressed application delegates.
23. Factory enumeration and parent eligibility, optional resize-capability metadata, ComboBox height restrictions and docked capabilities. The existing interface/default matrix also covers the new optional runtime interfaces and descriptor additions.

The separate internal executable tests 83 conditions covering incompatible/duplicate/default/cyclic native metadata, valid inherited default overrides, empty and populated storage lookups, out-of-range property/event access, missing enum-choice arrays, duplicate labels/values, invalid enum defaults/types, incompatible inherited enum domains, and invalid Items/ItemIndex/ActivePageIndex/TabVisible metadata. It injects failures after a replacement ComboBox and its items/edit subclass have been created. Both SetItems and Style failure preserve the original HWND, native content/selection, properties, serialized form, registration and live count. Tab tests inject header insertion, candidate Arrange, observer subscription after attachment, native caption update, reorder insertion, and hidden-header insertion failures. Each checks unchanged child/page/native counts, active selection, live counts and serialized state as appropriate; successful retries work and destruction releases all instances. Release executes the same checks with assertions disabled.

The choice pass also fixes an existing nested-layout issue: an ancestor layout pass no longer blocks layout of ready child containers. Explicit BeginUpdate still defers layout. Initialization continues to defer notifications while allowing OnCreate geometry edits to propagate through ready descendants.

Interactive sample: `tests\results\bin\Debug\QuadooFormHarness.exe --show-choices`. It presents two independent GroupBox radio groups and both CheckBox modes. Automated tests construct the same sample definition.

Windows XP compatibility correction: the harness previously assumed a WM_QUIT-filtered PeekMessage would immediately retrieve a reposted quit. PostQuitMessage marks a deferred quit request; pending ordinary posted messages can prevent the synthesized WM_QUIT from being returned on XP. The harness now uses a bounded, unfiltered outer-loop pump, with a deliberate posted marker ahead of the pending quit. It tests exit codes 123, 0 and -123 and separately verifies that the helper does not invent a quit when none exists. The runtime's ShowModal/PostQuitMessage behavior and public ABI are unchanged. See [Microsoft's explanation of deferred WM_QUIT delivery](https://devblogs.microsoft.com/oldnewthing/20051104-33/?p=33453).

XP resource-test follow-up: the user confirmed the modal correction and next reported a USER-object comparison failure. The original resource test destroyed its focused EDIT before pumping and assumed one full workload was sufficient warm-up. The harness now pumps while controls remain alive, waits for resource samples to stay unchanged for 100 ms, and establishes a baseline only after three identical end-of-cycle samples (maximum eight warm-up cycles). Each of 20 measured cycles allows up to two seconds of message-driven cleanup and must remain at or below that fixed baseline. Retained HWND checks and factory live counts apply during warm-up too; no count allowance or measured-phase rebasing was added. Failure diagnostics include actual/baseline counts, warm-up history, and surviving window handles/classes. [GetGuiResources reports process-wide handle totals](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getguiresources), so steady-state initialization and immediate component destruction are checked separately. Assertion totals can vary with the host's required warm-up cycles. The corrected resource test has passed locally; its XP result is not yet known.

The toolchain is Microsoft C/C++ 15.00.30729.01 with Windows SDK 6.0A. VS2008 project builds required execution outside the Codex sandbox because the program-database service failed inside it with C1902. The original debug settings were preserved.

Local validation used the current Windows host. The user confirmed the modal test now passes on XP; the updated resource tests, item controls, tabs and new designer prerequisites still require an XP rerun. Physical monitor changes and other older Windows versions were not exercised locally. No actual x64 configurations, scripting adapter, Studio integration, ComboBoxEx, owner-drawn images, image lists, TreeView or ListView were implemented. The subsequent standalone Phase A/B designer is documented in DesignerReadme.md and DesignerValidation.md. `QuadooFormHarness.exe --show-items` is the interactive text-item sample; `--show-tabs` demonstrates the VCL-style page-container model.

The snapshot checks now use `IRStringArray::Compare` after the SimbeyCore length-ordering correction. They cover equality, both prefix directions, empty arrays, changed contents, self-comparison, input failures, and balanced temporary string references. A remaining duplicate release in Compare was corrected by clearing its temporary after the loop release. Native item change detection retains direct JSON comparison because its stored property values are already JSON; no array conversion or ABI change was introduced. Native HWND consumers must reacquire GetHandle after Items or Style replacement, as documented in NativeRuntime.md.

The subsequent designer pass preserves all 35 existing published interface
declarations (verified against Subversion BASE), structure layouts and ordinals
5-11. It adds optional interfaces ending 0025-0028 and icon exports 12/13. New
Form/Button properties shift event-property indexes; callers resolve names across
versions. All seven runtime/designer projects received clean Win32 Debug/Release
VC9 rebuilds with zero warnings/errors. Both public-header include orders compile
alongside QuadooVM with its normal Windows prerequisites; binary imports have no
QuadooScript dependency or mandatory newer DPI APIs. See DesignerValidation.md
for editor tests and remaining integration limits.

The inline-designer pass appends QuadooProperty::Memo (17), preserving all existing
enum values and published interfaces/layouts/exports. Both Win32 configurations
pass the full 6,837-assertion native suite and the extended 88-assertion private
suite. Memo tests cover descriptor type/defaults, String accessors, multiline
Unicode values, rejected non-string input, and JSON reload. The public-only
extension additionally advertises a Memo property for designer round-trip tests;
its ordinary runtime schema remains unchanged. See DesignerValidation.md for the
container-order rollback and inline-edit tests.

## Nonvisual component milestone and Timer (2026-10-16)

VS2008 Win32 Debug and Release each pass the full public harness (7,403 assertions)
and private native suite (116) with zero build errors/warnings. The complete
designer suite also passes 1,750 assertions in each configuration, including its
resource checks. Logs are target/Components-{Debug,Release}-{build,native,private,
designer}.log. Release uses the isolated corrected SimbeyCore dependency folder.

The independently compiled extension now provides ExternalService using only
published QuadooForm interfaces. Tests verify shared visual/nonvisual identity
namespaces, separate runtime Frames, eager binding/PostBind, absence of visual
interfaces/HWNDs/layout, editing, JSON, rollback, reentrant destruction, retained
arguments and factory/DLL lifetime. Nine injected registration failures preserve
the prior collection and serialized definition. Persistence/host interface errors
abort saving without producing a lossy result. COM identity and balanced
AddRef/Release are checked across the new interfaces and independent implementation.

Private chooser tests cover accept/cancel/error, owners, thread affinity, nested
execution rejection, invalid results, callback failure rollback and sender
destruction. Timer tests cover initialization gating, runtime Form/Frame ticks,
enable/disable/zero/maximum intervals, invalid values, stale/reentrant delivery,
sender/root destruction, retained args and 32 create/tick/destroy cycles. Injected
timer start failures preserve existing timers/properties and abort failed root
activation without OnDestroy callbacks; Design/Preview never start OS timers.

All 39 existing published interface declarations match Subversion BASE. Existing
public IIDs, vtables, structures, property enum values and export ordinals
5-13 are unchanged. Additions are nine interfaces ending 0029-0031, EDesignUnsupported,
PlaceholderReason, and QuadooCreateComponent at ordinal 14. Schema remains version
1 with an optional root components array. The ownership/API report and remaining
limits are in ComponentExtensibility.md. No QuadooScript/Studio code was changed.
Windows XP execution was not performed in this milestone.

## Form keyboard preview, Default/Cancel and Memo

The keyboard/Memo pass preserves all 48 existing published interface declarations
and export ordinals, adding optional IQuadooFormKeyboard (0032) and IQuadooButton
(0033), Form.KeyPreview, Button.Default/Cancel, OnKeyPress, and a separately
registered Memo. The technical decisions and host preprocessing example are in
KeyboardAndMemoImplementation.md. Runtime-attached Frames are traversed for
keyboard arbitration without changing FindControl or serialization namespaces.

Clean VC9 Win32 Debug/Release rebuilds report zero errors/warnings. Both full
public suites pass 7,769 assertions and private suites pass 119. Native tests cover
message-stage ordering, handled/queued characters, Unicode, Ctrl+A, modified Enter,
thread affinity, claims, nested Frames, tab/ancestor eligibility, reentrant
activation/destruction, close-query cancellation, Memo persistence/style changes,
factory live counts and strict native GDI/USER teardown. The independent extension
DLL claims Enter/Escape through published interfaces/native subclassing and retains
its code while the subclass is attached.
Reentrant OnChange callbacks replace Text in both Memo and TextBox, with matching
native/logical values and exactly one notification per committed value.

Both designer suites pass 1,934 assertions. One Release run reproduced the existing
intermittent designer GDI baseline failure; its unchanged rerun passed. The failing
log and earlier unchanged pre-PNG reproduction are retained. See the implementation
note and DesignerValidation.md; resource limits were not relaxed. Existing
SimbeyCore DLL hashes are unchanged, and ordinary Release dependencies pass.
Windows XP and live IME composition were not tested. No QuadooScript/Studio code
was changed.

## Factory palette icons

IQuadooPaletteIcon (0034) is an additive factory interface; all 50 previous
interface declarations and export ordinals remain unchanged. All 18 concrete
built-in factories return independently owned icons from standard DLL resources.
The independent extension implements the same interface without runtime classes.
Icon ownership, sizing/thread checks, live counts, COM identity and module unload
are exercised by the designer suite's new palette group, alongside shared-view
and resource-cycle tests. No QuadooScript, runtime image-list component or
SimbeyCore rebuild was introduced. Both existing dependency hashes match.

Both Debug and Release public/private suites pass 7,769/119 assertions. Earlier
Release full public runs encountered USER growth 23 to 24 in the existing
choice-control resource check; an isolated pre-change runtime/extension also
reproduced USER 23 to 24 in its sizing/focus resource check. The final Release
runtime run passed. Focused palette/tray tests pass 740 assertions in each build.
See
PaletteIconsImplementation.md and DesignerValidation.md for final run results,
the preserved failure/baseline logs and the separate designer GDI intermittency.

## Windowless Image control

The Image implementation and bounded reusable PNG/ICO renderer are documented in
ImageControlImplementation.md. All 51 interface declarations and export ordinals
are unchanged; the Picture property type is appended as value 18. Existing custom
factory and independent-DLL coverage passes.

VC9 Win32/x64 Debug/Release builds have zero final errors/warnings. The full public
suite passes 7,895 assertions in each configuration; the private suite passes 665.
After the final ancillary-PNG CRC check, private tests and affected Image/PNG
suites were rerun in all four configurations. Fault injection covers replacement,
paint-time allocation and serialization output, retaining previous state. Strict
GDI cycle counts pass. All four SimbeyCore hashes are unchanged. XP execution and
physical monitor changes were unavailable. Detailed sequencing and retained logs
are in the implementation note; logs use target/ImageControl-*.

## Quadoo colors and inspector swatches

QuadooProperty::Color now preserves RGB or tagged system-color values. The new
ResolveQuadooColor export is appended at ordinal 15; existing interfaces, exports
and ordinals remain unchanged. See QuadooColorsImplementation.md for the contract,
late resolution, default values, designer insertion copies and complete validation.

All four Win32/x64 Debug/Release builds have zero errors/warnings. Focused native
color suites pass 68 assertions, private suites pass 671, and Studio integration
passes 87 in every configuration. Full Win32 runtime suites pass (Debug on an
unchanged rerun). Full x64 runs reached and passed the color tests but retained
failures in older USER-count groups. Initial failure logs remain intact. A saved
pre-color DLL baseline reproduced sizing/focus and choice-control USER growth.
Both baseline and current isolated tab resource checks pass; current checks pass
446 assertions in each configuration. Resource thresholds were not changed.
SimbeyCore hashes are unchanged. Actual XP remains untested. Logs use
target/QuadooColors-*; the implementation note records failures and reruns.

## Windowless ColorSwatch

See ColorSwatchImplementation.md for the property/event contract and complete
2026-10-17 validation results. All four VC9 builds and final focused native
(92), designer (18), private failure-injection (671), and Studio integration (87)
suites pass. The full x64 Release suites pass. Other full runs retained existing
choice/item/tab/palette USER-count intermittency; their original logs remain in
target/ColorSwatch-full-*. No resource thresholds were changed. The published ABI
and SimbeyCore binaries are unchanged; actual XP execution remains untested.
