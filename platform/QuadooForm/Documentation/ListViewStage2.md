# ListView stage two: native foundation, columns, and references

Date: 2026-10-25.

Status: complete. Win32 Debug and Release builds and validation pass.

This report records the stage-two boundary. Stored rows and script wrappers were
subsequently implemented in [ListViewStage3.md](ListViewStage3.md).

## Scope

The runtime now registers a native Win32 ListView factory with per-type metadata,
inherited window-control behavior, four classic views, and the planned scalar
style properties. It supports an empty stored or OwnerData view in this stage.
The implementation uses the existing VS2008 component architecture and custom
containers, with no STL, exceptions, or QuadooScript dependency.

The native implementation is split across QuadooForm/ListView.h, ListView.cpp,
ListMetadata.cpp, ListColumns.cpp, ListModel.cpp, ListImages.cpp, ListNative.cpp,
and the explicitly deferred row-method entry points in ListItems.cpp.

## Additive public ABI

The five reviewed interfaces and fixed enums from stage one are appended to
Published/QuadooForm.h: IQuadooListView, IQuadooListItems, IQuadooListItem,
IQuadooListColumns, and IQuadooListColumn, UUID suffixes 0043 through 0047.
Existing interfaces, UUIDs, vtables, exports, and ordinals are unchanged. The
probe contract header now includes the published declarations rather than
maintaining duplicate definitions.

IQuadooListView exposes the implemented columns, three image roles, update
batching, and Refresh operations. Row/selection/data-generation/label-edit and
OwnerData operations currently return E_NOTIMPL, with interface outputs cleared.
No IQuadooListItems or IQuadooListItem implementation is advertised yet. Nonempty
Items JSON is rejected with E_NOTIMPL rather than discarded. This preserves the
complete reviewed interface layout while stages three and four supply the row
implementations. All property/event descriptors use the reviewed signatures;
only OnColumnClick and empty-area OnContextPopup are active new events here.

## Columns and native synchronization

Columns are lightweight reference-counted objects with stable GUID/COM identity.
They are not registered components. The collection supports insertion, deletion,
logical movement, display reordering, clearing, GUID lookup, captions, widths,
alignment, and autosizing. Retained detached columns expose ID and Attached;
other calls return EDestroyed. Retained collections/columns keep their factory
busy after the native control is destroyed, without retaining a usable window.

Logical indexes and display positions are distinct. Native header resizing and
dragging are read back after the notification has completed. Synchronous reads
also flush pending header changes. JSON preserves column GUIDs and a validated
display-order permutation. Columns are limited to 4,096 entries; captions to
32,767 UTF-16 units without embedded NULs; widths to 0..1,000,000 logical pixels.
These limits do not impose a future flat-row collection limit.

Logical column zero must be left aligned. Setting another alignment there, or
deleting/moving columns so a non-left-aligned column would become logical zero,
returns E_INVALIDARG. Change that column to Left before the structural operation.
Widths remain in 96-DPI units and are converted explicitly for native headers.
Header autosizing measures the caption; content autosizing currently has no rows
to measure and uses the empty-cell minimum, pending stage three.

Style and column changes stage a replacement HWND. DPI changes also stage a
replacement with newly acquired image snapshots. Old windows, fonts, snapshots,
and model state remain available until commit, and are restored on failure.
Reacquire IQuadooWinControl::GetHandle after successful style, column, or DPI
changes. Update batches coalesce property notifications; row-paint optimization
belongs with the future row implementation.

ViewStyle accepts named or numeric enum input and follows the existing framework's
numeric JSON output convention. This is a refinement of the original proposal's
suggestion to emit named enum strings: keeping the shared property representation
avoids a ListView-specific inspector or selection-property exception.

## Images, popups, and XP findings

LargeImages, SmallImages, and StateImages resolve through the owning Form/Frame's
component namespace and query only published capabilities. Typed assignments
reject foreign components even when their GUID matches a local component. All
roles use retained IQuadooImageListSnapshot objects and LVS_SHAREIMAGELISTS.
CheckBoxes and StateImages are mutually exclusive. Each HWND separately owns its
generated checkbox list, which is retired during WM_DESTROY.

