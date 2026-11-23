# QuadooForm native runtime

Keyboard/Memo additions and the required nonmodal host preprocessing call are
documented in [KeyboardAndMemoImplementation.md](KeyboardAndMemoImplementation.md).
They add Form.KeyPreview, Button.Default/Cancel, Memo, IQuadooFormKeyboard (0032)
and IQuadooButton (0033), preserving existing interfaces and export ordinals.

The native implementation covers stages 1, 2, and 4 of the architecture review's implementation sequence. The QuadooScript/Studio bridge is separate shared source in ScriptAdapter. The native DLL has no dependency on the QuadooScript VM.

## Final lifecycle notifications

Visual and nonvisual built-ins expose IQuadooComponentLifecycle (004A), with
AddDestroyedObserver/RemoveDestroyedObserver. IQuadooComponentDestroyedObserver
(004B) receives void OnComponentDestroyed after state Destroyed and native-window,
child and context teardown. These are additive interfaces; existing vtables and
exports are unchanged. Observe COM identity through QueryInterface as usual.

Registration owns a reference; duplicate registration and removal of an absent
observer return S_FALSE. Adding during Destroying/Destroyed returns EDestroyed.
Removal remains supported, and an observer removed before its notification is
skipped. Notifications borrow a callback-protected component, fire once and
release subscriptions, including during construction rollback. All operations
are confined to the component's UI thread. OnDestroy retains its existing earlier
timing, when named child controls remain available.

Native-only hosts choose their own shutdown policy; QuadooForm.dll still does
not post WM_QUIT on ordinary Form destruction. AQVMT's session observes roots
through these published capabilities and exits after the last Form; see
[the lifetime implementation](../../desktop/QuadooStudio/ApplicationLifetimeImplementation.md).
Independent implementations can provide these capabilities without private base
classes, and third-party child controls need no new capability.

## Build and run

Use Visual Studio 2008 and the existing Win32 toolset:

```bat
tests\platform\QuadooForm\build-native.cmd Debug --test
tests\platform\QuadooForm\build-native.cmd Release --test
tests\results\bin\Debug\QuadooFormHarness.exe --show
tests\results\bin\Debug\QuadooFormHarness.exe --show-choices
tests\results\bin\Debug\QuadooFormHarness.exe --show-items
tests\results\bin\Debug\QuadooFormHarness.exe --show-tabs
```

The DLL and harness link against the existing `JSON.lib` and `SimbeyCore.lib` for the selected configuration. Build those platform dependencies first if they are absent. The solution includes the harness and its dependency on QuadooForm. Compiler and linker configuration, Unicode, custom allocation/container classes, and exception settings remain compatible with the original project.

`QuadooFormHarness.exe` runs native behavioral tests by default. `--show` runs an interactive native form with a panel, button, text box, and painted label. `--show-choices` demonstrates two-state/three-state CheckBox controls and independent radio groups in real GroupBox containers. `--show-tabs` demonstrates page switching, page-local children, a hidden tab, and Ctrl+Tab navigation. Debug builds also accept `--trace` to print expected/failing HRESULT check locations. Builds from the Codex sandbox require host execution because the VS2008 program-database service cannot start normally inside that sandbox.

## Published API

`Published/QuadooForm.h` is the active v2 header. `QuadooForm_v2.h` forwards to it. The original is retained as `QuadooForm_v1.h` for reference; do not include v1 and v2 in the same translation unit. Consumers must rebuild for the new interfaces. The GUI container is `IQuadooControlContainer`, avoiding QuadooScript's collection interface name.

The native hierarchy refactor and hardening pass preserved the published v2 ABI. The choice-control pass adds the optional `IQuadooRadioButton` interface (IID ending `0020`) and the `QuadooForm::RadioSelectionProperty` window-property name used to coordinate reentrant group selections. Existing interfaces, IIDs, vtables, structures, export ordinals, and JSON schema are unchanged. Existing clients continue to work; clients using the new capability rebuild with the active header. Invalid metadata is rejected before construction/registration.

| Entry point | Purpose |
| --- | --- |
| `QuadooCreateForm` | Create a runtime top-level Form, optionally with a native owner and C++ event binder. |
| `QuadooCreateFrame` | Create and register a runtime Frame against an existing container. |
| `QuadooGetControlRegistry` | Create a UI-thread-owned registry with fifteen visual and three nonvisual built-in factories. |
| `QuadooCreateConstructionContext` | Select registry, binder, diagnostics, DPI, and Runtime/Design/Preview mode. |
| `QuadooCreateControl` | Construct an entire definition subtree through factories, bind it, initialize it, and activate it transactionally. |
| `QuadooGetFormSerializer` | Obtain the native JSON serializer. |
| `QuadooValidateControlName` | Validate an atomic name change against the containing definition and any Frame attachment namespace. |
| `QuadooCreateIconValue` | Validate DIB ICO bytes, reconstruct omitted 32-bit AND masks, and wrap the normalized bytes as embedded Base64 JSON using SimbeyCore. |
| `QuadooLoadIconValue` | Validate/decode icon JSON and create a caller-owned HICON at the requested size; null clears it. |

Registries are independent objects. To use a registered custom factory, share that registry through `ContextOptions`; convenience creators use their own built-in registry. There is no process-global mutable registry or COM registration.

Components, registries, factories, contexts, property mutation, and event values belong to their creating UI thread. Wrong-thread calls return `QuadooForm::EWrongThread`; the DLL does not marshal calls. Descriptor collections are immutable and may be read across threads. Returned interface pointers are AddRef'd. Returned `RSTRING`s must be released with `RStrRelease` or held by `CRString`. Dynamic strings should be managed `RSTRING`s; tagged static strings must remain valid for their retained lifetime. Stored strings and JSON follow existing repository ownership conventions.

Input definitions are borrowed and never modified. Returned property/default JSON and serialized definitions are deep copies. Caller changes cannot bypass validation, observer notifications, or event rebinding.

