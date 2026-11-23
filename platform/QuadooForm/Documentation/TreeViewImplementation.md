# TreeView and native ImageList integration

Implemented for the native runtime, shared designer, Quadoo Studio, and AQVMT
script adapter. This adds only TreeView to the built-in control set. Nodes are
collection objects, never registered components, palette tools, or script fields.

## Published interfaces and ABI

Existing interface UUIDs, vtables, exports, and export ordinals are unchanged.
The following optional interfaces were appended to Published/QuadooForm.h:

| Interface | UUID suffix | Purpose |
| --- | --- | --- |
| IQuadooEventObjectArgs | 003C | Read/write object-valued event arguments |
| IQuadooImageListSnapshot | 003D | Retained immutable native image-list handle and physical size/DPI |
| IQuadooImageListObserver | 003E | Notification of source edits and destruction |
| IQuadooImageList | 003F | Acquire a DPI-sized snapshot; subscribe/unsubscribe |
| IQuadooTreeNodes | 0040 | Root collection, GUID lookup, insertion, deletion, movement, clearing |
| IQuadooTreeNode | 0041 | Stable identity, attachment, hierarchy, text, image indexes, expansion, checks |
| IQuadooTreeView | 0042 | Collection, selection, ImageList assignment, native label editing |

EventValueType appends ValueObject. IQuadooEventArgs itself is unchanged; clients
query IQuadooEventObjectArgs when the descriptor specifies ValueObject.

All new implementations use BEGIN_UNK_MAP and the existing CBaseUnknown identity
and reference-counting machinery. TreeView exposes the ordinary window/control
interfaces and its actual TreeView capability; it does not become a container or
a registered component collection for its nodes.

Metadata validation permits Items to be a Collection on a new type, while
preserving List for the existing ComboBox/ListBox string collections. Inherited
descriptors still cannot change type or flags incompatibly. No QuadooScript VM,
parser, SimbeyCore, or IRStringArray changes were made.

## Native ImageList contract

Width/Height are logical pixels at 96 DPI. Acquire accepts 48 through 768 DPI
and returns a retained IQuadooImageListSnapshot. GetHandle returns a borrowed
HIMAGELIST: consumers must neither mutate nor destroy it. Keep the snapshot
alive until the native control has detached that handle.

Each snapshot owns its handle and retains the factory lifetime. Existing
snapshots remain valid after property edits, cache eviction, or component
destruction. Rebuilding stages all cached sizes before publishing replacements;
allocation failure preserves the previous cache. Up to eight DPI sizes are
cached. A native snapshot is bounded to 16 Mi pixels across all its cells.
Native storage uses premultiplied 32-bit images plus a transparency mask;
ColorDepth/Masked appearance and logical item order come from the existing
ImageList renderer. Stretch/Contain render from the original source at the new
size; Crop/Center preserve their logical crop and scale that completed cell.

Observers are retained. They may acquire a replacement, unsubscribe, or destroy
a consumer. Recursive source property edits during this notification return
EInvalidState. On source destruction Acquire returns EDestroyed; TreeView
detaches its handle and clears its Images property. Destruction notification
does not allocate. Retained snapshots and nodes keep the corresponding factory
busy until their final release.

The consumer uses only these published capabilities, component properties,
parent/container interfaces, and the root component namespace. An independent
factory can implement the ImageList capability without deriving from any
private runtime class.

## Tree model, properties, and persistence

TreeView uses the shared native window metadata, layout, colors, font, tab order,
keyboard, focus/OnEnter/OnLeave, lifecycle, and factory machinery. Its added
properties are:

| Property | Default | Meaning |
| --- | --- | --- |
| Items | [] | Hierarchical node definitions |
| Images | null | ImageList component GUID in this form/frame namespace |
| SelectedNode | empty string | Serialized node GUID; hidden from the property grid |
| CheckBoxes | false | Native two-state node checkboxes |
| ReadOnly | true | Prevent native label editing; false enables it |
| ShowLines / ShowButtons / ShowRoot | true | Standard native hierarchy presentation |
| HideSelection | false | Keep selection visible without focus |