Source acquisition and observer subscription happen before replacing a binding.
Failed subscription is cleaned up even when a third-party source registered the
observer before reporting failure. DPI refresh acquires all roles before changing
handles. Source destruction detaches native handles and clears references. Errors
from synchronous operations are returned directly; asynchronous observer failures
use the existing callback-error reporting path. Provider callbacks cannot destroy
the control during a staged replacement or image-binding operation.

WM_CONTEXTMENU is the single automatic popup path. Empty-area callbacks receive
Item=null and ColumnIndex=-1; negative mouse coordinates are preserved. Keyboard
requests anchor below the visible header in an empty Report view. Header requests
are excluded. The event guard ends before native menu invocation so menu actions
can edit the control. Design mode suppresses application popup callbacks.
Row hit testing, selection, and focused-row keyboard placement arrive with rows.

The shared inspector filters all three image-reference dropdowns by the published
ImageList capability. The dedicated Columns Editor and ListView artwork remain
stage-five work; the registered factory currently reuses the ListBox icon resource.

The supplied XP stage-one logs remain the compatibility evidence for native row
behavior: explicit virtual-checkbox writeback is required, and both XP classic
and v6 require measured Icon state-slot geometry and the SmallIcon/image-less
Icon composite fallback. Stage four must implement those demonstrated paths;
v6 activation must not bypass them. This stage does not claim virtual rows or a
new XP VM run of the runtime wrapper.

## Validation

Run focused tests from the repository root:

```powershell
.\tests\platform\QuadooForm\test-listview-stage2.ps1 -Build
```

For the affected Win32 Debug/Release regression suites and stage-one probes:

```powershell
.\tests\platform\QuadooForm\test-listview-stage2.ps1 -Build -Full
```

Logs are written to tests/results/ListViewStage2Logs and tests/results/ListViewStage1Logs.
The build scripts build only the named projects; they do not rebuild SimbeyCore,
the JSON library, or QuadooScript's compiler/VM DLLs. Full mode builds the existing
designer, Studio, AQVMT, and application tests against the additive header.

Coverage includes all view mappings, styles, native columns, DPI 96/144/192,
independent image and popup providers using only published interfaces, native
event arguments, header synchronization, JSON round trips, failed HWND creation,
malformed/duplicate column definitions, failed image acquisition/subscription,
blocked provider destruction during staging, factory lifetime, and repeated
creation/replacement/destruction with warmed GDI/USER resource counts.

| Validation | Debug assertions | Release assertions | Result |
| --- | --- | --- | --- |
| ListView native foundation and resource cycles | 166 | 166 | Passed |
| ListView failure injection and independent providers | 45 | 45 | Passed |
| ListView shared inspector/reference integration | 39 | 39 | Passed |
| Full native harness | 9,027 | 9,027 | Passed |
| Full native metadata/storage/private tests | 742 | 742 | Passed |
| Full shared designer suite | 21,799 | 21,799 | Passed |
| Full Studio integration suite | 545 | 545 | Passed |
| Full application/script runtime suite | 109 | 109 | Passed |

The stage-one regressions also pass in both configurations: classic native probe
601 assertions, v6 native probe 504, managed ImageList contract 24, and script
contract 14. The last queued-header setter fix was followed by both complete
native suites and focused designer/reference tests in both configurations. The
earlier complete designer, Studio, and script runs passed with the shared-code
changes. Debug negative-test traces are expected; all final HRESULTs are S_OK.

Resource checks include 20 measured cycles after warm-up. The Debug and Release
SimbeyCore.dll SHA-256 hashes match their pre-build values. No new DLL imports
are introduced by the ListView files: Win32/common-control operations use the
existing imported APIs. Release PE inspection confirms x86 and OS/subsystem
version 5.00. No x64 build or new runtime-wrapper XP VM run is claimed
for this stage; the preserved XP logs apply to the stage-one probes.

## Following stages

Stage three implements stored items and their script wrappers, actual row events,
selection, editing, sorting, payload ownership, and JSON row persistence. Stage
four implements OwnerData requests/writeback and the measured XP fallbacks. Stage
five implements the dedicated Columns Editor and artwork. Full wrapper visual,
theming, physical-DPI, and XP validation remain implementation-stage work.
