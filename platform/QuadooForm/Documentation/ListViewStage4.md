# ListView stage four: OwnerData

Implementation and validation report, 2026-10-26.

## Runtime behavior

OwnerData uses the interfaces, event descriptors and enum values established in
stage two. This stage adds no published interface, UUID, vtable entry or export.
The public header changes only its index-based selection comment. QuadooForm.dll
continues to have no QuadooScript dependency.

- Runtime Count starts at zero and accepts 0 through 100,000,000. Count is not
  serialized. Stored Insert/Delete/Move/Clear/Find/Sort operations are rejected.
- Mode changes require empty stored and virtual collections, no update batch,
  edit or callback. Existing stored Data is never silently discarded.
- Every GetAt or display request has its own lightweight IQuadooListItem object:
  GUID_NULL, IsVirtual=true, fixed ItemIndex and DataGeneration. No object is
  allocated for an index until requested, and virtual wrappers bypass the
  script instance's permanent object cache.
- Caption, SubItems, Data, images/state and captured selection/focus remain
  readable after the request, generation invalidation and control destruction.
  Request setters are usable only during OnData/OnDataUpdate. They never act as
  live selection setters; use the ListView selection methods instead.
- Count changes and ResetVirtualData advance the generation. Changed Count
  preserves surviving native selection/focus; ResetVirtualData clears both.
  Both silently invalidate active edits. Refresh and column/checkbox schema
  changes also invalidate snapshots. RefreshRange only repaints its inclusive
  range and does not change the data-set generation.
- Native text buffers are filled within their supplied capacity, including
  subitems; temporary strings never escape in notification pointers. Native
  DI_SETITEM caching is disabled. Missing providers return empty/default data.
- Nested requests for independent indexes have independent storage. Recursion
  for the same index, depth above 32, and structural reentry are rejected.
  Callback destruction is supported; retained snapshots stay readable.

## Writeback and notifications

OnData supplies display data. It does not store rows. UpdateVirtualText and
UpdateVirtualChecked submit a separate proposal through OnDataUpdate. The
application must persist accepted changes before setting Handled=true.
Missing, canceled, failed and unhandled updates do not become a framework cache.
An original retained display snapshot is never reused as the mutable proposal.

Native caption editing requires ReadOnly=false and a bound OnDataUpdate handler.
OnEditing can cancel initiation; OnEdited can cancel or replace text before
OnDataUpdate. Enter accepts, Escape cancels, and count/reset cancels stale edits.
Checking runs OnItemChecking before writeback and OnItemChecked after acceptance.
Post-change notifications follow persistence; no later cancellable hook can veto
an accepted backend change.

Space proposes the focused row's inverse Checked value for selected rows, or
only the focused row when it is unselected. Inherited OnKeyDown.Handled takes
precedence. Updates are per-row backend transactions: the framework cannot undo
application work already accepted for another row if a later update fails.

OnDataHint and OnDataFind expose native cache/search requests. OnDataStateChange
reports native selection/focus ranges without expanding them into per-row
requests. Individual changes can additionally raise OnChange/OnSelectItem when
bound. Native OwnerData selection is post-change; it cannot promise the
LVN_ITEMCHANGING cancellation protocol. Explicit selection methods preflight
their own indexed target through OnChanging when bound. Internal synchronization
and count/reset remain silent.

Provider errors produce safe native output and a recoverable diagnostic. A
private optional IDiagnosticReport capability connects the built-in construction
context with AQVMT's host; it does not alter the published ABI. Third-party
reporters retain the existing OnError fallback. Script provider exceptions no
longer set the application's fatal error or post WM_QUIT. The original detailed
exception is preserved instead of being replaced by subsequent generic reports.
Ordinary application event failures retain their existing fatal-error behavior.

## Images, DPI and scale

The XP probe demonstrated missing checkbox state slots in Icon/SmallIcon on both
classic and v6 common controls. OwnerData therefore uses control-owned derived
checkbox/image composites in those views on all versions. List/Report use a
control-owned checkbox state list. Source ImageList snapshots are retained,
shared handles are never modified/destroyed, and source updates rebuild the
derived images. There is no dependency on a newer common-controls API.

Checkbox geometry scales with DPI. Image-backed rendering tests compare source
mask/transparency and color channels, allowing the one-level premultiplied-channel
rounding introduced by native GetIcon/ReplaceIcon. Derived lists have a bounded
16-million-pixel budget; failed preparation preserves existing bindings.