Each node is serialized as an object with id, Text, ImageIndex,
SelectedImageIndex, Expanded, Checked, and an Items array of children. Missing
IDs are generated; duplicate IDs within a tree are rejected. IDs survive moves,
serialization, and reloads. The limits are 4,096 nodes, 128 hierarchy levels, and
32,767 UTF-16 code units per label, without embedded NULs. Image indexes must be
at least -1; -1 means no image, or the normal image for SelectedImageIndex.
Indexes are not renumbered when images are edited or removed.

GetCount/GetAt address roots; GetChildCount/GetChild address a node's children.
Find searches the entire tree. Insert/Move accept a nullable parent and a final
sibling index. A node cannot move into itself, its descendants, or another tree.
These operations preserve the same native node object, even when indexes change.
Deleting a branch detaches every descendant. Replacing Items detaches the previous
collection objects. Retained detached nodes still provide ID and Attached;
other native node operations return EDestroyed.

Structural changes and text/image/check updates stage a replacement HWND before
retiring the old one. Failed staging restores the previous model and native
handles. Native HWND/HTREEITEM values are transient; reacquire GetHandle after a
mutation. The published node reference and GUID are the stable identities.
Bulk initialization can set the Items JSON property in one operation.

Selection is serialized by node GUID. Selecting a hidden descendant expands its
ancestors; collapsing a selected branch selects the collapsing parent. Deleting
the selected branch or clearing/replacing Items clears selection. Selection and
expansion during construction/rebuild remain silent.

## Logical events

| Event | Arguments |
| --- | --- |
| OnChanging | Cancel (writable), Node, OldNode |
| OnChange | Node, OldNode |
| OnExpanding / OnCollapsing | Cancel (writable), Node |
| OnExpanded / OnCollapsed | Node |
| OnEditing | Cancel (writable), Node |
| OnEdited | Cancel (writable), Node, Text (writable) |
| OnCheck | Node |

Node arguments are read-only object arguments and may be null for selection
clearing. Before-event cancellation returns S_FALSE without applying that
requested change. OnEdited runs before committing the native edit so a handler
can reject or replace the new text. Programmatic SetText is not a label edit.
Unchanged selection/expanded/checked assignments produce no duplicate events.

Public node/selection mutations are rejected with EInvalidState while a tree
event or property notification is running. Native mutation messages are also
guarded against callback reentry. Destruction remains supported during callbacks;
arguments retain their nodes safely. Checkbox Space handling honors the inherited
OnKeyDown.Handled behavior. Mouse-wheel scrolling stays with the native tree.