Form icon import and embedded icon loading accept complete 32-bit DIB images
whose legacy AND masks were omitted by the ICO writer. The runtime reconstructs
those masks from alpha and updates directory offsets, image sizes and the repaired
bitmap headers in memory. All-zero alpha without a mask defaults to opaque legacy
BGRX. Original files are never rewritten, complete valid ICO bytes are preserved,
and saved icon JSON contains the repaired representation. Truncated pixels,
partial masks, overlapping image ranges and repairs exceeding 1 MiB are rejected.
The public ABI and JSON schema are unchanged.

`QuadooFormDesignerTests.exe --png-only` includes ICO repair tests using
`samples/QuadooUI/SimpleMarkup/main.ico`, synthetic alpha/BGRX and mixed-mask
images, native drawing, JSON round trips, idempotence and malformed-input checks.
Studio integration tests additionally verify settings import and repaired RT_ICON
resources in generated executables.

## Native component architecture

`CControl` supplies shared identity, property storage, notifications, lifecycle, event binding, and logical layout behavior. It implements common service interfaces and does not inherit Form, Frame, Panel, window, or painted-control interfaces. `CWinControl` adds HWND creation, subclassing, focus, tab order, and window messages. `CContainer` adds child ownership, name lookup, registration, painting of static children, and container layout.

The concrete implementations are `CForm`, `CFrame`, `CPanel`, `CScrollingPanel`, `CLabel`, `CButton`, `CTextBox`, `CMemo`, `CCheckBox`, `CRadioButton`, `CGroupBox`, `CComboBox`, `CListBox`, `CTabControl`, and `CTabPage`. Form alone owns close/modal/top-level behavior; Form and ScrollingPanel share CScrollingContainer for their content HWND and scrolling; Label owns text measurement and painting. ComboBox and ListBox share CItemsControl for descriptor transactions and snapshot copying, with concrete native styles, messages and notifications. CheckBox and RadioButton share a native checked-button base; their property validation and selection policies remain in the concrete classes. GroupBox supplies native border/caption painting and content insets. TabControl manages native headers and page selection through public page interfaces; TabPage supplies an ordinary child container with a tab caption and header visibility. Interface adapters forward the published inheritance chains to these shared bases. Each concrete class selects its actual interface chain, and QueryInterface exposes that chain and its implemented optional capabilities. There is no control-kind enumeration or capability filter.

Each built-in factory supplies a `TypeSpec` with reusable base descriptors and local property/event additions or overrides. Factory initialization composes that metadata into its own immutable descriptor sequence. Stable ordering retains the previous per-type property indexes. Every instance allocates property values and notification flags from its factory's sequence; event bindings and argument storage are also dynamic. No fixed global property slots or kind-filtered property/event tables remain.

Native property descriptors can supply an enum-choice sequence, including names and integer values. Normalization, inspection and default validation use that sequence; inherited overrides must preserve its domain. CheckState uses this shared mechanism.

Construction, namespace validation, registration, child traversal, event phases, layout, native child notification routing, and serialization use published interfaces. Layout reads common properties by descriptor name and calls `IQuadooLayoutControl::Measure/Arrange`. Containers subscribe through `IQuadooPropertyNotifications` so edits to external controls trigger layout, and they detect logical bounds/anchor changes through public state. Painted child hit testing uses `IQuadooStaticControl`; optional `IQuadooMessageHandler` receives the matching native input or child notification. External controls may instead process their own HWND messages.

`IControlInternal` and `Internal()` were removed. Optional private services exchange pending-notification state, logical DPI, callback errors, or a copied definition template; none return implementation pointers. They coordinate built-in batching/DPI and preserve unknown design metadata. External controls do not need these services: the framework uses public parent traversal, native DPI fallback, optional layout/notification/message capabilities, and descriptor-based serialization. The serializer uses the same descriptor and child traversal for every control, including built-ins.

`TestExtension/ExternalControl.cpp` builds the independent `QuadooFormTestExtension.dll` against the published header and shared library only. Its `ExternalMeter` has unrelated property indexes, custom `Level` and `Legend` properties, writable `OnPulse.Value` arguments, an `OnFinalizing` event, and its own construction/lifetime implementation. It neither derives from nor includes any native runtime implementation class. The harness loads it by absolute path with LoadLibrary/GetProcAddress, obtains its factory through an export, and exercises it through the normal registry/context/loader and serializer. The DLL exports DllCanUnloadNow and counts all its COM objects, including descriptors and event arguments. Its additional test interfaces/exports are confined to the test extension header and do not change the published runtime ABI.

## Descriptor contract and validation

Every control must provide the seven base descriptors below. They reflect the corresponding public identity/control methods; `Name` is persisted as the definition's top-level `name` field. Window controls also require `TabOrder`.

| Mandatory property | Type | Flags |
| --- | --- | --- |
| Name | String | Serializable |
| Left, Top, Width, Height | Int | Serializable, AffectsLayout |
| Visible | Boolean | Serializable, AffectsLayout |
| Enabled | Boolean | Serializable |
| TabOrder (HWND controls) | Int | Serializable, AffectsLayout |

Other framework properties are optional for independent controls. If supplied, their types and flags are fixed:

| Optional framework property | Type | Flags |
| --- | --- | --- |
| Align | Enum | Serializable, AffectsLayout |
| Anchors | Flags | Serializable, AffectsLayout |
| Padding, Margins | Rect | Serializable, AffectsLayout |
| AutoSize, AutoScroll | Boolean | Serializable, AffectsLayout |
| MinWidth, MinHeight, MaxWidth, MaxHeight | Int | Serializable, AffectsLayout |
| Caption | String | Serializable, AffectsLayout |
| Text | String | Serializable |
| Font | Font | Serializable, AffectsLayout |
| ForeColor, BackColor | Color | Serializable |
| Checked, ThreeState | Boolean | Serializable |
| CheckState | Enum (Unchecked=0, Checked=1, Indeterminate=2) | Serializable |
| Items | List (JSON array of strings) | Serializable |
| ItemIndex | Int (-1 means no selection) | Serializable |
| ActivePageIndex | Int (-1 means no active page) | Serializable |
| TabVisible | Boolean | Serializable, AffectsLayout |
| ModalResult | Long | RuntimeOnly for Form; Serializable for Button |

