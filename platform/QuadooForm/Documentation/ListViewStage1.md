# ListView stage one: contracts and compatibility probes

Date: 2026-10-25.

This report records the stage-one boundary. Stage two subsequently publishes
the reviewed declarations and adds the empty native control/columns foundation;
see [ListViewStage2.md](ListViewStage2.md) for the current implementation scope.

Status: stage one is complete. The interface/descriptor/script contracts and local
Win32 Debug/Release probes are validated, and the user supplied passing native
probe logs from Windows XP. No ListView factory, designer tool, or production
script wrapper has been registered.

## 1. Deliverables and ABI boundary

The exact proposed C++ declarations are in
[ListViewContract.h](../../tests/platform/QuadooForm/ListViewCompatibility/ListViewContract.h). The probes
compile this file against the current published interfaces. It is deliberately
outside Published and is not included by QuadooForm.dll.

| Proposed interface | UUID suffix | Purpose |
| --- | --- | --- |
| IQuadooListView | 0043 | Collections, selection/focus, image roles, editing, refresh, batching, virtual writeback |
| IQuadooListItems | 0044 | Stored row operations and virtual count/indexed access |
| IQuadooListItem | 0045 | Caption/subitems, images/state, runtime payload, stable stored identity or a virtual snapshot |
| IQuadooListColumns | 0046 | Logical column collection and GUID lookup |
| IQuadooListColumn | 0047 | Caption, logical width/alignment/index, display position, autosizing |

These UUIDs use the existing QuadooForm UUID prefix and are proposals reserved
in this draft, not deployed interface changes. Stage two can append the reviewed
declarations to Published/QuadooForm.h. Existing vtables, UUIDs, exports, ordinals,
property-type enum values, and EventValueType remain unchanged.

No new ImageList or event-object interface is needed. Reuse IQuadooImageList,
IQuadooImageListSnapshot, IQuadooImageListObserver, IQuadooEventObjectArgs, and
IQuadooPopupMenu. The production event adapter's TreeNode-only ValueObject branch
still needs the capability-based dispatcher in stage three.

## 2. Native interface rules

All mutable operations are UI-thread-affine. Inputs are borrowed. Successful
interface/RSTRING outputs are retained. Validate out pointers and initialize
them before operations that can fail. Each object uses one controlling IUnknown;
multiple capabilities do not introduce independent reference counts.

| Condition | Result |
| --- | --- |
| Null mandatory pointer | E_POINTER |
| Invalid enum, range, foreign item/column, or conflicting image assignment | E_INVALIDARG |
| Invalid indexed lookup | DISP_E_BADINDEX |
| Wrong property/value type | DISP_E_TYPEMISMATCH |
| Find/GetNextSelected has no match | S_FALSE with null object |
| Nullable getter has no selected/focused item, image source, or Data | S_OK with null object |
| Canceled before-event/unhandled virtual writeback | S_FALSE, no framework commit |
| Operation requires the other OwnerData mode, is reentrant, or targets a frozen snapshot | QuadooForm::EInvalidState |
| Stored item/column or its source has been destroyed | QuadooForm::EDestroyed, subject to the readable identity fields below |
| Wrong UI thread | QuadooForm::EWrongThread |

Unchanged setters produce no logical events; typed mutation methods may return
S_FALSE. Property interfaces keep their existing setter result conventions.

### Items, identity, and virtual generations

- Stored rows have stable GUID/COM identity. ItemIndex is read-only and changes
  with logical order. After detachment, ID and Attached remain readable; other
  operations return EDestroyed. Release row Data during detachment, not only when
  the last external row reference is released.
- Virtual rows are bounded request snapshots: IsVirtual=true, ID=GUID_NULL,
  immutable ItemIndex and data generation. Captured values remain readable after
  the source changes or is destroyed. Attached reports whether the source,
  generation, and index remain current.
- Data fields are writable only during OnData/OnDataUpdate for a virtual item.
  Frozen selection/focus fields are snapshots too. Use ListView.SetItemSelected
  for live per-index selection without fetching row data, and the ListView's
  Selected/Focused operations for object-based requests. Validate generation
  when accepting a virtual item reference for a live operation.
- Stored GetDataGeneration returns zero. Virtual generations start at one.
  Changed Items.Count advances generation, cancels stale edits, and keeps selected
  indexes that remain valid. ResetVirtualData(count) also clears selection/focus.
- Virtual Clear/Insert/Delete/Move/Find/stored Sort are invalid. Clear a virtual
  view using Items.Count=0 or ResetVirtualData(0). No per-row GUID map or permanent
  wrapper cache is created for virtual data.