Programmatic expansion supplies logical events explicitly because Win32 can
suppress repeat TVM_EXPAND notifications after TVIS_EXPANDEDONCE. Native duplicate
notifications are silenced. See Microsoft's
[TVM_EXPAND documentation](https://learn.microsoft.com/en-us/windows/win32/controls/tvm-expand).

Checkbox styling is applied after HWND creation and before populating items.
The generated state image lists are released before the native WM_DESTROY,
including failed staging windows, and replaced state lists are released when
normal-image dimensions change. See Microsoft's
[tree-view styles](https://learn.microsoft.com/en-us/windows/win32/controls/tree-view-control-window-styles).

## Designer and Studio

The shared Tree Nodes Editor is available from the Form menu, control context
menu, and Items property. It provides a hierarchical tree with the assigned
images, text and both image indexes, expanded/checked fields, Add Sibling,
Add Child, Delete, Up/Down, Indent/Outdent, Clear All, and OK/Cancel/Apply.

The editor works on an independent form definition and commits through the
existing document/source transaction. Cancel discards uncommitted changes.
Source preparation failure preserves the original document. TreeView remains one
GUI node in Studio's Objects tree; collection nodes do not appear there.

Images uses the existing reference dropdown filtered by IQuadooImageList.
Removing an ImageList clears the live reference. Source deletion preparation
also clears matching descriptor-defined ObjectReference values so Studio's
saved source and the live document agree and can reopen successfully.

The 20x20 TreeView icon is in Icons/TreeView.png and TreeView.ico; the ICO is a
standard QuadooForm.dll resource. DesignerExamples/TreeView.qform.json is a
standalone example with an ImageList and nested/checkable nodes.

## Script access

The existing AQVMT component adapter exposes TreeView.Items (also Nodes),
Selected (also SelectedNode), Images, and BeginLabelEdit(node).

Collections expose Count, GetAt/GetItem(index), Add(text), Insert(index,text),
Find(guid), Delete(node or index), Move(node,parent,index), and Clear(). Node.Children/Items are scoped
collections. Nodes expose ID, Attached, Index, Parent, ChildCount, Text,
ImageIndex, SelectedImageIndex, Expanded, Checked, Delete(), Expand(), Collapse(),
and MoveTo(parent,index).

```text
var root = TreeView1.Items.Add("Project");
var child = root.Children.Add("MainForm");
TreeView1.Images = ImageList1;
child.ImageIndex = 0;
child.SelectedImageIndex = 1;
TreeView1.Selected = child;
root.Expanded = true;
```

Live node wrappers are cached by native COM identity. Repeated lookups,
Selected, Parent, and event arguments return the same script object, independent
of sibling indexes. The cache prunes detached nodes and is cleared when the form
instance detaches. Its ownership does not create a script/form/node cycle.

## Validation and remaining verification

Run `tests\platform\QuadooForm\test-treeview.ps1 -Build -Regressions` from the repository
root for direct VS2008 Win32 Debug/Release builds of all affected projects and
the focused native, private, shared designer, Studio, and script suites. The
script never rebuilds SimbeyCore.

Coverage includes actual image drawing, DPI changes, hierarchy moves and invalid
cycles, stable node references and script identity, typed selection properties,
cancellation, native keyboard and label-edit notifications, callback reentry and
destruction, failed HWND staging, imaging allocation failure, JSON round trips,
source transaction rollback, dialog OK/Cancel, ImageList deletion/reopen,
factory lifetime, and repeated strict GDI/USER resource cycles.

All ten affected projects build with VS2008 in Win32 Debug and Release with zero
errors and warnings. Focused results are identical in both configurations:

| Suite | Assertions | Result |
| --- | ---: | --- |
| TreeView private runtime / failure injection | 13 | Passed |
| TreeView native harness | 300 | Passed |
| TreeView shared designer | 14 | Passed |
| TreeView Studio integration | 13 | Passed |
| TreeView compiled-script runtime | 9 native checks plus script assertions | Passed |
| Existing ImageList native harness | 25 | Passed |
| Existing ImageList designer | 358 | Passed |
| Existing ImageList Studio integration | 14 | Passed |
| Palette / factory icons / resource lifetime | 1,335 | Passed |

The full private runtime (697 assertions), native harness (8,861), shared
designer (21,669), Studio integration (419), and application-script runtime (109)
also pass in both configurations. Build and focused-test output is in
`target\TreeView-Final-Win32.log`; full-suite output is in
`target\TreeView-full-*-final.log`.

Full regression validation also updated stale palette/factory counts and
replaced the older Image/ColorSwatch modal-enum test driver with the existing
inline-dropdown driver. Those changes update test expectations for the current
designer; they do not change the existing controls' behavior.

The PE/import audit is in `target\TreeView-compat.log`. QuadooForm.dll and
AQVMT.bin are x86 PE32 images with OS/subsystem version 5.00. The new native
code uses XP-era tree/common-control/image-list APIs, without introducing
imports of newer DPI APIs. Native checkbox styles are applied after creating
the HWND; their state image lists are explicitly released before WM_DESTROY.
All four existing SimbeyCore.dll hashes remain unchanged.

Actual Windows XP guest execution remains unverified: no accessible XP runner
is configured on this host. The final Win32 binaries and `--tree-only` harness
mode are ready for the user's VM. No unresolved public-interface decision
blocks use of the implementation.