Controls exposing `IQuadooRadioButton` must advertise `Checked` with the contract above. CheckState descriptors expose exactly the three values in the indicated order.

Controls exposing `IQuadooComboBox` or `IQuadooListBox` must advertise `Items` and `ItemIndex` with these contracts; ComboBox also requires `Text`. These are optional capability interfaces, not a dependency on the native classes. Framework names retain their types and flags for independent factories too.

Controls exposing `IQuadooTabControl` require `ActivePageIndex`; controls exposing `IQuadooTabPage` require `Caption` and `TabVisible`. These contracts also apply to independently implemented tab/page capabilities.

Framework properties may additionally be Hidden. Other flags, including ReadOnly or a design/runtime restriction on the seven writable base properties, are incompatible. Align publishes six values in order 0..5; Anchors publishes four values 1, 2, 4, 8. Enum labels may be localized but must be unique nonempty wide strings. Insets and font defaults use the existing JSON structures and validation rules.

Names are case sensitive, unique nonempty wide strings up to 256 characters. Duplicate names are rejected even if their types match. Custom names may use the published property types and flags, with valid defaults; RuntimeOnly cannot also be DesignOnly or Serializable. Every factory event has exactly one corresponding Event property, and event/argument names are unique within their collections. Property/event counts are limited to 4096 and event arguments to 256. Factory metadata is immutable after registration. Instance descriptor order, names, types, flags, defaults, and enum choices must match the factory.

Validation runs before factory registration, before construction, on returned controls, on container registration, and before serialization. Optional QueryInterface failures are distinguished from E_NOINTERFACE; successful null interface/descriptor results and invalid notification indexes are rejected. Built-in TypeSpec composition additionally checks native base requirements, compatible overrides, duplicates, null metadata, and bounded inheritance depth. Built-in storage accessors use HRESULT checks before indexing, including in Release builds. Native QueryInterface implementations use BEGIN_UNK_MAP-based maps and chain macros; all paths share the CBaseUnknown identity and reference count.

`QuadooProperty::Memo` is appended as value 17 for multiline text. Existing enum
numbers, interfaces, structures and exports are unchanged. Memo uses a wide JSON
string, defaults to an empty string, and supports GetValueString/SetValueString
and GetValueJSON/SetValueJSON. Newlines and Unicode round-trip without a new JSON
representation. Framework Name, Caption and Text descriptors remain String; Memo
does not permit incompatible overrides of reserved properties. Custom factories
can advertise Memo on their own property names. The designer opens Memo in its
multiline dialog, while ordinary single-line String/Event values edit inline.
Plugins using Memo require the updated runtime; older runtime versions reject the
previously unknown property type during descriptor validation.

## Definitions and identities

```json
{
  "schemaVersion": 1,
  "type": "Form",
  "name": "MainForm",
  "properties": { "Caption": "Native application", "Width": 640, "Height": 480 },
  "events": {},
  "children": [
    {
      "type": "Button",
      "name": "btnOK",
      "properties": { "Caption": "OK", "Left": 20, "Top": 20, "Width": 100, "Height": 28 },
      "events": { "OnClick": "Button_Click" }
    }
  ]
}
```

Supported visual built-ins are **Form, Frame, Panel, ScrollingPanel, Label, Button, TextBox, Memo, CheckBox, RadioButton, GroupBox, ComboBox, ListBox, TabControl, TabPage**. Names and property/event names are case sensitive. Each named component, including the root, must be unique within its Form/Frame definition. An absent ID gets a new GUID; an absent name gets a type/GUID-based name. Serialization persists IDs and names. Property indexes are local to the type's descriptor collection; use names to identify properties across versions.

`GetChild` enumerates direct children in registration order. `FindControl` indexes the queried subtree and stops at Frame boundaries. A parent can find a frame's attachment name, but cannot find its private controls through that frame. Rename validation is atomic. Duplicate names and duplicate IDs fail without committing a new registration/name.

A GUID is persistent **definition identity**, scoped by its Form/Frame root instance. Live object identity is canonical `IUnknown`. Two instances of one frame can reuse their definition GUIDs, while their attachment names must be distinct in the parent namespace. The caller can clone a Frame definition and override its root name/placement before instantiation.

**Frames are runtime attachments.** `Frame` and `FrameInstance` are rejected in definition child arrays. Create Frame roots explicitly against a live container. Each scripted Frame will have its own script object/interface/binder when stage 3 is implemented; native C++ frames require no script object. Form serialization omits attached frames and their complete subtrees. Serializing a Frame root saves its own definition, omitting descendant runtime frames.

Serialization writes all supported stored properties, in descriptor order, and children in registration order. It does not serialize effective bounds, DPI, scroll position, or modal results. Unknown root metadata is preserved. Runtime and Preview reject unknown properties/events; Design preserves unknown property/event fields for later editor round trips. Only schema version 1 is supported.

## TabControl and TabPage

These controls follow the VCL TPageControl/TTabSheet model: a native tab strip owns real page containers, and each page owns its own controls. TabPage can only be attached to a parent exposing `IQuadooTabControl`; TabControl accepts only children exposing `IQuadooTabPage`. Put ordinary controls inside a page. Pages are ordinary serialized definition children, unlike runtime-only Frame attachments.

The additive public capabilities are `IQuadooTabControl` (IID ending `0023`) and `IQuadooTabPage` (`0024`), both inheriting `IQuadooControlContainer`. Existing interfaces, structures, exports and ordinals are unchanged. TabControl provides GetPageCount/GetPage, GetActivePage/SetActivePage, GetActivePageIndex/SetActivePageIndex, SelectNextPage, MovePage, and UpdatePage. TabPage provides GetPageIndex/SetPageIndex and GetTabVisible/SetTabVisible. Returned pages are AddRef'd; GetActivePage succeeds with null when no page is selected. SetActivePage(null) clears selection.

`ActivePageIndex` is a serializable Int and refers to all pages in child order, including hidden headers. It accepts -1 or an existing page index; invalid explicit indexes return E_INVALIDARG. Its descriptor default is -1. If the initial JSON omits this property, the first header-visible page is selected after construction; an explicit -1 preserves no selection. Adding a visible page to a ready control with no selection selects that page. Removing the active page or hiding its header selects the next header-visible page in order, wrapping, or clears selection if none remains. Removing/reordering earlier pages updates the index while preserving the active page's identity. SetPageIndex/MovePage changes child/header order, which is persisted by JSON serialization; no separate PageIndex property is stored.

