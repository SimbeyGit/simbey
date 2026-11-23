# QuadooForm ListView implementation plan

Status: stages one through five implemented, 2026-10-26. Stage one includes user-run XP
native probes. Stage two implements the empty native control, columns, and references.
Stage three implements stored items, logical events, JSON and script objects.
See [ListViewStage1.md](ListViewStage1.md) for exact draft interfaces, descriptor
and script contracts, measured fallback requirements, local Debug/Release results,
and the supplied XP logs. See [ListViewStage2.md](ListViewStage2.md) for the additive
public ABI, implementation scope, and validation results. See
[ListViewStage3.md](ListViewStage3.md) for stored-mode delivery and validation.
See [ListViewStage4.md](ListViewStage4.md) for OwnerData delivery, passing native,
Studio and script checks, and the unresolved full-designer Preview resource check.
See [ListViewStage5.md](ListViewStage5.md) for the shared Columns Editor, native
DPI synchronization fix, palette icon and validation results. Stage six remains,
including actual XP/x64 validation and resolution of the full UI-suite blockers.

## 1. Intended result

Add a native Win32 `ListView` to QuadooForm, the shared designer, Quadoo Studio,
and the existing AQVMT script adapter. Its programming model should be familiar
to users of VCL's `TListView`, `TListItems`, `TListItem`, and `TListColumns`.

The control supports the four classic views, ordinary stored items, virtual
items through `OwnerData`, column editing, checkboxes, image lists, native
caption editing, and an assigned popup menu. Columns and items are lightweight
collection objects. They are not QuadooForm components, designer tree nodes,
palette tools, or individually generated script members.

The designer provides a Columns Editor, but **no Items Editor**. Applications
populate ordinary items in code or provide virtual data through events. The
ListView itself has the named script member and all event handlers.

This pass does not include ListView groups, tile view, owner drawing, arbitrary
subitem editors, per-cell embedded controls, ToolBar, or unrelated controls.

## 2. Findings from the current code

Paths below are relative to the repository root.

| Existing area | Inspected implementation | Use in this work |
| --- | --- | --- |
| Extensible component and window architecture | `platform/QuadooForm/Published/QuadooForm.h`; `QuadooForm/Runtime.h`, `Metadata.cpp`, `Validation.cpp`, `Windows.cpp` | Add an independent type specification, factory, descriptors, and concrete window control. Preserve descriptor validation and inherited lifecycle/layout behavior. |
| Lightweight objects and stable identity | `QuadooForm/TreeView.h`, `TreeView.cpp`, `TreeNodes.cpp`; `TreeViewImplementation.md` | Follow the public-capability, reference-counting, factory-lifetime, attachment, and thread-affinity patterns. Do not derive the ListView model from TreeView. |
| Managed native image lists | `QuadooForm/ImageList.cpp`, `NativeImageList.h`, `NativeImageList.cpp`; published ImageList interfaces | Reuse DPI-specific retained snapshots and source observers. No additional native ImageList interface is needed. |
| Popup routing | `QuadooForm/Windows.cpp::CWinControl::Message`; `MenuNative.cpp::ShowControlPopup` | Reuse component-namespace lookup and `IQuadooPopupMenu::Popup`. Improve ListView-specific hit testing and keyboard placement. |
| Transactional collection editor | `Designer/TreeNodesEditor.h`, `TreeNodesEditor.cpp`; document/source transaction infrastructure | Build one shared Columns Editor for Studio and the standalone designer. |
| Inspector and selection abstraction | `Designer/Inspector.cpp`, `Selection.cpp` | Add Columns editing and image-reference roles; preserve common-property editing across multiple selected controls. |
| Component script binding | `ScriptAdapter/RuntimeObjects.h`, `RuntimeObjects.cpp`, `RuntimeEvents.cpp`, `TreeObjects.h`, `TreeObjects.cpp` | Add collection/item/column wrappers and generalize the object-valued event dispatcher. |
| Existing string arrays | `platform/SimbeyCore/Published/SimbeyCore.h`; `CRStringArray` implementation | Reuse `IRStringArray` for native subitem-string snapshots. Do not introduce a second native string-array contract. |

Two details affect the design:

1. `RuntimeEvents.cpp` currently treats a non-null `ValueObject` as an
   `IQuadooTreeNode`. It must dispatch by supported public capability before
   ListItem or ListColumn objects can be passed to scripts. Existing TreeView
   wrapping and identity must continue to work.
2. Metadata reserves `Items` as serializable and already permits its Collection
   form for TreeView. ListView must use that valid descriptor shape, with the
   Hidden flag to suppress editing. Similarly, do not introduce an incompatible
   runtime-only component descriptor named `ItemIndex`. The item object's
   `ItemIndex` and the ListView script convenience property can use typed
   capabilities without changing the reserved descriptor contract.

## 3. User-facing properties

The following are the proposed initial defaults. They favor a useful report
control and immediately available caption editing; they do not promise identical
VCL defaults for every property.

