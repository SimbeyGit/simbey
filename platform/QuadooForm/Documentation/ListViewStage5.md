# ListView stage five: designer and Studio

Implemented 2026-10-26. No published ABI, compiler or VM changes are required.

## Shared Columns Editor

The standalone Designer and Quadoo Studio use the same ListColumnsEditor.cpp
implementation in the existing designer core library. No new DLL or executable
was introduced. Open it from Form > ListView Columns Editor, the control context
menu, or by double-clicking the Columns property of a selected ListView.

The dialog provides a logical-order column list, Caption, Width, Alignment,
Display position, Add/Delete/Up/Down/Clear, Apply Column, Size to Caption, a
read-only Report header preview, and OK/Cancel/Apply. The first logical column
remains left aligned, following the existing native collection contract. Up/Down
moves logical indexes; Display position edits the independent visual order.
Collection GUIDs remain stable. Captions are not component or script-member names.

A CListColumnsEditSession clones the current design document into a hidden
working document using public ListView/column capabilities. It commits through
the existing ApplyComponentProperties/source-preparation transaction. Cancel
discards edits since the last Apply; Apply followed by Cancel retains the last
applied state. No-op Apply preserves the document. Column insertion, deletion
and movement also carry existing row-cell associations through the transaction.
There is no row/item editing UI.

Collection-editor commands require a single capable component. In a multiple
ListView selection, Columns is read-only while shared scalar properties remain
editable through the existing selection abstraction. ImageList and PopupMenu
reference behavior and dropdowns continue to use the common component namespace.
Studio source tests verify one ListView member, no column declarations, no column
Objects-tree nodes, and unchanged script text after column edits.

The inspector dispatch now compares retained RSTRING property names by contents.
The new double-click test exposed a raw-handle comparison that otherwise sent
Columns to the generic JSON editor; the shared collection dispatch also uses
content comparison for TreeView Items.

## DPI and artwork

The existing native model stores Width in logical pixels at 96 DPI. Serialization
retains that value; PopulateColumns and the OwnerData in-place rebuild scale it
with MulDiv(width, currentDPI, 96) at the HWND boundary. Native header resizes are
normalized back with the native header's DPI before saving, including when the
posted synchronization message has not yet run. The editor's Width field is
explicitly labeled 96 DPI, and its header preview scales for the dialog DPI.

A regression test also exposed lost widths when a DPI change arrived before
the queued header-sync message. Arrange now consumes pending header edits
before changing the font/DPI or rebuilding columns. The test failed before this
fix and now covers both stored and OwnerData paths.

New tests cover stored and OwnerData controls at 96, 144 and 192 DPI, change a
native header width to 151 physical pixels, serialize immediately, inspect its
normalized JSON value, and reload at 144 DPI. They also verify that an untouched
101-pixel logical width does not accumulate rounding changes. An additional
157-pixel resize followed immediately by a DPI transition retains its normalized
width and scales the replacement/in-place native columns correctly.

Icons/ListView.png and ListView.ico contain a dedicated transparent 20x20 report
grid glyph, distinct from ListBox. The ICO uses a 32-bit uncompressed DIB and AND
mask compatible with XP. It is a standard icon resource in QuadooForm.dll,
returned through the existing IQuadooPaletteIcon capability. Normal builds do
not require Python/Pillow. GenerateIcons.py ListView regenerates this icon and
the palette preview without rewriting unrelated icon files.

DesignerExamples/ListViews.qform.json demonstrates three normalized columns,
an assigned ImageList and PopupMenu, and runtime-only row population.

## Validation

Visual Studio 2008 Win32 Debug and Release builds complete with zero errors and
warnings. Both configurations pass:

| Suite | Assertions |
| --- | ---: |
| Focused native ListView, including DPI and header visibility cases | 346 |
| OwnerData | 189 |
| ListView rollback | 114 |
| Shared designer ListView, including Columns Editor and icon checks | 77 |
| New Studio ListView workflow | 21 |
| Existing TreeView / ImageList collection editors | 14 / 358 |
| Complete native harness | 9,396 |
| Complete private native suite | 811 |
| Complete application/script integration | 113 |
| Classic / modern-v6 native probes | 601 / 504 |
| Managed-image / script contract probes | 24 / 14 |

The editor tests cover staged cancellation, accepted/no-op Apply, source rejection,
native dialog OK/Cancel/Apply, inspector double-click, stable GUIDs, existing
subitem associations, multi-selection restrictions and shared scalar edits.
Studio checks cover actual Form-menu dispatch, source-allocation failure rollback,
absence of column declarations/nodes, saved widths, and persisted image/popup
references. The icon test checks 20x20 output, the standard group-icon resource,
and different rendered pixels from ListBox. Manual inspection through the
computer-use skill confirmed the dialog layout and palette artwork using the
new example; the document was closed without changes.

**The complete designer and Studio suites remain blocked by the existing
clipboard-preservation fixture on this host.** CTestClipboard::Save fails at
DesignerTests/ClipboardTests.h:51 when GetClipboardData returns 0x8007054F.
This reproduces in Debug/Release on both an isolated desktop and the normal
desktop. Full designer runs stop at the context-menu/clipboard test; full Studio
runs stop inside the general integration test. The passing focused ListView
tests do not use that clipboard-preservation fixture. No assertion or resource
threshold was weakened, and no user clipboard formats were cleared to force a
pass. The previous stage's Preview GDI-resource issue is not cleared by this
run, because the clipboard failure stops the full designer suite earlier.

A private-window-station test-isolation experiment avoided the shared clipboard
but was unsuitable for existing visibility-dependent UI tests. It was reverted;
the original desktop launcher remains unchanged. Original, normal-desktop and
experimental logs are retained under tests/results/ListViewStage5Logs.

Reproduce from the repository root:

```powershell
& tests\platform\QuadooForm\test-listview-stage5.ps1 -Configuration All -Build -Full -IsolatedDesktop
```

The driver records independent suite failures and continues other suites. Full
GUI runs may reproduce the clipboard fixture failure described above. Focused
feature validation is available by omitting -Full.

SimbeyCore, JSON and QuadooScript dependency DLLs were not rebuilt. Actual XP
execution, supported x64 regressions and closure of the broad UI-test blockers
remain stage-six validation work.

## Header visibility correction

Report columns were saved correctly, but their native header stayed hidden in
both design mode and application runtime. NativeCreated suspended ListView
redraw when it saw the framework's initial construction update count. That
count is cleared by InitializeControl without calling EndUpdate, so the native
control never received WM_SETREDRAW(TRUE). The independent Columns Editor
preview did not use this initialization path and displayed the same columns.

NativeCreated now suspends redraw only for updates on a Ready control. Initial
construction already creates a hidden HWND. Explicit BeginUpdate/EndUpdate
batching and replacement HWNDs retain their existing redraw handling.

The new native and designer header-visibility checks failed before the fix and
pass afterward. Tests now check native header visibility and nonempty geometry
after creation and editor Apply, after DPI/JSON round trips in stored and
OwnerData modes, after update batches, and after reopening a Studio project.
Win32 Debug/Release focused ListView, full native/private, and full script
suites were rerun. The table above includes the additional assertions.