Virtual style, column and DPI changes retain the HWND, count and native range
selection. Header candidates are allocated before discarding the old columns;
failure restores the old header, style, font and image snapshots. This avoids
enumerating/materializing virtual rows during those changes. Content autosizing
samples at most 128 intersecting visible rows.

All external snapshot handles are validated before changing a binding or its
observers. A snapshot that initially returns a handle and then fails a later
query leaves the previous native handles, component references and subscriptions
intact. Rollback uses retained snapshots and saved native handles.

The scale test uses Report view, sets Count=100,000,000, checks private-byte
growth below 32 MiB, selects the full range, and verifies bounded data requests.
The focused Debug and Release runs measured zero additional private-byte growth.
This is a
measurement of that native control/run, not a promise that every native view or
fragmented selection has the same memory cost. Large-count Icon positioning is
not included in that measurement.

## Validation

All affected Win32 projects build with Visual Studio 2008 in Debug and Release,
with zero errors and warnings. Final results in both configurations:

| Suite | Assertions | Result |
| --- | ---: | --- |
| Focused stored/native ListView | 276 | Passed |
| Focused OwnerData | 189 | Passed |
| Focused ListView rollback/failure injection | 114 | Passed |
| Complete native harness | 9,326 | Passed |
| Complete private native suite | 811 | Passed |
| Designer ListView references | 43 | Passed |
| Complete Studio integration | 545 | Passed |
| Complete application/script integration | 113 | Passed |
| Classic / modern-v6 native probes | 601 / 504 | Passed |
| Managed image / script contract probes | 24 / 14 | Passed |

Coverage includes source-backed and image-less checkbox rendering, source alpha
and masks, all four views, DPI changes, native mouse/Space input, Enter/Escape and
replacement label edits, rejected/failed/unhandled writeback, JSON round trips,
foreign snapshots, nested requests, destruction during OnData, count/header/DPI
rollback, third-party snapshot failure, full-range selection at 100 million
rows, retained payloads, script access, modal provider failure, and repeated
GDI/USER/factory teardown checks.

**The complete designer suite does not currently pass on this host.** Both
configurations stop at the existing ExtensionsAndResources Preview check,
Tests.cpp:490, before the ListView tests execute. Initial runs observed a GDI
baseline of 29 becoming 30; independent fresh-desktop reproductions observed
28 becoming 30. USER stayed at 3. No threshold, warm-up rule or designer code
was changed to suppress it. The stage-three report also records host-sensitive
designer resource failures; the precise cause of this occurrence remains
unresolved and deserves a separate designer-resource investigation. Focused
designer ListView tests and the new native ListView resource cycles pass.

Original full-suite logs and independent designer reproduction logs are retained.
Final successful results use Debug/Release-final-full-native.log,
Debug/Release-validated-full-private.log, Debug/Release-final-rollback.log and
Debug/Release-final-{native,virtual,designer-listview,studio}.log. Final script
exception-detail checks are in Debug/Release-validated-script.log. Earlier
final-private.log files contain a new test-fixture duplicate-name failure; the
fixture was corrected and the complete private suites were rerun successfully.

Reproduce from the repository root:

```powershell
& tests\platform\QuadooForm\test-listview-stage4.ps1 -Configuration All -Build -Full -IsolatedDesktop
```

The driver reuses the individual-project stage-three build and isolated-desktop
launcher, runs the new --listview-virtual-only harness path, and writes separate
logs to tests/results/ListViewStage4Logs. It does not rebuild SimbeyCore, JSON or the
QuadooScript compiler/VM DLLs.

The Debug/Release SimbeyCore.dll SHA-256 values and the user-supplied XP classic/v6
probe log hashes match their pre-stage-four values. Final PE/import audits found
x86 targets with OS/subsystem version 5.00 and no newly required post-XP APIs.
New drawing/image imports use the established GDI/common-control APIs. Import
and header audit output is retained alongside the test logs.

## Remaining work

The Columns Editor, dedicated ListView palette icon and final designer/Studio
workflow are stage five. No actual-item designer UI or unrelated controls were
added in this stage. Final x64 coverage and actual Windows XP wrapper execution
remain stage-six validation; no new XP VM run is claimed. The user-supplied XP
classic/v6 probe logs remain the compatibility evidence for the fallback choice.