| Property | Default | Contract |
| --- | --- | --- |
| `ViewStyle` | `Report` | `Icon`, `SmallIcon`, `List`, or `Report`; named inspector choices and JSON input. Serialization uses the framework's numeric enum representation. |
| `ReadOnly` | `false` | Prevent user caption editing when true. Does not prohibit programmatic changes, selection, or checkbox changes. |
| `FullRowSelect` | `true` | Native full-row highlighting in Report view. Retained but inactive in other views. |
| `OwnerData` | `false` | Select stored-item or virtual-item operation. |
| `CheckBoxes` | `false` | Two-state row checkboxes, including owner-data operation. |
| `MultiSelect` | `false` | Enable ordinary Win32 multiple-row selection. |
| `HideSelection` | `false` | Keep selection visible when focus moves away. |
| `GridLines` | `false` | Native Report-view grid lines. |
| `ShowColumnHeaders` | `true` | Show the native header in Report view. |
| `ColumnClick` | `true` | Enable clickable native column headers and `OnColumnClick`. |
| `ColumnReorder` | `false` | Allow native header dragging to change display order. |
| `LargeImages` | `null` | ImageList component for Icon view. |
| `SmallImages` | `null` | ImageList component for SmallIcon, List, and Report views. |
| `StateImages` | `null` | Optional custom state ImageList; mutually exclusive with `CheckBoxes`. |
| `Columns` | `[]` | Serializable column collection; opens the Columns Editor. |
| `Items` | `[]` | Serializable stored-item collection, hidden from the property grid. Must be empty with `OwnerData=true`. |
| `PopupMenu` | inherited `null` | Existing component reference and runtime menu implementation. |

Inherited bounds, docking, anchors, margins, visibility, enabled state, font,
Quadoo Colors, tab order, focus/keyboard events, and lifecycle remain intact.
Resolve Quadoo Colors only when supplying real colors to native drawing APIs.

Selection, focus, virtual item count, and application `Data` are runtime state,
not designer properties. The virtual count is set through `Items.Count` or its
native equivalent after event binding has completed.

Use the existing inline inspector dropdowns for enums and booleans. Image-list
and popup references use the existing eligible-component dropdowns.

## 4. Proposed public interfaces and ABI

Append interfaces with fresh UUIDs; preserve all existing UUIDs, vtables, exports,
and export ordinals. Reuse `IQuadooEventObjectArgs`, `IQuadooImageList`, its
snapshot/observer interfaces, and `IQuadooPopupMenu` unchanged.

| New interface | Proposed responsibility |
| --- | --- |
| `IQuadooListView` | Get item/column collections; selected and focused items; selection enumeration; virtual count/refresh/reset; label editing; ensure-visible; image-list roles; update batching. |
| `IQuadooListItems` | Count, indexed access, stored Add/Insert/Delete/Move/Clear, GUID lookup, and owner-data count management. |
| `IQuadooListItem` | Identity/attachment, `IsVirtual`, data-set generation, read-only `ItemIndex`, Caption, subitems, ImageIndex, StateIndex, Checked, Data, and selection/focus access. |
| `IQuadooListColumns` | Count, indexed access, Add/Insert/Delete/Move/Clear, and GUID lookup. |
| `IQuadooListColumn` | Identity/attachment, logical Index, Caption, Width, Alignment, DisplayIndex, and explicit autosizing operations. |

Publish the required view/alignment/change-mask enums with fixed values when the
header is prepared. Final method signatures, ownership annotations, error codes,
and enum values are an implementation checkpoint before changing the header.
This proposal requires additive interfaces, not changes to existing interfaces.

Collection objects implement `IUnknown` capabilities and use `CBaseUnknown` and
`BEGIN_UNK_MAP`. They do not inherit `IQuadooComponent`. Audit every interface
path for one COM identity and correct AddRef/Release. Retained objects/snapshots
keep the relevant factory alive without keeping destroyed windows usable.

External implementations must be usable through these published capabilities.
Framework paths must not unwrap a third-party object into private `CListView`,
`CControl`, `CFactory`, or `Internal()` pointers.

## 5. Stored items, strings, and Data

### Identity and lifetime

Each stored item has a stable GUID and stable COM identity. Its zero-based
`ItemIndex` changes as rows are inserted, moved, deleted, or sorted. Holding an
item object must continue to refer to that item, rather than whichever row later
occupies its old index. Native indexes and HWND values are transient.

An item belongs to one ListView; moving it to another ListView requires copying
its data. Deletion detaches it. A retained detached item exposes ID and Attached;
other operations return `EDestroyed`, following the TreeView pattern. Release its
Data payload at detachment even if a stale item wrapper is retained. Clear and
control destruction detach all stored items.

Native `LVITEM.lParam` is reserved for an internal stable identity/token. It must
never be the user-facing Data value or a truncated script/object pointer.

### Caption and subitems

Logical column 0 uses `Caption`. `SubItems[0]` is logical column 1,
`SubItems[1]` is logical column 2, and so forth. Extra strings are retained even
when there are fewer visible columns. Missing strings display as empty strings.

Native GetSubItems returns an independent `IRStringArray` snapshot; SetSubItems
copies its input. Add GetSubItem/SetSubItem helpers for efficient cell access.
Use `CRStringArray::Compare()` where applicable to suppress unchanged updates.
Do not implement a mutable native string proxy over `IRStringArray::GetArray()`;
raw array access would bypass validation and change notification.