- OwnerData mode changes require an empty view outside edits, callbacks, and
  update batches. Native HWND recreation is necessary; no silent data conversion.

### Text, payloads, columns, and images

- Caption is logical column zero. SubItems[n] is logical column n+1. Native
  GetSubItems clones IRStringArray; SetSubItems copies it. SetSubItem fills missing
  intervening entries with empty strings. Preserve extra nonvisible strings.
- Data is a retained nullable IUnknown payload, runtime-only. Script values use
  an adapter-private QVARIANT holder. Never store it directly in native lParam,
  truncate it to LONG, or interpret an arbitrary native payload as IQuadooObject.
- DisplayIndex moves a column's visible position without changing data mapping.
  Logical collection insertion/deletion/movement migrates the corresponding cell
  strings. Native logical column zero remains left-aligned. Width uses logical
  96-DPI pixels, with separate autosize methods rather than negative sentinels.
- Custom StateIndex is -1 or 0..14. Native state-image indexes are translated to
  1..15. ImageIndex is -1 or any nonnegative INT; unavailable images render empty
  without rewriting the value.
- CheckBoxes and custom StateImages cannot be assigned together. Checkbox mode
  uses StateIndex 0/1 and Checked as a boolean alias. Checked operations outside
  that mode return EInvalidState, avoiding accidental interpretation of a custom
  state image as a checkbox. Enabling CheckBoxes normalizes -1 to unchecked and
  rejects other incompatible state indexes. Disabling preserves the numeric state.
- LargeImages/SmallImages/StateImages use published ImageList capabilities in
  the same Form/Frame namespace. Borrowed snapshots remain retained until the
  last native use is detached. Use LVS_SHAREIMAGELISTS on every ListView HWND.

## 3. Property descriptor contract

Inherit the existing window-control descriptors, including PopupMenu, without
incompatible overrides. S below means PropertySerializable; H means PropertyHidden.
The following are the added descriptors in their proposed local order:

| Name | QuadooProperty type | Flags | Default / choices |
| --- | --- | --- | --- |
| ViewStyle | Enum | S | Report; Icon=0, SmallIcon=1, List=2, Report=3 |
| ReadOnly | Boolean | S | false |
| FullRowSelect | Boolean | S | true |
| OwnerData | Boolean | S | false |
| CheckBoxes | Boolean | S | false |
| MultiSelect | Boolean | S | false |
| HideSelection | Boolean | S | false |
| GridLines | Boolean | S | false |
| ShowColumnHeaders | Boolean | S | true |
| ColumnClick | Boolean | S | true |
| ColumnReorder | Boolean | S | false |
| LargeImages | ObjectReference | S | null; IQuadooImageList capability |
| SmallImages | ObjectReference | S | null; IQuadooImageList capability |
| StateImages | ObjectReference | S | null; IQuadooImageList capability |
| Columns | Collection | S | [] |
| Items | Collection | S + H | []; must be empty for OwnerData |

ViewStyle values are model values, not numeric Win32 LVS_* style values; translate
them explicitly. FullRowSelect and GridLines retain their values outside Report
mode. ReadOnly gates native caption editing, not programmatic text or checking.

Do not create runtime-only property descriptors named Items or ItemIndex: the
reserved descriptor rules must remain intact. Typed collection/item APIs expose
runtime count/index/data/selection. The hidden serializable Items descriptor
preserves native/C++ stored-row JSON support without offering an Items Editor.

Columns and rows contain nested data, not component definitions. Column GUIDs
are collection identities, never generated class members or component names.
Checked and custom StateIndex representations must not conflict in serialized
rows. Selection, focus, virtual count/generation, and Data are omitted from JSON.

## 4. Event descriptor contract

Argument order below is part of the draft contract. All arguments are read-only
unless followed by '*'. B=ValueBool, I=ValueInt, S=ValueString, O=ValueObject.
Object-valued arguments require the existing IQuadooEventObjectArgs capability;
the argument itself is immutable even when request-item fields can be filled.
Sender remains the existing event sender, not an additional argument.

