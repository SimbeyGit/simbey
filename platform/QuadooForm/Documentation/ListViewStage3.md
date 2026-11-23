# ListView stage three: stored items and script objects

Completed 2026-10-25. Win32 Debug and Release validation passed.

## Delivered behavior

The native ListView now implements stored rows through the existing published
IQuadooListView, IQuadooListItems and IQuadooListItem contracts. This stage adds
no interfaces, UUIDs, vtable entries, exports or enum values. The published
header only gains event-contract comments.

- Insert, delete, move, clear, GUID lookup and stable merge sorting.
- Stable row GUIDs and COM identity; ItemIndex follows the row's current position.
- Caption, ImageIndex, StateIndex, Checked, Selected, Focused and opaque Data.
- Independent IRStringArray snapshots and validated copy-in for SubItems.
  Compare() suppresses unchanged assignments; missing cells read as empty strings.
- Batched redraw and property notifications with incremental row insertion and
  cell updates. Adding or editing individual rows does not replace the HWND.
- Selection enumeration without constructing a script array.
- Native caption editing through F2, the mouse or BeginLabelEdit(), including
  cancellation, handler-supplied replacement text, Enter and Escape.
- Automatic row/cell popup placement, selection before the callback, cancellation
  of a proposed selection, keyboard placement and one popup invocation.
- Stored Items JSON, stable IDs, retained extra subitems and omitted runtime
  Data/selection/focus. Cloning a ListView remaps its column and row GUIDs.

Logical column insertion/deletion/movement migrates the corresponding row cells.
Moving column zero promotes/demotes Caption. DisplayIndex changes only presentation.
Whole Columns replacement retains the logical data positions of existing rows.
Autosizing to content measures stored cell text and accounts for first-column
image/state slots.

Normal image indexes outside the current image-list bounds remain logical values
and render without an image. The managed source snapshots and native ownership
rules from stage two are preserved across row population, style changes and DPI.

## Events and transactions

Implemented stored-mode events are OnChanging, OnChange, OnSelectItem,
OnItemActivate, OnEditing, OnEdited, OnItemChecking, OnItemChecked, OnInsert,
OnDeletion and OnCompare, in addition to the stage-two column/context events.

Before-events run before persistence. Checkbox proposals allow replacement as
well as cancellation. State flags in OnChanging preserve unaffected exposed
LVIS_SELECTED, LVIS_FOCUSED and LVIS_STATEIMAGEMASK bits consistently for native
and wrapper operations. Checking emits OnItemChecking, OnChanging, OnChange and
OnItemChecked in that order; selection emits OnChanging, OnChange and OnSelectItem.
Unchanged/canceled assignments and construction/recreation are silent.

Space honors OnKeyDown.Handled and sets selected rows to the focused row's proposed
new checked value, or changes only the focused row when it is not selected.
The batch preflights checking/cancellation before committing any row.

Insert/delete/clear/state/sort operations stage allocations and preconditions.
Native failures roll back model membership, indexes, selection and definitions.
Column/definition/style/DPI replacements continue to use staged HWNDs. Failed
column remapping restores the original cells along with the original window.

OnDeletion runs after removal from the live collection, while the item remains
readable and its Data is available. The item then detaches and releases Data.
Retained detached items expose only ID and Attached. Clear and destruction release
payloads even when a script or native consumer retains the old item.

Mutation during a notification is rejected with EInvalidState. Native providers
cannot destroy the ListView during an internal commit; normal event handlers may
destroy it and callers recheck its lifetime. Post-commit callback failures report
an error without undoing an already committed operation. Property, collection
and selection changes are rejected while the native label editor is active;
destroying the control remains supported.

The native edit HWND receives its keyboard messages before form shortcuts,
KeyPreview, default/cancel buttons or dialog-key handling can consume them.
Design mode suppresses runtime events and prevents native label/check editing.

## Script adapter

ListViewObjects.cpp adds capability-based wrappers for:

| Object | Script surface |
| --- | --- |
| ListView | Items, Columns, Selected, Focused, ItemIndex, SelectedCount, image roles, BeginUpdate/EndUpdate, Sort, Refresh, NextSelected, SetItemSelected, BeginLabelEdit and EnsureVisible |
| Items/Columns | Count, GetAt/GetItem/indexed access, Add, Insert, Find, Delete, Move and Clear |
| Item | Native item properties, Delete(), MoveTo() and a live SubItems facade |
| Column | Caption, Index, Width, Alignment, DisplayIndex, ID, Attached, AutoSize(), Delete() and MoveTo() |
| SubItems | Count, indexed get/set, GetAt/GetItem, Add, Insert, Replace, Delete and Clear |

Both view.Items[index] and collection[index] work. Item.SubItems[index] and a
retained SubItems facade update the same native row immediately.

Data uses a private retained QVARIANT holder in the adapter. Scalars, strings,
arrays and script objects retain their identity/ownership, including indexed
array access through Data. Arbitrary native IUnknown payloads are exposed through
an opaque adapter and can be assigned back; they are never cast to IQuadooObject.
QuadooForm.dll has no QuadooScript dependency.