Scripts receive a small live `SubItems` facade backed by these public item
operations. It supports Count, indexed Get/Set, Add, Insert, Replace, Delete,
and Clear using existing VM indexed-property conventions. Thus familiar code
such as `item.SubItems.Add("Ready")` updates the row immediately. This facade is
an adapter object, not a redundant published string-collection interface.

### Data

Recommended contract: Data accepts arbitrary script values, including objects,
and is runtime-only. Native access uses an opaque nullable `IUnknown` payload
with normal retained-reference semantics. A C++ consumer can attach its own COM
object. The script adapter owns a private value holder for scalars, strings,
arrays, and objects using existing QVARIANT ownership helpers. No QuadooScript
types or dependencies are added to QuadooForm.dll or its published header.

Only adapter-created value holders are unboxed as script values. An arbitrary
native payload must not be cast to `IQuadooObject`; expose it through an opaque
adapter object if script access is required. Data is omitted from JSON and
clipboard definitions. Row deletion, collection replacement, failed staging,
form detachment, and control destruction must release framework-held payloads
and wrapper caches. Application-created reference cycles follow existing VM
ownership rules; the ListView must not add unnecessary permanent VM roots.

### Operations

Expose Items.Add(caption), Insert(index,caption), GetAt(index), Find(id),
Delete(item or index), Move(item,index), Clear(), and Count. Selection/focus are
separate: Selected denotes the focused selected item, or the first selected item
if the focused row is not selected; it is null with no selection. Support
SelectedCount and NextSelected without building an unbounded script array.

BeginUpdate/EndUpdate batch redraw and native synchronization; they are not a
promise to roll back unrelated application code. Each individual mutation still
validates before becoming observable. Do not recreate the entire HWND for every
Add or cell assignment. Incremental updates need staged allocations and explicit
rollback; whole-window staging is reserved for style/model replacements that
actually require it.

Provide stored-item Sort(column,ascending) and optional `OnCompare` support.
Header clicks do not sort automatically: scripts choose a sort policy. Sorting
must be stable, preserve item references and selection, and stage its permutation
before commit. OwnerData sorting belongs to the application's data source.

## 6. Columns and the Columns Editor

Columns have stable GUID/COM identity within their collection. Use Caption,
logical Width at 96 DPI, Alignment (`Left`, `Center`, `Right`), and DisplayIndex.
Keep numeric Width separate from explicit AutoSize-to-header/content methods;
do not serialize native negative autosize sentinels as dimensions.

Logical Index identifies the data column. DisplayIndex identifies its visible
position. Header dragging and the editor's Up/Down buttons change display order
without reassigning captions/subitem strings. `OnColumnClick.ColumnIndex` is
always logical, even after reordering. The editor shows the logical data index
so this distinction is visible.

For public Insert/Delete/Move operations that change logical indexes, move or
insert/delete the associated stored cell values atomically. Moving logical
column 0 promotes/demotes Caption and subitem strings accordingly. Virtual
column-schema changes invalidate data requests; they never rewrite an external
application model. Native logical column 0 must remain left-aligned, so reject
an incompatible Alignment rather than silently displaying something different.

The shared Win32 Columns Editor contains:

- A list of columns in display order, with Caption, data index, Width, and
  Alignment fields.
- Add and Delete; Move Up and Move Down; per-column editing; autosize actions.
- A native Report-view preview of header captions, order, alignment, and widths.
- Standard OK, Cancel, and Apply, using the same candidate-document/source
  transaction pattern as the Tree Nodes Editor.

Add opens a column with a generated collection GUID and a sensible default width.
There is no editable component Name and no generated script declaration for it.
The dialog may use transient sample cells to preview widths, but these never
enter the actual form, its Items, or saved source.

Expose the editor from Columns in the inspector, the Form menu, and the control
context menu. Enable it only for one selected object exposing `IQuadooListView`.
The shared inspector continues to edit compatible common scalar properties for
multi-selections. It must not launch a single collection editor for a mixed group.

Column resize uses logical widths without cumulative DPI-rounding drift.
Persist design-time changes only through document transactions, not from native
notifications generated while painting/rebuilding the designer's hidden model.
For OwnerData, content autosize must not scan an entire virtual data set: use an
explicit visible-range measurement; header autosize remains independent of rows.

## 7. ImageList integration and DPI

All three image properties resolve through the existing Form/Frame component
namespace and require `IQuadooImageList`, including implementations from external
factories. Store canonical component GUID references, not HWNDs, native handles,
names, or concrete-class pointers. Extend the inspector's existing image-list
eligibility filter to LargeImages, SmallImages, and StateImages.