| Event | Arguments, in descriptor order | Event flags |
| --- | --- | --- |
| OnColumnClick | Column:O, ColumnIndex:I | 0 |
| OnChanging | Cancel:B*, Item:O, ChangeMask:I, OldState:I, NewState:I | EventCancellable |
| OnChange | Item:O, ChangeMask:I | 0 |
| OnSelectItem | Item:O, Selected:B | 0 |
| OnItemActivate | Item:O, ColumnIndex:I | 0 |
| OnEditing | Cancel:B*, Item:O, ColumnIndex:I | EventCancellable |
| OnEdited | Cancel:B*, Item:O, ColumnIndex:I, Text:S* | EventCancellable |
| OnItemChecking | Cancel:B*, Item:O, Checked:B* | EventCancellable |
| OnItemChecked | Item:O | 0 |
| OnInsert | Item:O | 0 |
| OnDeletion | Item:O | 0 |
| OnCompare | Item1:O, Item2:O, ColumnIndex:I, Result:I* | 0 |
| OnContextPopup | Item:O, ColumnIndex:I, X:I, Y:I, Handled:B* | 0 |
| OnData | Item:O, RequestedFields:I, ColumnIndex:I | 0 |
| OnDataUpdate | Cancel:B*, Item:O, ChangeMask:I, ColumnIndex:I, Handled:B* | EventCancellable |
| OnDataHint | FirstIndex:I, LastIndex:I | 0 |
| OnDataFind | Text:S, StartIndex:I, Flags:I, ResultIndex:I* | 0 |
| OnDataStateChange | FirstIndex:I, LastIndex:I, OldState:I, NewState:I | 0 |

OnChanging/OnDataStateChange OldState/NewState use Win32 LVIS_* state bits;
ChangeMask uses the separate public ListItemChange flags. Virtual range changes
describe selection/focus, not application-owned custom state. ResultIndex starts
at -1; OnCompare.Result starts at zero; writable booleans start false. Canceled
operations return S_FALSE without post-events. Inherited event signatures remain
unchanged; no per-column event or event-bearing column component is introduced.

ColumnIndex is logical, not DisplayIndex. It is zero for native label edits,
-1 for a whole-row/collection operation, or -1 for empty-area context menus.
OnData's RequestedFields uses ListDataField; state includes Checked/StateIndex.
OnDataFind flags use ListFindFlags. Normalize native search requests into that
contract rather than exposing pointers or native notification structures.

Programmatic changes and native notifications describe one logical operation,
not two events. Construction/recreation/load are silent. OnInsert/OnDeletion
are non-cancellable; deletion still exposes Data before detachment. OnCompare
is used only for explicit stored Sort; zero results preserve the previous order.
OnEdited and OnItemChecking run before virtual OnDataUpdate; no later cancellable
event may undo a successfully committed application update.

OnDataUpdate requires explicit Handled=true and Cancel=false before the wrapper
accepts a proposal. A handler owns consistency of its backend update. A frozen
virtual item cannot be changed to fake persistence; UpdateVirtualText/Checked
submit new proposals through this event instead. Virtual caption editing also
requires a bound writeback handler and ReadOnly=false.