The object-valued event dispatcher queries public TreeNode, ListItem and ListColumn
capabilities. Existing TreeView bindings continue to work. Stored wrappers are
cached by canonical IUnknown identity, with bounded detached-entry pruning,
explicit pruning after script deletion/clear and cache clearing on form detachment.
Virtual request objects are excluded from that cache.

Native item resolution validates its reported current index and canonical identity,
avoiding a linear scan on every cell assignment. Sorting stages a stable merge
permutation before the native commit.

## Bounds and serialization

Stored mode permits up to 1,000,000 rows, 4,096 columns and 4,096 subitem strings
per row. Captions/cells permit at most 32,767 UTF-16 code units and reject embedded
NULs. Custom StateIndex permits -1 through 14; checkbox mode permits 0/1 and
Checked is available only in checkbox mode.

Assigning an unchanged serialized collection returns S_FALSE and preserves its
objects, HWND, runtime Data and selection. Candidate JSON validates object fields,
string types, indexes, GUID uniqueness, state representation and OwnerData conflicts
before replacement. Data and native
temporary state are never serialized. Items stays hidden in the inspector:
there is still no Items Editor or generated script member for an item/column.

## Validation

The VS2008 Win32 Debug and Release runtime, harness, private tests, shared designer,
standalone host, AQVMT, Studio and ApplicationTests projects build successfully.
No SimbeyCore, JSON parser or QuadooScript compiler/VM project was rebuilt.

Final suite totals:

| Suite | Debug | Release |
| --- | ---: | ---: |
| Focused native ListView, rows and resource lifetime | 276 | 276 |
| Focused rollback and independent image/popup providers | 95 | 95 |
| Shared designer references, stored rows and clone IDs | 43 | 43 |
| Full native harness | 9,137 | 9,137 |
| Full private native tests | 792 | 792 |
| Full shared designer | 21,803 | 21,802 |
| Full Studio integration | 545 | 545 |
| Full application runtime, including ListView scripts | 127 | 127 |
| Stage-one classic native probe | 601 | 601 |
| Stage-one modern-host v6 native probe | 504 | 504 |
| Stage-one managed-image contract | 24 | 24 |
| Stage-one script contract | 14 | 14 |

All listed completed runs return S_OK. Native coverage includes real inline edit
accept/cancel and form-key preprocessing, row/cell and keyboard popups, checkbox
batching/cancellation/replacement, stable sorting/identity, column migration,
snapshot independence, JSON round trips, COM identity, event destruction,
payload release and factory/GDI/USER lifetime.

Failure injection covers native insertion, subitem setup, deletion, clearing,
sorting and fully populated candidate HWND failures. It verifies unchanged
definitions, row identity, selection, handles and cells. Malformed item JSON
and independent popup-time row mutation are covered.

Application fixtures populate 1,000 rows per run and verify live strings,
scalar/string/array/object/form Data, direct indexed collections, cancellation,
sorting, detachment and wrapper identity. A native-to-script test verifies column
event identity and opaque native Data round trips. Existing TreeView script tests
run in the same suite.

The first ordinary-desktop attempts encountered existing USER-object/OLE-window
accounting failures and color-picker focus/time-out failures outside ListView.
Failed logs were retained. An independent color test passed, and the full affected
native/designer suites passed on isolated desktops without changing those tests.
The one-assertion Debug/Release designer count difference is configuration-specific.

TestDesktop.cpp provides an optional test launcher. It creates a temporary desktop
for each test process, never switches the input desktop, inherits captured output,
waits for completion and disposes the desktop. It isolates focus and accessibility
activity while keeping the same test assertions and runtime binaries.

Reproduce from the repository root:

```powershell
& tests\platform\QuadooForm\test-listview-stage3.ps1 -Configuration All -Build -Full -IsolatedDesktop
```

Logs are under tests/results/ListViewStage3Logs. Ordinary-desktop failures remain in
the original full-suite logs; successful final native runs are named
Debug/Release-final-full-native.log. The full designer passes are named
Debug/Release-isolated-full-designer.log.
Final focused results are also summarized in target/ListViewStage3-final-focused.log.
After the full designer/Studio passes, a final event-state consistency assertion
and unchanged-collection safeguards were added. Both builds and the complete
native/private/application suites and focused designer tests were then rerun.

The Debug/Release SimbeyCore.dll hashes are unchanged. The user-supplied XP probe
logs are unchanged. Release QuadooForm.dll and AQVMT.bin are x86 PE images with
OS/subsystem version 5.00; the runtime import audit found no newly required
post-XP APIs.

## Remaining stages

OwnerData count/generation, provider snapshots, virtual requests/writeback and the
XP checkbox fallback remain stage four. Virtual mutations/count setting are
explicitly unimplemented; they do not fabricate stored items.

The Columns Editor and dedicated ListView palette artwork remain stage five.
No actual-item designer UI, ListView groups, owner drawing, subitem editor,
ToolBar or unrelated control was added.

Actual Windows XP wrapper execution, final x64 regression coverage and visual
theme/DPI review remain stage-six validation work. The earlier XP probe findings
remain authoritative: both XP classic and v6 require the planned virtual checkbox
fallback. This stage did not perform a new XP VM run.