`TabVisible` defaults to true and controls the header independently of the page's stored Visible value. A hidden-header page remains selectable programmatically by object or all-page index, matching [VCL TabVisible](https://docwiki.embarcadero.com/Libraries/Athens/en/Vcl.ComCtrls.TTabSheet.TabVisible). Keyboard navigation skips hidden headers. The framework's common Visible descriptor remains the requested visibility: inactive pages retain Visible=true in JSON while their HWNDs are hidden. Only the active, requested-visible page is shown. Page OnShow/OnHide follow actual local HWND visibility changes; parent hiding does not rewrite the stored property.

User mouse/keyboard navigation raises cancellable `OnChanging` with writable Boolean `Cancel`, then commits selection/visibility/layout and raises argument-free `OnChange`. Cancellation or a failing OnChanging leaves selection unchanged. Programmatic setters, SelectNextPage, header visibility edits, page movement and destruction do not raise these selection events. This follows the [VCL user-navigation event contract](https://docwiki.embarcadero.com/Libraries/Florence/en/Vcl.ComCtrls.TPageControl.OnChanging). Initial selection is silent. Property observers still receive committed ActivePageIndex changes under the normal batching rules. OnChange failure leaves the committed selection in place. Reentrant selection/page-structure changes during selection callbacks return EInvalidState; destruction remains supported, including destruction of the incoming page or the whole tab control.

Arrow keys and Home/End navigate while the tab strip has focus. Ctrl+Tab and Shift+Ctrl+Tab navigate from the strip or built-in page descendants, including an editable ComboBox's edit child. Nested tab controls use the nearest containing tab control. A page remembers its last focused live descendant; switching away from focused page contents restores the destination page's saved focus or its first dialog tab stop. Switching from the strip keeps strip focus. Hosts retain their usual IsDialogMessage loop. Independent HWND controls that consume dialog keys must forward Ctrl+Tab/WM_GETDLGCODE to their containing tab control's public message capability or implement equivalent host routing.

The native class is WC_TABCONTROLW with top horizontal tabs. `MultiLine` and `HotTrack` are Boolean properties, default false. Native header rectangles determine page bounds through TCM_ADJUSTRECT, then TabControl.Padding insets the display area. Pages fill this area regardless of their stored bounds/Align; stored geometry is preserved for deterministic persistence. Page.Padding applies to its children's ordinary docking layout. Requested child anchors establish their baseline after the page has its initial display bounds. Font, DPI, multiline and size changes recompute the display area. Top-only positioning follows [TCM_ADJUSTRECT's documented support](https://learn.microsoft.com/en-us/windows/win32/controls/tcm-adjustrect). Images, owner drawing, side/bottom tabs and detachable/reparentable pages are not included.

Page registration reserves storage and stages native headers and candidate arrangement before publishing ownership. Native insertion, candidate layout or observer-subscription failures remove the staged header and preserve the previous child list, active index, native selection and serialized tree. Caption/header-visibility edits and reorder insertion also stage native changes before committing. TabPage calls public UpdatePage while its proposed Caption/TabVisible value is readable, before publishing notifications; a failed call rolls its proposed value back. Independent page implementations should use the same protocol and normal lifecycle/unregistration contract. Public WM_NOTIFY routing and layout/serialization require no internal implementation pointer. Raw TCM_SETCURSEL synchronizes logical selection silently; raw insert/delete/set-item messages are rejected so header state cannot diverge from pages.

```json
{
  "type": "TabControl",
  "name": "settingsTabs",
  "properties": { "Align": "Client", "ActivePageIndex": 0 },
  "events": { "OnChanging": "Settings_Changing", "OnChange": "Settings_Change" },
  "children": [
    {
      "type": "TabPage", "name": "generalPage",
      "properties": { "Caption": "General" },
      "children": [
        { "type": "TextBox", "name": "userName", "properties": { "Left": 12, "Top": 12 } }
      ]
    },
    { "type": "TabPage", "name": "advancedPage", "properties": { "Caption": "Advanced" } }
  ]
}
```

`tests\results\bin\Debug\QuadooFormHarness.exe --show-tabs` runs the interactive example. Automated tests use the same definition and additionally host the independent ExternalMeter DLL in a page.

## ComboBox and ListBox

The item-control pass adds `IQuadooComboBox` (IID ending `0021`), `IQuadooListBox` (`0022`), and `QuadooForm::ComboBoxStyle`. All previous IIDs, vtables, structures, exports and ordinals remain unchanged. Include `Published\SimbeyCore.h` to use `IRStringArray`; QuadooForm forward-declares that existing interface and introduces no new collection interface or QuadooScript dependency.

Both interfaces provide `GetItems(IRStringArray**)`, `SetItems(IRStringArray*)`, `GetItemIndex(INT*)`, and `SetItemIndex(INT)`. GetItems returns a fresh array and fresh wide strings. SetItems reads the published interface, copies its contents, and retains no reference to that input. Editing either the input or a returned snapshot has no effect until SetItems. UTF-8 RSTRING inputs are converted to wide text. Empty strings, Unicode and duplicates preserve insertion order; there is no native sorting. Embedded NULs, null items, non-string JSON items, and negative/oversized collection counts are rejected. The current count and per-string length limits are 1,000,000. ListBox supports single selection.

`IRStringArray::Compare` can be used to compare snapshots: S_OK/0 means equal and S_FALSE with a negative or positive result gives their ordering. The harness uses it to verify equal, changed, empty and prefix snapshots, independent array implementations, and balanced string references. The corrected SimbeyCore implementation preserves length ordering and clears released temporary references. The native Items transaction still compares its descriptor JSON values directly; converting those values into two arrays solely to compare them would add unnecessary allocations. GetItems/SetItems continue copying through Length/GetRString without requiring a caller's Clone, Compare or GetArray implementation.

`Items` is a serializable List property represented by an ordinary JSON array of strings. `ItemIndex` is serializable, defaults to -1, and accepts -1 or an existing index. An invalid explicit index fails without a change. Replacing Items retains the numeric index if it still exists, otherwise clears selection. GetValueJSON returns an independent array, and SetValueJSON follows the same copying/validation/selection rules as SetItems. No collection-specific serializer path is required.

ComboBox has `Style`: DropDown=0 (editable, default) or DropDownList=1 (selection only). JSON accepts either label or number and serialization writes the number. `Text` follows the selected item. Editing text that differs from the selected item clears selection; arbitrary editable text with ItemIndex=-1 is persisted. Clearing selection or replacing Items retains free text in DropDown mode. DropDownList has empty Text when unselected, rejects arbitrary text assignments, and switching to that style clears unselected free text. A selected ItemIndex takes precedence over a stale Text value during construction. `DropDownHeight` defaults to 160 logical pixels and describes the additional popup height; logical Height describes the collapsed control. Native minimum field/border sizes still apply at very small heights.

Items, ItemIndex, Style and Text changes commit together before observers and logical events. `OnSelectionChange` (no arguments) fires once per actual index transition, for both programmatic and user changes. ComboBox `OnChange` fires once per Text transition, after OnSelectionChange when both change. Initial values and unchanged setters emit neither event. Replacing the string at the same selected index changes Text, but does not change ItemIndex. Observer notifications follow the existing BeginUpdate batching rules; logical events follow the existing immediate-after-commit policy. Reentrant item/selection/text/style changes that would change state return EInvalidState while a transaction notifies callbacks; unchanged requests return S_FALSE. Destruction is permitted during callbacks. Callback failures leave committed state in place and stop remaining event delivery.

Items and Style updates stage a fully populated native HWND before retiring the old HWND. If staging fails, the old window, properties, registration and serialized state remain unchanged. Successful replacement preserves effective bounds, z-order, local visibility, enabled state and focus without another lifecycle event. Reacquire GetHandle after Items/Style updates; cached HWNDs and extra state set directly on them are not retained. ComboBox's edit child forwards common key events and supports Handled; focus can reside in that child.

Native CB_SETCURSEL/LB_SETCURSEL and CB_SELECTSTRING/LB_SELECTSTRING also synchronize logical selection. Native selection/edit notifications are deduplicated against logical state. Raw native item-mutating messages (ADDSTRING, INSERTSTRING, DELETESTRING, RESETCONTENT, directory/count population) are rejected: edit a snapshot and call SetItems so descriptors and JSON remain consistent. The controls are ordinary text-only COMBOBOX/LISTBOX windows, with no owner drawing, ComboBoxEx, multi-selection or image-list support.

Example properties: `{"Items":["Zulu","Alpha","Alpha",""],"ItemIndex":1,"Style":"DropDownList"}` on a ComboBox. `tests\results\bin\Debug\QuadooFormHarness.exe --show-items` displays editable and noneditable ComboBoxes and a ListBox.

## CheckBox, RadioButton, and GroupBox

CheckBox is a native BUTTON (`BS_CHECKBOX` or `BS_3STATE`) exposing `IQuadooWinControl`. `CheckState` is its single stored state: Unchecked=0, Checked=1, Indeterminate=2. JSON accepts the enum names or numbers and serializes numbers. `ThreeState` defaults to false; CheckState defaults to 0. A two-state control rejects Indeterminate. Turning ThreeState off while indeterminate returns E_INVALIDARG and preserves both values; set CheckState to 0 or 1 first. Click/Space cycles 0→1→0 or 0→1→2→0. ThreeState changes the native button style immediately.

RadioButton is a native BUTTON (`BS_RADIOBUTTON`) exposing `IQuadooRadioButton` and its inherited window/control interfaces. `Checked` defaults to false. Immediate logical siblings exposing that capability form one group, regardless of registration adjacency or native WS_GROUP bits. A nested Panel, Frame, GroupBox, or other container has a separate group. Selecting a radio clears checked siblings, including disabled/hidden siblings. Setting false can leave an empty group; clicking an already checked radio leaves it checked. Conflicting initial Checked=true definitions are rejected before application callbacks. Creating an initially checked radio during an in-progress selection returns EInvalidState without registering it. To select a newly attached radio in an existing group, construct it unchecked, then call SetChecked(TRUE).

Arrow keys traverse eligible radio siblings by TabOrder, with registration order breaking ties. They wrap, skip hidden/disabled controls, select the destination and move focus. Space and native activation follow the same selection rules. OnKeyDown.Handled suppresses arrow navigation. A group contributes one native tab stop: its checked eligible radio, or the first eligible radio if none is checked. GroupBox itself is not a tab stop; WS_EX_CONTROLPARENT enables traversal of its children. Hosts use IsDialogMessage as for the existing controls.

Both stateful buttons expose `OnChange` (no arguments) and `OnClick` (the common Handled argument). After initialization, an actual state transition updates storage and HWND, notifies property observers, then raises OnChange. Programmatic property edits, SetChecked and BM_SETCHECK raise no OnClick. Initial values and unchanged assignments raise no OnChange. User activation raises OnClick after any successful state changes; selecting an already checked radio still raises OnClick. OnClick does not cancel the committed state transition. Arrow activation raises OnClick on the destination. Destruction or a callback failure ends the remaining activation sequence.

Group selection clears siblings before selecting the destination, so callbacks can observe an empty group but never two checked built-ins. A parent HWND property, named by the published `RadioSelectionProperty` constant, guards the selection. A different nested TRUE selection returns EInvalidState; setting FALSE remains permitted and unchanged assignments return S_FALSE. External radio implementations use the same public capability/property protocol and must keep HWND, Checked and descriptors consistent. If a sibling callback fails after its state has committed, selection completes and returns the callback failure. A sibling that refuses to clear prevents selecting the destination. Application callbacks run after committed changes and do not roll them back.

GroupBox is a real `IQuadooControlContainer` with a native BUTTON (`BS_GROUPBOX`) HWND and the shared Caption/Padding/AutoSize properties. Native children use its HWND as their physical parent; painted children participate in its painting and hit testing. Docking reserves 8 logical units on the left, right and bottom, and one caption-font height plus 4 on top, in addition to Padding. Font and DPI changes recompute the caption inset. Absolute/anchored child coordinates remain relative to the entire child-host client area, as for other containers. Serialization includes its ordinary children; ownership and recursive destruction follow the normal container contract. Runtime attachment and property edits propagate layout through ready nested containers, while explicit BeginUpdate batches still defer it.

## Construction, factories, and ownership

The transaction runs these phases:

1. Validate identity and properties, create native windows, and register the whole tree. Child HWNDs have their physical parent at construction time.
2. Resolve every nonempty callback name before application initialization. Empty names are unbound; named unresolved handlers fail creation.
3. Perform initial layout, then call `PostBind` once for the completed root in Runtime mode.
4. Initialize controls in reverse construction order, giving children their `OnCreate` before their parent.
5. Activate visibility and flush pending notifications after every component is ready.

On failure, the subtree is aborted and children, native windows, registrations, delegates, and references are unwound. Diagnostics provide construction paths and component/event details through `IQuadooConstructionReport`. A parent construction transaction also recovers newly registered controls from a factory that fails without returning one; factories remain responsible for cleaning up resources they never registered.

Factory `CreateControl` is the low-level operation: it returns an already registered **Constructing** control and must not emit application callbacks. All factory-created objects must expose `IQuadooConstructionParticipant` for the common Bind/Initialize/Activate/Abort phases. Use `QuadooCreateControl` for ordinary dynamic creation. Custom controls can expose `IQuadooLayoutControl` to measure/arrange without modifying stored bounds and `IQuadooChildWindowHost` to host native children. Custom setters should use the name-validation helper before committing names.

Containers retain their children; a child's logical parent is weak and immutable. Registration checks parent identity, state, name/ID scope, and HWND parentage. Repeated registration fails. Ordinary unregistration of a live control fails; use `Destroy`. A destroyed child is unregistered, and its weak parent link is cleared. There is no supported reparenting operation.

Registration stages all interface/metadata/namespace checks, reserves the child entry, and subscribes an inactive observer before publishing anything. Identity, state, parent, and metadata are checked again after subscription. Failed registration preserves child order/count, lookup results, layout state, subscriptions, and container-owned references. The observer is detached on failure even when a third-party AddObserver attaches before returning failure. Reentrant child registration/unregistration, batching changes, container destruction, and layout execution are blocked during staging; reads see the old tree. Unregistration uses the retained notification interface and observer rather than querying the child again.

An implementation that also refuses RemoveObserver may retain an inert observer until it releases its subscriptions. That observer cannot call or retain the parent. Such a control must still be corrected by its provider; the framework cannot force another COM object to release a reference it owns.

Built-in controls retain their factory until final interface release, including when destroyed. `IQuadooFactoryLifetime` reports live objects; registry removal fails while that count is nonzero and is rejected for factories that do not supply this capability. Registry removal releases a registry reference and does not unload a plugin. Hosts/custom factories must keep code modules loaded while any associated factories, descriptors, components, contexts, event arguments, or delegates remain referenced. The runtime DLL is intended to remain loaded for the application lifetime in this pass.

## Events and destruction

All built-in IQuadooWinControl types inherit OnEnter and OnLeave descriptors.
OnEnter fires when focus enters the control or one of its native descendants;
OnLeave fires when focus leaves that subtree. Containers enter before their
children and leave after them. Moving between siblings keeps common containers
entered. Internal editable ComboBox HWND changes and native-handle replacements
do not duplicate these logical events. Handlers may redirect focus or destroy a
control; dispatch retains the traversal snapshot and abandons stale transitions.
OnFocus/OnBlur retain their direct native notification behavior. Labels, other
windowless controls and nonvisual components have no OnEnter/OnLeave descriptors.
Independent HWND implementations remain responsible for their own focus events,
as with their existing native input/event handling. No public vtable was added.


The C++ binder receives callback names only. Delegates receive typed mutable event arguments with sender and event metadata. Built-ins expose `OnCreate`, `OnDestroy`, visibility events, focus/key events for HWND controls, `OnClick` for Button/Label, `OnChange` for TextBox/Memo, and Form close events. `OnCloseQuery.Cancel` is writable Boolean; key events have a readonly integer `Key` and writable Boolean `Handled`. Wrong types, invalid indexes, and attempts to write readonly arguments fail.

The eventual script adapter will use the user-specified protocol: `Form.Create(this)` obtains the event interface through `IQuadooObject::GetInterface`, resolves callback names with `IQuadooInterface::ResolveMethod`, and wraps that interface with its `ULONG` ordinal. The designer will generate named-control variables/property variables at design time; `PostBind` will populate them before `OnCreate`. No adapter or QuadooScript change is implemented here.

Changing an event property on a live control resolves the replacement delegate before committing the new callback name. A failed replacement leaves the previous binding intact. Native dispatch holds the delegate and sender alive through reentrant callbacks. A callback can destroy its sender or its form safely.

`HideForm` preserves the HWND; `Close` performs the cancellable query, then `OnClose`, then destruction. Cancellation returns `S_FALSE`. A failing query leaves the form alive. An accepted close still destroys the subtree if a close/destroy notification fails, returning/reporting that failure. `Destroy` bypasses the close query and is idempotent (`S_FALSE` for repeated/reentrant destruction). A referenced destroyed object retains identity, metadata, and stored values; HWND-dependent operations return `EDestroyed`. Destruction releases delegates, observers, and construction-context links.

Observers are retained until removal/destruction. Dispatch snapshots references and skips subscriptions removed during a callback. Nested container `BeginUpdate`/`EndUpdate` coalesces stored-property, event-property, layout, and effective-bound notifications throughout the built-in subtree. Stored-property notifications precede the corresponding effective-bound notification. Observer errors are returned after committed updates; they do not revert stored values. Reentrant notification changes are coalesced with a bounded pass count.

`ShowModal` runs an auxiliary message loop, temporarily disables an enabled owner, and restores owner state on every exit. Set the runtime-only Long `ModalResult` property and call `Close` to complete it. Close-query cancellation keeps the loop running. Reentrant modal calls fail. `WM_QUIT` destroys the modal form, restores the owner, returns `HRESULT_FROM_WIN32(ERROR_CANCELLED)`, and reposts the quit code for the primary host loop. The DLL never owns the primary application message loop or calls `PostQuitMessage` when an ordinary form closes.

PostQuitMessage records a deferred quit request. The outer host loop must continue normal message retrieval; pending ordinary posted messages can delay generation of WM_QUIT. In particular, an immediate PeekMessage filtered only to WM_QUIT is not a reliable propagation test on Windows XP. The native harness pumps with no filter and checks the exact exit code once the quit is retrieved.

## Layout, scrolling, and DPI

Requested and effective geometry are separate. All stored bounds, padding, margins, scroll offsets, and layout measurements are 96-DPI logical units. Native child positions are effective logical bounds converted to physical coordinates. A Form's Width/Height are its client size; its Left/Top locate the outer window. Native user resizing changes effective Form size without overwriting requested geometry.

- Dock in child registration order using `None`, `Top`, `Bottom`, `Left`, `Right`, or `Client`. Padding reduces the dock/content rectangle. Margins inset dock allocations. A container permits one Client child.
- Absolute/anchored Left/Top are coordinates in the child-host client area. They already include any intended inset; padding does not silently add an offset. Margins contribute to measured content extents. Anchors apply for Align=None and retain a baseline across parent resizing; explicit child bounds/anchor edits reset that baseline.
- Min/max constrain effective sizes. Forms also apply those constraints to native resize tracking. A minimum larger than remaining dock space can extend into clipped space. Docking overrides anchors.
- Label AutoSize measures natural text including explicit line breaks, without automatic word wrapping. Panel, ScrollingPanel, and Frame AutoSize fit their children's requested/measured extents plus padding/margins; Form AutoSize is not exposed. Docking can override the measured dimension on its stretch axis.
- Font JSON has `Name`, integer point `Size`, `Bold`, and `Italic`. Fonts are per control. ForeColor/BackColor use Win32 COLORREF integer encoding; native themed controls retain the OS's rendering behavior. Labels and container backgrounds are painted by the runtime.
- A ScrollingPanel has distinct viewport and content HWNDs. Native children are parented to the content HWND. Native notifications and painted-control input are routed through that host. Scrollbars and wheel input move the content offset without rewriting any child geometry. Nested panels have independent offsets.

`ContextOptions.DPI=0` selects parent/native window DPI dynamically. Explicit DPI allows deterministic scales for native previews/tests. Parent layout propagates its DPI to children. The core handles Form `WM_DPICHANGED`, recreates fonts, applies the suggested window rectangle, and relays out descendants. `GetDpiForWindow` and `AdjustWindowRectExForDpi` are loaded dynamically; older systems use device/system DPI and `AdjustWindowRectEx`. No newer SDK is required. The **host** chooses process DPI awareness. An external design host relays its DPI/scale changes using `IQuadooLayoutControl::Arrange`; designer zoom is a separate host concern and is never stored as component geometry.

## Validation and deferred work

The harness verifies real HWND ownership/parenting and notifications, eager binding failures before OnCreate, rename/registration rollback, destruction during callbacks, namespace/GUID behavior for two Frame instances, deep-copy and deterministic JSON round trips, dock/anchor/margin/min/max/AutoSize behavior, physical scaling at 96/144/192 DPI, nested scrolling without geometry drift, observer batching/removal, wrong-thread errors, external HWND destruction, modal owner restoration and quit propagation, custom factory recovery, design-mode suppression, focus/tab order, and reference/resource cleanup. Independent extension tests cover custom metadata and defaults, construction phases, property editing, event replacement/rollback, writable custom arguments, painting/input, dock/anchor/AutoSize/DPI layout, serialization/reconstruction, destruction, and factory retention after registry/context release. Capability tests verify supported and unsupported interfaces, canonical IUnknown identity, and factory/instance metadata and defaults for the original fourteen visual built-ins; Memo has additional keyboard-suite coverage. Tab tests additionally cover cancellable native navigation, silent setters, hidden header indexes, focus restoration, page reordering, initial/dynamic anchors, extension children, and native header/layout/observer failure injection.

The resource tests pump messages while focused native controls remain alive, then verify each captured control/child-host HWND is destroyed and each relevant factory has zero live instances. Process GDI/USER counts are sampled while pumping and yielding, until unchanged for 100 ms, with a two-second limit. Warm-up requires three identical end-of-cycle samples within at most eight complete workloads. The resulting baseline is frozen for 20 further cycles; growth is never accepted by raising that baseline. Each measured sample must settle at or below it within two seconds.

Resource-monitor failure injection retains a real USER window, schedules timer-driven destruction, leaks one new window per warm-up cycle, and adds a retained window after a baseline is frozen. These cases verify that deferred cleanup can progress, retained resources are still rejected, and persistent growth cannot be hidden by indefinite warm-up. Failures report baseline/actual GDI and USER counts, warm-up history, and surviving process window classes/handles. Assertion totals can vary with the number of warm-up cycles required by the host.

The build script also runs `QuadooFormNativeTests.exe`, which links the native implementation separately to exercise missing/empty storage, invalid indexes, malformed/cyclic native metadata, and compatible default overrides in both configurations. These private tests are separate from the independent DLL. Extension tests inject failures into identity, parent/state/name/ID/property access, optional interfaces, descriptor collections, observer attachment/rollback, and metadata changes. COM tests query every supported interface from every other path and verify canonical identity and balanced references. Unload tests retain event arguments and descriptors beyond component destruction, then release everything, unload the DLL, and reload it successfully.

Two integration constraints remain relevant to a future designer/plugin host. Factory live count covers components; module unloading must also account for referenced descriptors, event arguments, delegates, and other plugin objects (DllCanUnloadNow or equivalent host ownership). Also, generic external serialization preserves advertised properties/events; unknown top-level JSON fields and unadvertised design properties can now use the optional published IQuadooDefinitionSource template capability. See ComponentExtensibility.md for nonvisual factories, root component ownership, file dialogs, Timer and design fallback. External notifications also retain their implementation's batching policy; the private built-in batching service is not required of plugins.

The compiler/SDK and tests are verified on the current Windows host. The user has also exercised the harness on XP; the updated resource checks, item controls, tabs and designer prerequisites still require an XP rerun. Physical monitor transitions and other older Windows versions have not been exercised locally; DPI test scales are explicit. Actual x64 configurations are not supplied. The standalone Phase A/B designer is documented in DesignerReadme.md. ListView, source generation, Studio integration, and release/debug application runners remain later stages. Stage 3 scripting integration is intentionally deferred.

## Designer runtime prerequisites

The Phase A/B pass adds optional interfaces without modifying existing vtables or
structures: IQuadooDesignCapabilities (IID ending 0025), IQuadooModalDialog (0026),
IQuadooFactoryCatalog (0027), and IQuadooDesignFactory (0028). Icon helper exports
use ordinals 12 and 13; existing ordinals 5 through 11 remain unchanged. Ordinary
Form/Button property indexes are retained. Added properties precede event-property
entries, so those indexes shift; always resolve descriptor names across versions.

Form now exposes BorderStyle (None/Single/Sizeable/Dialog), BorderIcons flags
(SystemMenu=1, Minimize=2, Maximize=4, Close=8), WindowState
(Normal/Minimized/Maximized), FormStyle (Normal/StayOnTop), AutoScroll and Icon.
Defaults are Sizeable/15/Normal/Normal/false/null. All are serializable, while the
existing Form.ModalResult remains runtime-only. Fixed/sizable border changes use
SWP_FRAMECHANGED and preserve interface/HWND identity, client allocation and child
hierarchy. Close availability controls SC_CLOSE; programmatic Close remains valid.
StayOnTop uses native topmost placement. No MDI behavior is claimed.

Icon is null or `{ "format":"ico", "encoding":"base64", "data":"..." }`.
SimbeyCore encodes/decodes canonical padded Base64. Validation bounds the ICO to
1 MiB and 64 nonoverlapping images and accepts uncompressed DIB 1/4/8/24/32-bit
images with masks. PNG/compressed images are rejected. Both runtime icon sizes
are owned, replaced transactionally, refreshed on native DPI changes, and released
on native destruction even while interface references remain.

Button.ModalResult is a serializable Long with mrNone/mrOk/mrCancel/mrAbort/mrRetry/
mrIgnore/mrYes/mrNo choices. OnClick runs first. If the button survives, the current
nonzero result requests completion of the nearest logical ancestor Form through
IQuadooModalDialog. That interface reuses Close and OnCloseQuery; cancellation
restores the prior pending result. mrNone, property edits, and nonmodal clicks do
not close the Form. Preview suppresses application delegates but retains this
built-in behavior and uses a genuine top-level modal window.

Form always has a child content host and exposes IQuadooScrollable. AutoScroll
defaults false to preserve existing definitions. When enabled, it shares the
ScrollingPanel viewport/content layout, wheel, extent and focus-scroll behavior.
Visible scrollbars reduce the viewport within the allocated client size, and
neither scrolling nor native resizes overwrite stored child coordinates.

Design Form and standalone Design Frame roots construct directly as child windows
under ContextOptions.hwndHost; no reparenting is used. Preview's host is a native
owner. Runtime Frames still require a parent. Design capabilities report available
move/resize axes, including root, docking, AutoSize and ComboBox restrictions.
Optional factory placement queries provide palette feedback; CreateControl remains
authoritative. The factory catalog retains each returned factory normally.

## Native menu components

MainMenu, PopupMenu and MenuItem use the ordinary component registry, descriptor
properties, GUID serialization and event binding. See [MenuSupport.md](MenuSupport.md)
for the JSON hierarchy, optional interfaces, uniqueness scope, native ownership,
shortcuts and designer behavior. Existing interface vtables are unchanged.

## Native keyboard ownership

`IQuadooKeyboardInput` is an optional published control capability (IID ending
0048). It adds no methods to existing interfaces and requires no private runtime
base class or HWND registration map. The Form's existing keyboard tree resolves
each target HWND to the nearest registered control wrapper, including nested
native child HWNDs that are not themselves QuadooForm components. Resolution
crosses Frame boundaries for keyboard routing, as before.

For eligible controls in the receiving Form, `OwnsKeyboardMessage(const MSG*,
BOOL*)` is queried before native menu/popup shortcuts, Default/Cancel handling,
KeyPreview and dialog translation. TRUE leaves those paths untouched and returns
`PreprocessMessage`'s handled result as FALSE, so the normal host message loop
can translate and dispatch the original message once. FALSE, or an absent
interface, retains normal Form routing. Host accelerators still precede Form
preprocessing under the existing `IQuadooFormKeyboard` contract. Other Forms and
HWNDs outside the receiving Form are not queried.

The query is read-only, UI-thread-affine and receives a borrowed message. Controls
must not consume/dispatch it, modify or retain it, or change component state from
the query. A query failure propagates and marks the message handled; defensive
checks also stop recursive preprocessing and dispatch to destroyed targets.
No window handles or capability references are retained across messages, so
native child creation/destruction and replacement HWNDs need no map maintenance.

ListView implements the capability by comparing the target to its current native
label editor, keeping the editor-specific policy inside the wrapper. The Form's
keyboard dispatcher contains no ListView window-class checks or ListView messages.
Stored and OwnerData editors retain native Enter/Escape commit/cancel behavior.

`QuadooFormNativeTests --keyboard-input-only` exercises a non-ListView TextBox
wrapper and nested native children, native character dispatch, KeyPreview,
Default/Cancel, menu shortcuts, capability opt-out, unrelated HWNDs, reentrancy,
failure recovery, COM identity and destruction. These tests also run in the full
private suite; the public harness additionally checks ListView capability
identity and label-editor lifetime.

VS2008 Win32 Debug and Release validation passes the focused ListView/OwnerData,
Designer and Studio workflows, complete native harness (9,422 assertions), full
private suite (833 assertions, including 22 generic keyboard-capability checks),
and application/script suite (113 assertions). No new Win32 APIs were introduced;
the existing XP target is preserved. SimbeyCore was not rebuilt.