OnChanging cannot promise native virtual selection cancellation:
[LVN_ITEMCHANGING is not sent for OwnerData](https://learn.microsoft.com/en-us/windows/win32/controls/lvn-itemchanging).
OnDataStateChange can report ranges; do not manufacture an item object/event for
every row in a very large range. Native range-notification behavior still needs
the actual XP interaction checks noted below.

## 5. Script mapping

The following names are the intended production adapter surface. The probe's
Snapshot/Apply names are experimental equivalents of GetSubItems/SetSubItems,
not additional required public methods.

| Object | Properties / methods |
| --- | --- |
| ListView | Existing descriptor properties; Items, Columns, Selected, Focused, SelectedCount, DataGeneration; GetNextSelected, SetItemSelected, BeginLabelEdit, EnsureVisible, BeginUpdate/EndUpdate, Sort, Refresh/RefreshRange, ResetVirtualData, UpdateVirtualText, UpdateVirtualChecked |
| Items | Count (writable only OwnerData); GetAt/GetItem, Add(caption), Insert(index,caption), Find(id), Delete(item or index), Move(item,index), Clear |
| Item | ID, Attached, IsVirtual, DataGeneration, ItemIndex (Index alias), Caption, SubItems, ImageIndex, StateIndex, Checked, Data, Selected, Focused; GetSubItems/SetSubItems, GetSubItem/SetSubItem |
| SubItems facade | Count, indexed access, GetAt, Add, Insert, Replace, Delete, Clear; all writes route through public item operations |
| Columns | Count; GetAt/GetItem, Add(caption), Insert(index,caption), Find(id), Delete(column or index), Move(column,index), Clear |
| Column | ID, Attached, Index, Caption, Width, Alignment, DisplayIndex; AutoSize(Header/Content) |

Add appends through Insert(Count,...); Delete(index) first resolves the native
collection object. Script component references unwrap through existing public
component capabilities. Every collection/object argument retains the normal
interface ownership and live-instance checks; no concrete runtime class cast is
part of the mapping.

Both `Item.SubItems[0]` and `var strings=Item.SubItems; strings[0]` must work:
the former can call the item's named Get/SetIndexedProperty, while the latter
uses the facade's default indexed property. The focused experiment verifies both
routes through the current compiler/VM. Copying a returned IRStringArray snapshot
back remains explicit. CRStringArray::Compare suppresses unchanged commits.

The Data experiment verifies integer, 64-bit integer, string, null, array, and
script-object round trips; object mutation through Data preserves the original
script object. The private holder releases its value on replacement/destruction.
An opaque native-payload facade will need a private unwrapping capability so
assigning that facade to another row retains the original native IUnknown rather
than boxing the facade as an unrelated script payload. Production opaque
native-payload wrapping, actual form-reference cycles, full
item identity caching, and object-valued event dispatch remain stage-three work.

## 6. Native probe findings

The independent QuadooListViewProbe project dynamically loads the selected
common-controls DLL and verifies each ListView class belongs to that module.
Default mode loads the system classic DLL explicitly; --v6 activates embedded
manifest resource 101. Startup resource 1 contains only an explicit x86 application
identity. Merely LoadLibrary("comctl32.dll") was insufficient to
distinguish them on this host because it could reuse the v6 module.

Local DLLs tested: classic 5.82 and v6 6.16. These are modern-host copies, not
the XP DLL builds. The executable prints their complete paths and versions.
GetVersionEx output is compatibility-version information, not proof of an XP run.

Observed behavior in the local runs:

- OwnerData text, subitem, image, and state requests work through notifications.
- Native Space processing alone does not persist checkbox changes to the data
  provider in this probe. An explicit gesture-to-provider update is needed.
- Local v6 supplies state-icon hit testing. Classic Icon lacks the state-icon hit flag
  in the tested cases; its native slot can be hit using measured geometry when
  large-image geometry is present.
- Classic SmallIcon, and Icon without usable normal-image geometry, need a
  reserved/drawn checkbox slot. The prototype demonstrates owned checkbox/icon
  composite image cells, with or without normal images. It keeps native text,
  layout, caption editing, and selection instead of owner-drawing the whole list.
- The gesture fallback updates provider state and invalidates the native row.
  The composite path verifies that subsequent native image requests select the
  checked/unchecked cell. The prototype toggles the focused item; full selected-
  row batching from the broader plan is not implemented or claimed here.
- Stored and virtual caption editing accepts, rejects, or cancels correctly in
  all four views. Removing LVS_EDITLABELS prevents editing in the stored probe.
- Native virtual selection does not send LVN_ITEMCHANGING. The tested programmatic
  selection sequence sent individual change notifications, not range callbacks;
  this is not evidence that user Shift-selection ranges are unsupported.
- The virtual Report count can be set to 100,000,000 and cleared without the
  probe allocating a record/object per row. This is not a memory/performance
  benchmark for all view modes or for a finished wrapper.

The user-run XP classic 5.82 and v6 6.0 logs confirm the same need for explicit
provider writeback. Both XP DLLs lack native state-icon hits in the tested Icon
and SmallIcon cases: Icon with images uses measured slot geometry; Icon without
images and SmallIcon use the owned checkbox/icon composites. List and Report
use their native state slots. In particular, v6 activation alone does not make
the fallback unnecessary on XP. Both complete probe runs pass, including caption
editing, image ownership, virtual counts, and resource cleanup assertions.

### Implementation consequence

Use an explicit checkbox gesture/writeback path in the future wrapper. Do not
rely on LVM_SETITEMSTATE to persist virtual check state. Prefer native state slots
where they work; provide the narrow composite-slot fallback where necessary.
Composites are consumer-owned derived image lists and must never modify/destroy
the assigned managed source snapshots. Build them from DPI-sized source images,
include an image-less checkbox cell, and rebuild atomically on source/DPI/theme
changes. They do not make CheckBoxes and custom StateImages simultaneous features.

The experiment uses fixed 16/32-pixel drawing and stock icons to prove the path;
the production version must use actual image snapshots, logical metrics, theme
fallbacks, and public ImageIndex-to-derived-index mapping. This is compatibility
work within stages two/four, not an additional general image-list infrastructure.

## 7. Validation results and commands

Built with VS2008 Win32 Debug and Release, zero build errors/warnings.

| Focused run | Debug assertions | Release assertions | Result |
| --- | --- | --- | --- |
| Native classic common controls | 601 | 601 | Passed locally |
| Native v6 common controls | 504 | 504 | Passed locally |
| Managed ImageList / ABI draft | 24 | 24 | Passed locally |
| Existing-VM script contract | 14 | 14 | Passed locally |

Native checks include stored/virtual editing, virtual check gestures with and
without images, shared native handles, repeated creation/destruction, and warmed
GDI/USER resource counts. Managed checks exercise 96/144/192 DPI snapshots, source
rebuild notification, retained old handles, source destruction, and factory lifetime.
The Debug script log contains expected EInvalidState/EDestroyed traces from frozen
and detached-object negative tests; its final HRESULT is S_OK.

Run the focused suites from the repository root:

```powershell
.\tests\platform\QuadooForm\test-listview-stage1.ps1 -Build
```

Logs are written to tests/results/ListViewStage1Logs. The runner builds and tests the existing native harness and ApplicationTests with the current product DLLs. The standalone probe and its manifest checks were retired after the XP run below; its original findings still inform the runtime and tests.

### XP startup packaging correction

The initial executable was rejected by the user's XP VM before main, with
"The system cannot execute the specified program." Inspection of the actual PE
resources found that the build's manifest tool had merged the optional controls
manifest into startup resource 1. Its application identity also lacked an explicit
processor architecture. The original startup/resource separation was therefore
not what the source comments described; a static DLL-import check did not catch it.

The probe now embeds Application.manifest directly in resource 1 and keeps
CommonControls.manifest in nonreserved resource 101. Both identities explicitly
specify x86. Linker manifest generation/UAC insertion and automatic .manifest-file
merging are disabled; the files are RC assets excluded from the manifest tool.
The developer test script verifies the emitted resources with the SDK manifest
validator and XML checks. The rebuilt binaries pass both local configurations.
The user's subsequent XP logs confirm successful startup and complete classic
and v6 runs after this packaging correction.

Microsoft documents this XP error under
[side-by-side activation troubleshooting](https://learn.microsoft.com/en-us/cpp/build/troubleshooting-c-cpp-isolated-applications-and-side-by-side-assemblies)
and the required identity structure in
[application manifests](https://learn.microsoft.com/en-us/windows/win32/sbscs/application-manifests).

PE inspection of the Release standalone probe shows x86, subsystem/OS version
5.00, and imports only USER32.dll, GDI32.dll, and KERNEL32.dll. Common-controls
functions are dynamically resolved. The CRT is static; no additional application
DLL or VC runtime deployment is needed for this standalone probe. Import inspection
is supporting evidence, not a substitute for running on XP.

The Published/QuadooForm.h SHA-256 is unchanged:
137ABF9A6CA02A5E6CE93F05C5F4FC7F7E328FAF1E9F9AEC9E56E2F0C22FF451.
All four existing SimbeyCore.dll hashes also match the pre-build baseline.
Focused existing regressions passed in both Win32 configurations: native TreeView
(300 assertions), native ImageList (25), and ApplicationTests --tree-only (9 native
assertions plus the script's own checks). Validation did not include the full
runtime/designer/Studio suite or x64 builds for this test-only stage.

## 8. User-run XP validation

On 2026-10-25, the user reported successful execution on the XP VM and supplied
the following complete outputs. Both identify Windows 5.1 build 2600 and finish
with result 0x00000000; neither contains a failed assertion.

| User-run XP mode | Reported common-controls version | Assertions | Result / evidence |
| --- | --- | --- | --- |
| Classic | 5.82 build 2900, system32 module | 601 | [Passed](../../tests/results/historical/ListViewXP/ListView-classic.txt) |
| v6 | 6.0 build 2900, WinSxS module | 603 | [Passed](../../tests/results/historical/ListViewXP/ListView-v6.txt) |

These are user-run native probe results, not additional Debug/Release harness or
script runs on XP. Assertion totals depend on the common-controls fallback paths;
the XP v6 total differs from the modern-host v6 total because it takes additional
fallback paths. The common-controls DLL versions and paths are recorded in the
original logs, which are preserved unchanged.

The files linked above are the original standalone probe captures. Use the native and script harness command in section 7 for current regression coverage.

The automated XP run covers the native caption/check/image path. Further manual
visual review of composite placement, theming, and physical DPI is still needed
when the actual wrapper is implemented. Actual user Shift-range notifications,
full virtual writeback events, and designer interactions also belong to that
implementation validation.

The stage-one XP compatibility gate is satisfied. Stage two can proceed with the
reviewed contracts and demonstrated native paths; stages two through six remain
unimplemented. Retain the fallback for both classic and v6 XP common controls.