Acquire each source at the ListView's current DPI using the existing API. Keep
the returned snapshot alive for as long as its borrowed HIMAGELIST is attached.
Always use `LVS_SHAREIMAGELISTS`: otherwise destroying the HWND can destroy a
shared image list. This is required by the native
[list-view window-style contract](https://learn.microsoft.com/en-us/windows/win32/controls/list-view-window-styles).

Use one source subscription per distinct ImageList even if it serves several
roles. Stage all replacements needed for a notification or DPI change, attach
them, detach old handles, and then release old snapshots. On a failed rebuild,
retain the previous coherent assignment. Source destruction must detach handles
and clear corresponding live component references without allocating.

The same ImageList may be used simultaneously by TreeView and multiple
ListViews. LargeImages and SmallImages may reference different logical cell
sizes; never mutate a shared ImageList's Width/Height to suit a consumer.
`ImageIndex=-1` means no image. An index outside the current source count renders
no image while preserving its logical value, so later image edits can restore it.

Scale columns, hit rectangles, font/layout, and image snapshots through the
existing DPI infrastructure. Use theme services only where available, with an
XP-compatible fallback. Guard optional v6 double-buffer styles; the baseline
control cannot require newer common-control features.

## 8. StateIndex and checkboxes

Proposed public convention: `StateIndex=-1` means no custom state image;
`0` means the first state image. Convert zero-based public indexes to native
one-based state masks internally. The native state-image field has four bits,
so custom indexes are limited to 0 through 14. Do not confuse these with the
unrestricted normal ImageIndex property.

CheckBoxes and StateImages compete for the same native state-image slot.
Reject their simultaneous assignment atomically. In checkbox mode, StateIndex
0 means unchecked and 1 means checked; Checked is the convenient boolean alias.
Both ways of filling an OnData item are supported. Enabling checkboxes normalizes
an absent state image to unchecked and rejects incompatible custom indexes until
the caller clears them. Checked has checkbox meaning only in that mode.

Serialize Checked for checkbox rows and StateIndex for custom-state rows; do not
save conflicting representations. A checkbox gesture follows checking/cancellation
and post-change events. Unchanged assignments produce no duplicate events.
Space honors the existing OnKeyDown.Handled behavior and has a defined selection
policy: toggle each selected row to the focused row's proposed new value, or only
the focused row when there is no multiple selection.

Checkbox-generated native image lists have separate ownership from borrowed
ImageList snapshots. Detach and destroy owned handles exactly once on recreation,
style change, failed staging, and destruction. Borrowed handles are never destroyed
by this control. Use the
[native checkbox/style semantics](https://learn.microsoft.com/en-us/windows/win32/controls/extended-list-view-styles)
and request virtual state images through the
[state callback mask](https://learn.microsoft.com/en-us/windows/win32/controls/lvm-setcallbackmask).

The stage-one XP probe establishes OwnerData checkbox behavior in all four views
under classic and v6 activation. Implement the demonstrated narrow state-image
hit-test and draw fallback wherever native state callbacks do not provide correct
checkbox interaction. Do not silently omit virtual checkboxes or turn this into
a general owner-drawn ListView project.

Stage-one local testing of classic 5.82 and v6 found that explicit provider
writeback is required, and some classic icon modes also need a reserved checkbox
slot. The probe demonstrates a narrow owned checkbox/icon-composite image-list
fallback without owner-drawing the whole control; see ListViewStage1.md. The
production version must derive these cells from managed DPI-sized snapshots and
retain their source ownership correctly. The user-supplied XP 5.1 build 2600 logs
pass with classic 5.82 (601 assertions) and v6 6.0 (603 assertions). Both XP DLLs
need explicit geometry for Icon with images and composite slots for SmallIcon
and image-less Icon; v6 activation must not disable this fallback.

## 9. OwnerData model

### Count and data requests

OwnerData uses the same `IQuadooListItem` abstraction but not a persistent object
per virtual row. Items.Count sets the native virtual count; Add/Insert/Delete/Move
and stored sorting return `EInvalidState` in this mode. The application changes
its backing data and then updates Count or calls Refresh/ResetVirtualData.
Count begins at zero and is not serialized.

Changing OwnerData requires native window recreation. Permit it only while the
collection/count is empty, with no active label edit, update batch, or callback;
otherwise return `EInvalidState`. Do not silently discard stored rows or Data.
Microsoft documents both the creation-style constraint and the different data
ownership in the
[list-view overview](https://learn.microsoft.com/en-us/windows/win32/controls/list-view-controls-overview).

For each request, create a lightweight request item with ItemIndex set before
OnData. Its Caption, SubItems, ImageIndex, StateIndex/Checked, and Data start from
safe defaults. Pass RequestedFields and the requested logical ColumnIndex so an
application can fill only what is needed, while allowing it to fill a full row.
The application determines its data source using `oArgs.Item.ItemIndex`.
This follows the VCL
[OwnerData event model](https://docwiki.embarcadero.com/Libraries/Sydney/en/API%3AVcl.ComCtrls.TListView.OnData).

An illustrative handler body is:

```text
var item = oArgs.Item;
var row = Rows[item.ItemIndex];
item.Caption = row.Title;
item.SubItems.Add(row.Description);
item.ImageIndex = row.ImageIndex;
item.Checked = row.Checked;
item.Data = row;
return true;
```

Callbacks may be repeated, partial, and out of order. They are not notifications
to append to persistent Items. Data assigned here belongs to the request snapshot,
not to an invisible stored-row cache. After return, a retained snapshot remains
readable and frozen; it never changes to represent a later request. Writes after
the permitted callback scope return `EInvalidState`.

Virtual Items.GetAt(index) requests data and returns such a snapshot. There is
no GUID lookup or permanent COM-identity guarantee for a virtual index. Expose
the current data-set generation; ResetVirtualData increments it and invalidates
stale edit/update contexts. Retained snapshots describe their original generation.
Applications needing stable business identity use Data or their own key.

Make this distinction inspectable: virtual items report IsVirtual=true and
ID=GUID_NULL, with fixed ItemIndex and generation. Attached indicates whether the
source control/generation remains current. Unlike detached stored items, their
captured value fields remain readable independently of the source's lifetime;
they never provide live selection/focus setters after the request. These rules
must be stated on the public item interface, not hidden in the script wrapper.

Do not accumulate virtual wrappers in the form's permanent object cache. Native
wrapper/model allocations must depend on columns, active requests, and selected
ranges, not the total virtual count. Measure common-control allocations separately.
Use native range notifications and bounded work for large
selections. Validate Count against native limits and overflow; Microsoft's
[LVM_SETITEMCOUNT contract](https://learn.microsoft.com/en-us/windows/win32/controls/lvm-setitemcount)
specifies a maximum of 100,000,000 virtual items.

Fill the supplied Win32 text buffers with length checks. Never retain pointers
to temporary RSTRING data in a native notification structure. Honor the requested
mask and subitem index in
[LVN_GETDISPINFO](https://learn.microsoft.com/en-us/windows/win32/controls/lvn-getdispinfo).
Unbound OnData returns blank/default fields safely. Callback failure supplies
safe fields and reports through existing error handling without throwing through
Win32 or unexpectedly closing a modal dialog.

### Editing and checkbox writeback

OnData supplies display data; mutating its request item does not persist a change
to the application's data source. Add `OnDataUpdate` for proposed user edits:
Item, ChangeMask, ColumnIndex, Cancel, and Handled. Its Item contains the proposed
caption/check state and the same ItemIndex/generation as the original request.

For virtual label editing: OnEditing permits the edit; OnEdited validates or
replaces text; OnDataUpdate commits it to the application. The handler sets
Handled only when it has accepted and stored the change. For checkboxes:
OnItemChecking precedes OnDataUpdate, followed by OnItemChecked after acceptance.
The wrapper does not commit a proposed value after an absent, canceled, failed,
or unhandled writeback. OnData remains authoritative for the next display request.
The application handler must perform its own update consistently before setting
Handled; the control cannot roll back arbitrary application work. A successfully
handled writeback is followed by invalidation and a fresh OnData request. No
later cancellable event may veto an already committed application update.

Allow virtual caption editing only when ReadOnly is false and a writeback
handler is bound. Stored items need no OnDataUpdate handler. Expose an explicit
Update operation for applications that intentionally submit a virtual proposal;
ordinary setters on frozen virtual snapshots do not pretend to update a backend.

### Reentrancy

During OnData/OnDataUpdate, allow request-item population and reads of stable
control properties. Reject structural item/column edits, count/mode changes,
sorting, and recursive fetching of the same in-flight item. Independent nested
requests use independent buffers and request objects. Guard generation and native
window lifetime after every callback. Destruction from a handler follows the
existing retained-dispatch pattern and must not access freed native storage.

Design mode never invokes application providers. Callback-free standalone Preview
therefore shows zero virtual rows; runtime/AQVMT instances may populate after
event binding. No fake preview data is serialized.

## 10. Logical event contract

Use immutable per-type event descriptors, existing delegate binding, and
`ValueObject` arguments for items/columns. Arguments that denote an object are
read-only references; the allowed item fields can be populated in request scopes.
ColumnIndex is logical; use -1 where a cell/column does not apply.

| Event | Proposed arguments and behavior |
| --- | --- |
| `OnColumnClick` | Column object, ColumnIndex; no automatic sorting. |
| `OnChanging` | Item, ChangeMask, old/new state, writable Cancel. Cancellable stored-item state changes and explicit wrapper requests. |
| `OnChange` | Item, ChangeMask; committed text/image/state changes. |
| `OnSelectItem` | Item, Selected; committed individual selection changes. |
| `OnItemActivate` | Item, ColumnIndex; native activation, distinct from inherited mouse clicks. |
| `OnEditing` | Item, ColumnIndex=0, writable Cancel. |
| `OnEdited` | Item, ColumnIndex=0, writable Text and Cancel; validation before persistence. |
| `OnItemChecking` | Item, writable Checked and Cancel; before a checkbox proposal. |
| `OnItemChecked` | Item; after a committed checkbox change. |
| `OnInsert` / `OnDeletion` | Stored item; Data remains readable during deletion notification, before detachment. Not cancellable. |
| `OnCompare` | Stored Item1, Item2, ColumnIndex, writable integer Result; used only by explicit Sort. |
| `OnContextPopup` | Item or null, ColumnIndex, screen X/Y, writable Handled; suppress automatic PopupMenu when handled. |
| `OnData` | Request Item, RequestedFields, ColumnIndex; fill virtual data. |
| `OnDataUpdate` | Proposed Item, ChangeMask, ColumnIndex, writable Cancel and Handled; persist virtual edits. |
| `OnDataHint` | FirstIndex, LastIndex; application cache hint, no forced per-row materialization. |
| `OnDataFind` | Search text, StartIndex, search flags, writable ResultIndex; virtual incremental search. |
| `OnDataStateChange` | FirstIndex, LastIndex, old/new state; virtual selection ranges. |

Inherited keyboard/focus/OnEnter/OnLeave/click event signatures remain unchanged.
For very large virtual range changes, use OnDataStateChange without expanding the
range into millions of OnSelectItem callbacks or item objects.

Do not promise cancellable native virtual selection through OnChanging:
[LVN_ITEMCHANGING is not sent for LVS_OWNERDATA](https://learn.microsoft.com/en-us/windows/win32/controls/lvn-itemchanging).
Use post-change virtual range notifications; explicit wrapper selection methods
can validate their own requested changes. Checkbox and edit cancellation remain
supported through the control's explicit gesture/writeback paths.

Construction, JSON load, HWND recreation, and internal native synchronization are
silent. Logical programmatic changes dispatch the documented events once;
unchanged assignments dispatch none. Before-events run before persistence, and
post-events run only after the native/logical state is coherent. Structural
reentry is rejected consistently rather than partially applying an operation.
Define combined change masks and ordering in the published interface comments.

## 11. Inline editing and popup menus

Use native label editing for Caption only. Report-view subitem strings are
script-editable; arbitrary subitem inline editors are outside this pass. Provide
F2, native mouse initiation, and BeginLabelEdit(item). Enter accepts and Escape
cancels. A canceled edit or unchanged text produces no duplicate change event.
Honor OnEdited text replacement and native
[end-label-edit return semantics](https://learn.microsoft.com/en-us/windows/win32/controls/lvn-endlabeledit).

Integrate the temporary native edit HWND with existing form keyboard routing:
typing, Delete, Ctrl+arrow keys, Enter, and Escape must reach the editor instead
of deleting/nudging designer objects or activating unrelated form buttons.
ReadOnly applies to user text editing; Enabled and checking events govern other
interaction. Design mode does not launch the runtime label editor or handlers.

Automatic PopupMenu behavior is:

1. Right-click any cell, including a subitem outside the label, and hit-test the
   row and logical column. Select/focus an unselected row before menu callbacks.
   If it is already selected, preserve the existing multiple selection. If a
   selection handler vetoes changing to the clicked row, suppress the automatic
   row popup rather than running actions against an unexpected selection.
2. Raise OnContextPopup and, unless handled, invoke the assigned popup once at
   the mouse's screen coordinates. Respect negative monitor coordinates.
3. For Shift+F10 or the keyboard Menu key, use the focused row's visible rectangle
   as the anchor, clamped to the control's visible client area. Use the client
   origin only when there is no usable row.
4. On empty space, preserve current selection and allow the assigned popup with
   Item=null and ColumnIndex=-1. Header clicks are not row/cell clicks and do not
   implicitly use the row popup.

Use WM_CONTEXTMENU as the single popup invocation path, coordinating any native
NM_RCLICK notification so it cannot open the menu twice. Reuse shared reference
lookup/menu invocation; add the placement/context hook needed by ListView rather
than building a second popup-menu implementation. This follows the native
[WM_CONTEXTMENU coordinate contract](https://learn.microsoft.com/en-us/windows/win32/menurc/wm-contextmenu).
End the OnContextPopup notification guard before entering menu tracking. A
PopupMenu item's OnClick must be able to modify/delete ListView rows normally.
Retain the participating objects and recheck control lifetime after callbacks.
In the visual designer, right-click selects the ListView component and opens the
existing designer context menu, never an application menu.

## 12. JSON and source synchronization

Keep the existing form format. The following is a component excerpt; the two
image/menu GUIDs refer to components in the same form/frame namespace:

```json
{
	"type": "ListView",
	"name": "Results",
	"id": "{B5000000-0000-0000-0000-000000000001}",
	"properties": {
		"Left": 12,
		"Top": 12,
		"Width": 480,
		"Height": 280,
		"ViewStyle": "Report",
		"ReadOnly": false,
		"FullRowSelect": true,
		"OwnerData": true,
		"CheckBoxes": true,
		"SmallImages": "{B5000000-0000-0000-0000-000000000002}",
		"PopupMenu": "{B5000000-0000-0000-0000-000000000003}",
		"Columns": [
			{
				"id": "{B5100000-0000-0000-0000-000000000001}",
				"Caption": "Name",
				"Width": 180,
				"Alignment": "Left",
				"DisplayIndex": 0
			},
			{
				"id": "{B5100000-0000-0000-0000-000000000002}",
				"Caption": "Description",
				"Width": 260,
				"Alignment": "Left",
				"DisplayIndex": 1
			}
		],
		"Items": []
	},
	"events": {
		"OnColumnClick": "Results_ColumnClick",
		"OnData": "Results_Data",
		"OnDataUpdate": "Results_DataUpdate"
	}
}
```

For stored mode, each Items element has an id, Caption, SubItems string array,
ImageIndex, and Checked or StateIndex according to the mode. Preserve stored
item/column IDs across round trips. Assign IDs when absent, reject duplicates
within the collection, and remap them when cloning a whole ListView component.
No Data, virtual count, temporary native state, selection, or focus is persisted.

Validate the entire candidate before replacing Columns/Items: object shapes,
strings without embedded NULs, GUIDs, enums, nonnegative widths, display-order
permutation, numeric bounds, item/image/state indexes, and OwnerData/Items or
CheckBoxes/StateImages conflicts. Establish documented allocation/collection
limits in the implementation; do not accidentally inherit TreeView's 4,096-node
limit for a flat list. Allocation or native failure must preserve the previous
observable definition, bindings, selection, and handles.

Rows use native string arrays internally; JSON remains an ordinary array of
strings. Descriptor-driven component reference serialization, copy/paste remap,
and reference clearing on source deletion apply to all image roles and PopupMenu.
Named reference input may use the existing normalization path; canonical output
stores GUIDs.

Studio source synchronization generates one ListView member and its handlers.
It must never traverse Columns/Items as components or generate column/item
members. The Columns Editor commits through source preparation before changing
the live definition, preserving unsaved buffers and rollback behavior.

## 13. Internal organization and script adapter

Proposed implementation files follow existing project organization:

| Area | Planned additions/changes |
| --- | --- |
| Native runtime | `QuadooForm/ListView.h`, `ListView.cpp`, `ListItems.cpp`, `ListColumns.cpp`; optional focused owner-data support file if size warrants it. |
| Public API and metadata | Append capabilities/enums to `Published/QuadooForm.h`; register the type/factory and per-type property/event metadata. Preserve mandatory base descriptors. |
| Shared image/menu infrastructure | Reuse native ImageList snapshots; extend popup placement/context hook narrowly; maintain independent factory support. |
| Shared designer | `Designer/ListColumnsEditor.h`, `ListColumnsEditor.cpp`, shared dialog resources; inspector, commands, context menus, and capability-based enablement. |
| Palette assets | A 20x20 `Icons/ListView.png` and ICO resource in QuadooForm.dll through the existing palette-icon mechanism. |
| Script adapter | `ScriptAdapter/ListViewObjects.h`, `ListViewObjects.cpp`; component property/method dispatch; a small capability-based object wrapper dispatcher shared by event arguments. |
| Tests/examples | Native ListView tests, private failure tests, shared editor tests, Studio integration tests, and an ApplicationTests script example. |

The native CListView derives from the existing window-control adapter and exposes
only its actual capabilities. Item/column classes share a lifetime/thread link
but no private implementation inheritance is required of external consumers.
Use existing repository arrays, maps, strings, sorting, formatting, streams,
HRESULT handling, and reference-counting patterns; no STL or exceptions.

Cache stored item/column wrappers by canonical COM identity, following TreeView.
Prune detached wrappers and clear form caches on detachment. Do not put virtual
request wrappers in a permanent identity cache. Generalize ValueObject wrapping
to query TreeNode, ListItem, and ListColumn capabilities, preserving null objects
and failure behavior for unsupported objects. Native callers use the same event
objects without needing QuadooScript.

No compiler/parser/VM language changes or SimbeyCore rebuild should be needed.
No additional DLL is required for the editor or resources. Add files/resources
to the existing VS2008 projects and matching supported configurations.

## 14. Implementation sequence

### Stage 1: contracts and compatibility probe

Completed; see [ListViewStage1.md](ListViewStage1.md) for contracts and evidence.

- Prepare exact additive interfaces/enums, item-state conventions, event argument
  descriptors, and script mappings for review before editing the public header.
- Probe XP common controls for all four OwnerData views, checkbox/state callbacks,
  native caption editing, selection ranges, and managed image-list ownership.
- Confirm the native checkbox fallback scope, Data ownership, and no-VM-change
  subitem facade with focused native/script experiments.

Exit: contracts are explicit and the XP virtual-checkbox path is demonstrated;
no requirement is quietly deferred because of native behavior.

### Stage 2: native control, columns, and references

Completed; see [ListViewStage2.md](ListViewStage2.md) for implementation and tests.

- Register ListView with inherited window behavior and per-type metadata.
- Implement view/style properties, columns, display order, logical DPI widths,
  image roles, and popup context/placement.
- Establish recreation/rollback and native resource ownership before rows.

Exit: an empty ListView renders in each view, headers behave correctly, references
can be assigned/removed, and destruction/recreation does not leak shared resources.

### Stage 3: stored items and script objects

Completed; see [ListViewStage3.md](ListViewStage3.md) for implementation and tests.

- Implement stable items, Caption/SubItems/Data/image/state fields, batching,
  sorting/reordering, selection, native label edits, and logical events.
- Add collection/item/column and SubItems script wrappers; generalize event-object
  dispatch while retaining TreeView behavior.
- Implement stored JSON round trips and failure rollback.

Exit: C++ and AQVMT scripts can populate, inspect, edit, select, sort, and delete
rows without losing object identity or leaking payloads.

### Stage 4: OwnerData

Implemented; see [ListViewStage4.md](ListViewStage4.md) for behavior and validation.
The runtime, private, Studio, script and focused designer checks pass in Win32
Debug/Release. The report records an unresolved full-designer Preview resource
check failure and the actual-XP/x64 checks still reserved for final validation.

- Implement count/generation, request snapshots, native buffer filling,
  hints/search/state ranges, and bounded wrapper lifetime.
- Add edit/checkbox writeback and the XP fallback demonstrated by the probe.
- Exercise reentrancy, destruction during callbacks, missing/erroring providers,
  and very large virtual counts.

Exit: virtual rows use ItemIndex correctly, accepted edits persist through the
application provider, and the framework allocates no object/storage per virtual
row. Record native common-control memory behavior as part of the scale tests.

### Stage 5: designer and Studio

Implemented; see [ListViewStage5.md](ListViewStage5.md). The shared Columns Editor,
menu/inspector integration, 96-DPI persistence tests, dedicated resource icon and
example definition are delivered. Focused feature checks pass in Win32
Debug/Release; the report separates the legacy clipboard fixture failures in
complete UI-suite runs.

- Add the shared Columns Editor and menu/context/inspector integration.
- Add the palette icon and example definitions.
- Verify source transactions, multi-selection inspector behavior, component
  reference dropdowns, and the absence of generated column/item members.

Exit: both designer hosts offer the same column workflow and no actual-item UI.

### Stage 6: validation and delivery report

- Build and run the focused tests as each stage lands, then full affected Win32
  Debug and Release suites for runtime, designer, Studio, and ApplicationTests.
- Build/test supported x64 configurations as regression coverage where present.
- Validate actual Windows XP operation separately from a modern-Windows build.
- Report additive ABI details, delivered behavior, measured resource/count tests,
  any VM-only/manual checks, and remaining issues.

## 15. Validation matrix

| Area | Required coverage |
| --- | --- |
| Construction/capabilities | Public factory creation, lifecycle, inherited focus/keyboard/layout, correct QueryInterface identity/refcounts, thread affinity, independent ImageList/PopupMenu capability implementations. |
| Views/DPI | All four modes at 96/144/192 DPI; font and logical width changes; Report-only properties retained across modes; XP themes enabled and disabled. |
| Columns | Add/delete/logical move/display reorder; caption/alignment/width; first logical column restrictions; cell-data association; zero columns; round-trip order; autosize bounded in OwnerData. |
| Stored rows | Add/insert/move/stable sort/delete/clear; stable references and changing ItemIndex; Unicode/empty/long text; extra/missing subitems; detached errors; efficient batching. |
| Data | Scalar/object/string/array script payloads; native opaque objects; replacement and deletion releases; Data visible during deletion event; no serialization; retained wrappers/form destruction; pointer-width correctness. |
| Images/state | Large/Small/State roles; same source in several roles and TreeView/ListViews; image edits/reorder/destruction; failed Acquire; DPI rebuild; out-of-range images; StateIndex mapping/limits; checkbox conflict validation. |
| Editing/checking | F2/mouse/API; ReadOnly; accept/cancel/replacement; Enter/Escape/Delete routing; unchanged assignments; mouse/Space and multi-selection policy; no duplicate notifications. |
| OwnerData | Correct ItemIndex and requested fields/subitem; repeated/partial/out-of-order requests; immutable retained snapshots/generation; no stale Data; hints/find/ranges; zero and very large counts; no O(count) wrapper allocations. |
| Virtual writeback | Accepted/rejected/unhandled/failed label and checkbox updates; model refresh; callback-modified proposals; count/reset during active edit; no loss of accepted provider changes. |
| Events/reentrancy | Stable Item/Column object arguments; ordering and cancellation; native virtual selection limitation; nested data requests; forbidden structural reentry; destruction from events; TreeView object-event regression. |
| Popup | Mouse row/subcell and blank area; existing multi-selection; keyboard anchor; offscreen row/negative monitor coordinates; one invocation; correct selection before menu callback; designer suppression. |
| Serialization/clipboard | Stored and virtual definitions; columns and references; malformed/duplicate IDs; state/mode conflicts; omitted runtime state; clone ID/reference remapping; removed component references. |
| Designer/source | Columns Editor OK/Cancel/Apply; allocation and source-preparation failure rollback; both hosts; no Items editing entry; only one generated ListView member; common scalar bulk edits. |
| Resources/failure injection | Per-allocation/native-operation failures; unchanged observable state; failed HWND recreation; observer subscription rollback; images/payloads/factories released exactly once; repeated open/edit/destroy GDI/USER counts. |

Keep deterministic native/automated assertions separate from manual visual checks.
Actual XP verification must include loading the produced binaries, editing a row,
checkbox OwnerData interaction, popup positioning, and image rendering. Audit
imports and optional feature guards with the XP SDK/toolset; modern documentation's
current supported-OS table alone is not proof of an API's introduction date.
Do not report an XP VM run unless one was actually performed.

## 16. Recommended defaults and remaining validation

No further detail is needed to prepare the implementation around this plan. The
following choices are proposed defaults, rather than assumptions about exact
historic VCL binary behavior:

1. Data is an arbitrary runtime script value/retained native COM payload, not a
   32-bit integer or serialized pointer.
2. Public custom StateIndex is zero-based with -1 for none; checkbox mode aliases
   0/1. Custom StateImages and CheckBoxes cannot occupy the same slot together.
3. Native inline editing covers Caption only. Virtual edits/checks use explicit
   OnDataUpdate writeback so display callbacks cannot silently lose user edits.
4. Report view, FullRowSelect=true, and ReadOnly=false are the initial defaults.
5. Empty-area right-clicks use the assigned PopupMenu without clearing selection;
   header right-clicks are separate from the row-popup contract.

The implementation report should identify any change to these contracts before
expanding scope or altering an existing public ABI.
