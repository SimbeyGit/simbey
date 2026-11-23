# Quadoo Studio: Application projects and embedded QuadooForm designer

Status: stages 1 and 2 implemented, 2026-10-16; stage 3 implemented, 2026-10-19;
stage 4A non-debug Application Run implemented, 2026-10-20.
Reports are `ApplicationDesignerIntegration.md`, `SourceSynchronization.md` and
`Stage4Validation.md`. Stages 4B and 5 remain planned.
Stage 4A supplies AQVMT and memory-only startup compilation;
see [Stage4ApplicationRuntimePlan.md](Stage4ApplicationRuntimePlan.md) for the
source findings, delivery sequence, validation and remaining scope.
The [uniform application lifetime follow-up](ApplicationLifetimeImplementation.md)
is implemented, October 28, 2026: Default selects startup only, every live Form
is treated equally, and normal shutdown waits for the last Form. The generated
helper and blocking host pump remain controlled by script WinMain. Local
Application debugging is the next runtime stage.
The next native-control proposal is
[QuadooForm ListView](../../platform/QuadooForm/QuadooForm_ListView_Design.md),
covering runtime collections, OwnerData, script binding, and the shared Columns Editor.
The proposed image-button component is documented in
[QuadooForm ImageButton](../../platform/QuadooForm/QuadooForm_ImageButton_Plan.md),
including shared button presentation, embedded normal/disabled pictures,
automatic grayscale, and palette artwork.
The design below records the broader plan; the report records the delivered
layout, commands and remaining limitations.

## 1. Recommended result

Add **Application** alongside Console, Windowed and Web Service. An Application
contains ordinary QuadooScript files and form/frame source units. Each source
unit pairs one script class with one existing-format `.qform.json` or
`.qframe.json` definition. Keep a single document tab for each source unit, with
Code and Design views and a toggle command. Opening the script node selects Code;
opening its form/frame child selects Design.

Reuse Studio's existing project tree and status bar. The project tree can host
the complete GUI hierarchy beneath the corresponding source file:

```text
MyApplication
  MainForm.quadoo                      [Code; Default startup form]
    MainForm (Form)                    [Design]
      details (Panel)
        nameEdit (TextBox)
        saveButton (Button)
      pages (TabControl)
        generalPage (TabPage)
        advancedPage (TabPage)
      openFile (OpenFile)
      refreshTimer (Timer)
  DetailsFrame.quadoo                  [Code]
    DetailsFrame (Frame)               [Design]
      heading (Label)
```

Both source-file nodes remain real source entries: breakpoint support, source
navigation and compiler inputs continue to use their `.quadoo` files. JSON is
associated metadata/storage, not a script file disguised as a project tab.
Forms and Frames remain independent definition roots. Neither appears in the
palette; this integration adds no nested Form/Frame definition handling.

## 2. Findings from the existing implementation

Paths in this table are relative to the repository root.

| Existing area | Relevant source | Consequence |
| --- | --- | --- |
| New projects store `target` as `qvm`, `wqvm` or `web`; initially `files` is empty | `desktop/QuadooStudio/QuadooStudio/NewProjectDlg.cpp`, `QuadooStudio.cpp::CreateNewProject` | Add an explicit Application target and unit scaffolding. |
| Project files are objects containing `path`, optional `default` and breakpoints | `QuadooProject.cpp::Initialize`, `UpdateFiles` | Extend each associated source entry; update the writer, which currently reconstructs entries and would discard new fields. |
| File tree `lParam` is a raw `CProjectTab*`; every non-root context menu assumes a file | `QuadooProject.cpp::AddFile`, `GetProjectFromTreeItem`, `ShowTreeContext` | Introduce typed project-tree records before adding GUI nodes. |
| `CTabs` stores opaque data, but source switching always uses one shared `ICodeEditor` | `Tabs.h`, `QuadooProject.cpp::SwitchToFile`, `ResizeEditor`, `WM_SETFOCUS` | Tabs can be reused; activation, source-buffer transfer, layout and focus need explicit Code/Design paths. |
| Source-only casts exist in project loading, breakpoint persistence and active-file lookup | `QuadooProject.cpp::Initialize`, `UpdateFiles`, `GetActiveProjectFile`, `BuildBreakpointData` | Keep the file map source-based and audit all these paths. A designer selection must never become a `CProjectFile*`. |
| Studio creates a single four-part status bar; cursor notifications write parts 1 and 2 | `QuadooStudio.cpp::OnCreate`, `WM_SIZE`, `WM_NOTIFY` | Add active-view status routing to that bar. |
| The editor is already a child-window component in a static library | `platform/QuadooForm/Designer/Designer.h`, `QuadooFormDesignerCore.vcproj` | Link the core into Studio; do not launch or embed the standalone executable. |
| Designer status bar, menus and main loop belong to the standalone host | `platform/QuadooForm/DesignerHost/Program.cpp` | Reuse editor status/zoom; Studio supplies the surrounding application UI. |
| Designer tree creation, layout and refresh are currently internal | `Designer/Editor.cpp::Create`, `AttachNodes`, `Refresh`; `Surface.cpp::Layout` | Sharing is feasible after separating hierarchy observation from tree rendering. Merely assigning Studio's HWND is unsafe: refresh calls `TreeView_DeleteAllItems`. |
| Designer tree nodes use indexes into `m_nodes`; notifications go to the tree's parent | `Designer/Editor.cpp`, `Surface.cpp::Message` | Use a host projection and explicit notification routing instead of mixing these indexes with Studio's file pointers. |
| Change messages currently contain no session identity | `Designer/Editor.cpp::Notify` | Identify the originating unit so hidden designers cannot overwrite active status or update the wrong tree branch. |
| Studio has Win32/x64 configurations; QuadooForm and designer projects currently have Win32 only | Studio solution and QuadooForm `.vcproj` files | Matching x64 runtime/designer builds are required for the x64 Studio shown in the screenshot. |
| Current VM interfaces already expose external class loading and interface method resolution | `platform/QuadooScript/Published/QuadooVM.h` | Build the script adapter against these interfaces without changing VM/compiler contracts. |
| Existing executable compilation embeds QBC, ARGS and the application icon | `QuadooStudio/ProjectCompilerDlg.cpp` | Application packaging must additionally embed definitions and a unit catalog; selecting WQVM alone does not supply form scripting. |

This is a source review, not a build or live-UI verification. Existing x64 JSON,
SimbeyCore and QuadooScript dependency DLLs are present in the target folders;
their presence does not establish compatibility of a future x64 QuadooForm build.

## 3. Designer host integration

Keep `QuadooFormDesignerCore.lib` as the common implementation for standalone and
Studio. Add a small host-facing header to the designer project, separate from
the native runtime's `Published/QuadooForm.h`.

The host contract should cover:

- A stable session/unit token, change reasons and document revision.
- Hierarchy snapshots: component GUID, parent GUID, name, type and whether the
  item is visual, nonvisual or a placeholder. The Components group is a tree
  presentation detail, not a synthetic runtime component.
- Committed selection changes and selecting a component by GUID.
- Status, zoom, dirty state and command availability queries.
- Command execution, committing/canceling property edits and message translation.
- Host requests for source navigation and, later, preparing coordinated source
  edits for component creation, deletion, renaming and event binding.
- Creation options for standalone layout versus embedded layout, including a
  palette parent supplied by Studio.

The editor retains its component index and observers even when it has no
internal Objects tree. Remove the assumption that observer attachment and
component lookup require inserting `HTREEITEM`s. Its existing tree becomes a
standalone implementation of the same hierarchy presentation contract.

Keep the current `CDesignerDocument` as the authoritative mutable definition
inside an open session. The Studio unit object owns/coordinates that session and
its associated source; it must not maintain a second independently editable JSON
model. Saved snapshots used by inactive tree branches are only projections.

Prefer pullable immutable snapshots with coalesced change notifications; an
initial implementation can refresh only the affected unit's projection. Use
granular updates where needed to retain selection and expansion. Do not add a
general docking framework or move native runtime behavior into Studio.

The host owns sessions. A reference-counted callback sink can hold a weak owner
pointer and be explicitly detached before the host or editor windows disappear,
following the designer observer's existing pattern. Posted messages carry a
validated session token/generation, not a borrowed object pointer. Late messages
from closed/replaced sessions are ignored. All HWND and registry work remains on
the owning UI thread.

### Embedded layout

Use Studio's left region for the shared project/object tree above the palette,
with a horizontal splitter. In Code view, hide the palette and give the tree the
available height. In Design view, show the active designer's palette below it.
The document region contains the canvas, component tray and Properties/Events
inspector. Do not create the designer's internal Objects pane or left-column
splitter in this mode.

Add a small left-pane host window in Studio. `CSplitter` resizes that host rather
than the tree directly; the host lays out the existing tree and active palette.
Create palette windows under the supplied palette parent, and forward their
notifications to the owning editor/session. Do not assume a palette notification
still arrives at the editor window after changing its parent. Inactive palettes
are hidden and their selection/scroll state retained.

Keep the standalone layout as another consumer of the same editor. Preserve its
current palette focus/scroll fix, native buffering, TabPage placement and
property editing behavior.

## 4. One shared tree: ownership and routing

Studio owns the tree HWND, image list and every item's `lParam`. Add a project
tree record containing a node category, owning source/unit identity and optional
component GUID. Categories distinguish project, source, design component and
presentation group; they do not encode built-in control capabilities.

Use `(unit GUID, component GUID)` as the GUI lookup key. Display names and tree
handles are mutable presentation state. Tree nodes store identities, not retained
native component pointers, so closed documents do not leave dangling pointers
or keep factory DLLs loaded.

The Studio tree adapter must:

1. Insert the Form/Frame root immediately under its source node, then project
   visual children, container nesting and nonvisual components below it.
2. Limit updates/deletes to that unit's subtree. Never invoke a designer-wide
   `TreeView_DeleteAllItems` on the shared tree or replace its image list.
3. Preserve unrelated source nodes, other units, expansion, selected identity and
   the first visible item. Ordinary refresh must not reveal a different branch.
4. Route selection, activation, infotips, context menus and deletion by node
   category. Studio remains the receiver of notifications from its tree.
5. Activate the owning Design view before calling `SelectID` for a GUI node.
   Single selection of GUI nodes selects the component; source-file activation
   opens Code. Selection/activation of a group has no file-removal semantics.
6. Suppress recursive selection notifications while reflecting canvas selection.
   If an inline property commit fails, keep the current view and restore the last
   committed tree selection rather than switching documents anyway.
7. Detach routing before deleting records. Process deletion notifications with
   explicit ownership so records are freed once and stale handles are rejected.

Populate inactive/closed units lazily from saved JSON, with a child placeholder
until expanded. This avoids creating HWNDs or running extension factories just
to show the project tree. Active sessions publish the authoritative live outline.
If loading a definition fails, retain the source node and association and show
the diagnostic on its design child; do not replace a valid active document.

Request tree artwork through `IQuadooPaletteIcon`, including external factories.
Copy it into Studio's own tree image cache at its tree icon size and destroy the
returned HICON. Palette/tray can retain their shared 20-pixel image list; the
tree's dimensions and lifetime are independent. Do not use type-specific icon
resource IDs in Studio.

## 5. Application project model and files

Recommend `target: "application"` as a distinct project type, with an
`application.schemaVersion` for its additional metadata. Existing target values
and ordinary source entries retain their current meanings. A representative
fragment is:

```json
{
  "target": "application",
  "args": "",
  "argsEmbedded": "",
  "startDir": ".\\",
  "application": {
    "schemaVersion": 1
  },
  "files": [
    {
      "path": ".\\MainForm.quadoo",
      "default": true,
      "designer": {
        "unitId": "{11111111-1111-4111-8111-111111111111}",
        "kind": "form",
        "definition": ".\\MainForm.qform.json",
        "class": "MainForm",
        "eventsInterface": "IMainForm"
      }
    }
  ]
}
```

This is proposed project metadata, not a change to QuadooForm definition JSON.
`unitId` is independent of the root component GUID and survives a path rename.
`kind` describes only the associated definition root. A frame uses `frame` and
`.qframe.json`. The catalog maps unit identity to resource and binding metadata.

The source entry marked `default: true` is the authoritative startup-form
selection. On startup, the privately generated main code includes that Default
form source and creates its associated form class, using the source entry's
`designer` metadata to resolve the class and definition. It must not hard-code
MainForm.quadoo or prefer a stale `application.mainForm` value over Default.
The lifetime follow-up removes that redundant project field, ignores
the legacy cache on load, and removes it on an explicit normal save. The embedded
catalog uses startup terminology; it does not assign a runtime Form role.

Require exactly one Default source with a valid `designer.kind: "form"`
association before generating startup code. Report a clear error for no Default,
multiple Defaults, a Default frame/helper source, or a missing form association.
Changing Default selects the form created by the next generated startup program;
other forms are created explicitly by application code.

New Application creation uses the Form menu's Add Form Source dialog and
NewFormUnit path so the user chooses the first source's name and location.
Mark that source Default and record its unit ID as the startup form. Canceling
the form dialog leaves an empty project open; its first subsequently created
form becomes Default. Later forms preserve the existing Default. Startup code is
generated only when build/run support needs it; it is never registered in the
project or displayed to the user.
Add Project > New Form / New Frame and Add Existing
Form/Frame Unit. Use explicit association metadata instead of filename guessing.
Initially associate one design root/class per source file. Ordinary files remain
available for arbitrary helper classes.

Keep `m_mapFiles` a map of sources. A proposed `CFormProjectFile`, derived from
`CProjectFile`, adds its unit association and a lazily created designer session;
do not insert JSON files or GUI nodes as fake `CProjectTab` entries. Retain and
round-trip additional project fields when `UpdateFiles` writes entries. Validate
duplicate associations, missing definitions, root-kind mismatches and invalid
script identifiers without discarding the source or definition.

Application target dispatch must be explicit. Currently an unrecognized
non-`wqvm` target can retain the console engine path. Until the Application host
exists, enable native Preview but disable Application Run/Debug/Compile with a
clear availability message. Never silently execute an Application through QVM.

## 6. Code/Design views, commands and status

One source tab represents both views of a form unit. Keep its design session
alive while switching to Code, avoiding serialization/reconstruction simply to
change views. Closing its tab prompts for the unit's changes and can release the
designer; a lightweight saved outline remains in the project tree.

Extend the current tab/custom-layout mechanism with explicit active-view
activation, deactivation, resize, focus, message translation and command routing.
The existing custom-layout height hook alone is insufficient: Studio still
unconditionally restores/enables/focuses its shared code editor. In Design view,
hide that editor and guard source-only actions and debugger hover/breakpoint
paths. In Code view, restore its existing document, caret, selection and scroll.

Provide an accessor for the correct source buffer whether it is currently in
the shared editor, retained by an inactive `CProjectFile`, or not yet loaded.
Source generation must not accidentally edit whichever other script is visible.
The unit's modified state is the union of source, definition and generated
binding changes; tab highlighting reflects that state, not vice versa.

Route existing Save/Save All, close, Delete and view commands through the active
surface. Commit inline edits before save, view switches and project commands.
An invalid edit cancels the command. Let text fields retain text-editing keys;
run designer message translation before Studio accelerators. Preserve F5 for
Start Debugging and Ctrl+F5 for Run. Give native Designer Preview a separate
command/shortcut (proposed Ctrl+F9); do not inherit the standalone F5 binding.
Add a context-sensitive Form menu for Preview, Tab Order and selection commands,
and View commands for Code/Design, zoom, grid and snap.

Native Preview remains Form-only. Editing a Frame does not manufacture a Form
definition or a declarative frame attachment just to enable Preview.

Reuse the four status fields with active-view content:

| View | Part 0 (wide) | Part 1 | Part 2 | Part 3 |
| --- | --- | --- | --- | --- |
| Code | Existing project/debug status | Ln | Col | Existing/reserved |
| Design | Selection, root client size, requested bounds or diagnostic | X/Y | W/H | Zoom |

Studio owns `SB_SETPARTS` and `SB_SETTEXT`; adapt widths to DPI and available
space. Initially the designer's existing `Status()` string can fill part 0,
with zoom in part 3; structured selection/bounds queries avoid parsing that text
when populating parts 1/2. Filter cursor notifications by source HWND and active
Code view, and designer updates by active session. Debug/error status takes
priority without allowing a hidden designer refresh to overwrite it. Restore
the correct fields immediately on switching views.

## 7. Save, reload and source synchronization

Treat an associated script and definition as one save/close unit. Save All and
project close must include dirty design sessions and generated source changes,
including hidden views. Designer metadata changes also dirty the project.
Removing a source removes its association/tree projection; normal Remove does
not delete either disk file. Save As/rename of a unit updates its paired paths
and project association coherently; changing Form.Name is not a file rename.

Prepare and validate all source/JSON/catalog output before replacing disk files.
Use temporary siblings, backups and rollback for ordinary failures; clear dirty
state only after every required replacement succeeds. A multi-file operation is
not made crash-atomic by individual `MoveFileEx` calls: retain a small recovery
record/backups until completion and detect unfinished saves on reopen. Preserve
the designer's UTF-8 JSON policy and source encoding conventions.

For external changes, compare saved revisions/content before overwriting. Reload
a clean unit or offer conflict resolution for a dirty unit. Missing JSON and
malformed definitions must remain repairable from Code or the file system.

Stage source synchronization after native embedding:

- Generate named-control/component property variables in a marked managed region
  of the user's script class, using GUID-to-member mappings for stable identity.
  Property variables allow the native binder to populate them through existing
  `IQuadooObject::SetProperty`; no VM member-injection feature is needed.
  Store generated-member ownership/mappings with the unit's project metadata;
  keep the existing native definition JSON contract unchanged.
- Generate an events interface containing each distinct bound method with the
  agreed single-argument handler shape. The existing QuadooScript grammar uses
  `virtual function Handler(oArgs)`, with new implementations returning true;
  the earlier `virtual bool` notation was illustrative. Keep handler bodies in user-owned source.
- Add a Create/Open Handler command to Events, preserving inline editing of
  handler-name strings. Reuse compatible methods and report conflicts.
- Rename/delete only designer-owned declarations. Do not perform unrestricted
  text substitution, delete user handler bodies, or automatically rename an
  existing handler when a control is renamed. Manual source edits remain valid.
- Edit the in-memory source through `ITextDocument` grouped operations and retain
  editor view state. Adjust breakpoints through the existing line-change path,
  or equivalent explicit notifications for edits to inactive buffers.
- Preflight managed-region boundaries, names and edits before committing a
  designer operation. Coordinate native JSON changes and source changes with
  snapshots/rollback. Observer notifications after mutation alone are not a
  transaction mechanism; direct canvas/palette operations need the same hooks.

The published parser provides compilation and definition lookup, not a source
rewrite AST. Use a conservative Studio-side scanner aware of strings, comments
and balanced scopes for managed-region edits; use existing compiler APIs for
validation at the appropriate save/build boundary. Do not claim disk-based
`QuadooFindDefinitions` reflects unsaved source, or rewrite ambiguous code.
Coordinated cross-view undo can be added with an explicit edit history; do not
advertise it merely because source text edits are grouped.

## 8. Script runtime and application hosts

The native editor can be integrated before this work. Native Design/Preview
continues to suppress application handlers. Run and Debug execute application
scripts in separate processes, not inside the IDE.

Implement a separate QuadooForm script adapter against the published native and
VM interfaces. The current native DLL intentionally supplies no such adapter.
AQVMT's Host implements `IExternalClassLoader`; register that Host through
`AddExternalClassLoader` before module initialization. It returns per-instance
Form/Frame adapters with access to the copied executable's catalog and embedded
JSON resources. Preserve the agreed constructor call `Form = Form.Create(this)`.

LoadExternalClass receives the script class name and instance: for
`class MainForm : extern(Form), IMainForm`, it receives MainForm, while Form is
the member variable initialized with its returned external adapter. Host uses
the supplied class name directly to resolve the catalog's kind, definition
resource and events-interface name. Use that class as the resource name
(proposed types QFORM/QFRAME). Scripts access native capabilities through the
member, such as Form.Close() and Form.ModalResult. The loader only implements
Create, which returns a distinct live form/frame wrapper. A generated __QuadooRoot
setter installs that live object before OnCreate. No loose JSON lookup,
class-name reflection or __QuadooUnitId getter call is required. The stage-3
readonly property can remain as unit metadata without being a runtime lookup
dependency. Verify construction/reentrancy timing in the initial prototype.
Frame creation additionally requires an explicit live parent container.

For each runtime Form/Frame instance:

1. Resolve its unit metadata and independent definition snapshot.
2. During Create, fetch the generated interface (for example `IMainForm`) using
   `IQuadooObject::GetInterface`.
3. Implement `IQuadooEventsBinder::BindEvent(name)` using
   `IQuadooInterface::ResolveMethod`. Each delegate retains that interface and
   the resolved `ULONG` ordinal and invokes it through `Invoke`.
4. Use `PostBind` to populate the generated named members before `OnCreate`.
   The external Form wrapper must also be usable during that callback, before
   the constructor's assignment of Create's returned value completes.
5. Wrap typed mutable event arguments, preserving descriptor types, writable
   Cancel/Handled fields and native HRESULT/script-error behavior. Specify and
   test the script Boolean return policy without treating it as an HRESULT.
6. Unbind delegates, clear script wrappers and break reference cycles on destroy
   and failed creation. Test reentrant destruction and two independent instances
   of one Frame definition.

Create `platform/QuadooScript/AQVMT`, an Application QVM Template GUI executable
project patterned after WQVMT, producing AQVMT.bin. Add it to
`desktop/QuadooStudio/QuadooStudio.sln` with matching Debug/Release Win32/x64
configuration mappings and build ordering. For non-debug Run, Studio
copies the matching template into a project-named `.exe`, embeds the compiled
application/catalog/definitions and launches it in a separate process. Keep the
adapter as shared source/static code outside QuadooForm.dll; no new DLL or
VM changes are required. A narrow parser map-lookup fix supports repeated
#pragma once includes. Reuse WQVM's host and
QVMMessagePump.cpp by source reference, as WQVMT already does. Add an application
keyboard handler and application-session lifecycle coordination, with narrow shared-pump
error/teardown fixes and regressions where necessary.

For Run, Debug and Compile, build a private generated main script entirely in
memory. Include every project script using its absolute path, and define
WinMain(strCmdLine, nCmdShow) to instantiate the Default form, show it nonmodally
and run the application message pump. Resolve the class/definition through that
Default source entry's designer association. Compile the memory root with
QuadooParseTextToStream; it pulls in the real source files. Never write this
script to disk or add it to `files`, the tree or editor tabs. Use existing
pragma-once inclusion semantics for overlapping includes, and add compatible
pragma/constructor support to Studio's scaffold/source synchronizer.

Stage 4A supplies adapter/AQVMT/non-debug Run. The implemented uniform-lifetime
follow-up precedes stage 4B local debugging against the same catalog/startup
model. New frames use extern(Frame) with an
Owner constructor argument forwarded to Frame.Create(this, Owner). Native
Preview continues unchanged. AQVMT now registers a real QuadooPlugin component;
its Module loads directly or through a `Modules\` fallback and supplies the named
script member before OnCreate. General external-factory bootstrap and module
deployment remain follow-ups.

Design-time placeholder packages are specified in
[DesignTimePackagesPlan.md](../../platform/QuadooForm/Packages/DesignTimePackagesPlan.md).
Studio and the standalone Designer install their shared JSON metadata under the
current user's `%APPDATA%\QuadooForms\DesignTime` and register placeholder
factories only in their design registries. Both hosts expose an always-enabled
Form > Packages command, with shared code and dialog resources compiled directly
from `platform/QuadooForm/Packages` into each EXE. Removal checks every current
project form/frame, including unopened units and unsaved designer definitions.

Application startup must register the real controls before the generated main
code creates the Default form. Their factory type names, properties, container
capabilities and event contracts must match the placeholders used to author
the definitions. Application runtime/debug hosts never read the DesignTime
directory or register design-time placeholders. Run/Debug/Compile must provide
explicit runtime registration/bootstrap and stage the actual dependencies;
design packages do not specify executable installation/loading hooks. Saved
definitions retain ordinary control types and values, so registered real
factories are used directly when forms/frames load. Test deployment on a machine
without any installed designer packages and fail clearly for missing real types.

The host owns startup, DPI/COM initialization, uniform Form lifetime and message
pumping. Under the uniform policy, script WinMain calls a generated helper that
creates/shows the Default Form and returns, then calls the argument-free blocking
Host.RunApplication(). The helper releases startup temporaries before pumping;
normal exit follows final Form destruction and release of explicit application
lifetime leases. No Form receives a persistent startup role. Call
`IQuadooFormKeyboard::PreprocessMessage` in the documented order,
avoid duplicate dialog translation, and preserve modal owner/WM_QUIT behavior.
Use the existing debugger protocol for source lines, breakpoints and call stacks;
leave GUI inspection out of the initial debugger integration.

Replace filename inference in engine/debugger/template selection with explicit
per-target selection. Stage 4A Run already embeds QBC, a versioned unit catalog
and every Form/Frame definition as EXE resources; loose source/definition files
are not runtime requirements. Keep icon Base64 utilities in SimbeyCore. Stage 5
extends this builder to the Compile/export UI and complete redistribution.
Run stages the matching runtime DLLs and module files explicitly listed in the
Application project's Settings dialog, and embeds its selected application icon.
Transitive dependency discovery and general real-control registration remain
follow-ups; embedding JSON does not imply a dependency-free executable. Paths resolve from
known project/runtime metadata, not the user's working directory. Stage 4B local
debugging shares the catalog and preserves real included-source line identities;
remote debugging stays unavailable until equivalent resource/module staging exists.

## 9. Build and ABI work

- Add Debug/Release x64 configurations to QuadooForm, designer core and relevant
  harness/extension projects; integrate matching Win32/x64 outputs into Studio's
  solution. All in-process DLLs/extensions must match Studio's architecture.
- Preserve VC9 and Win32 XP support. Audit pointer-sized tree/session tokens,
  window data, subclass callbacks and handle casts. Test target application
  architecture separately from IDE architecture when selecting hosts/extensions.
- Align CRT, defines and SimbeyCore API-redirection policy. Studio already compiles
  shared BaseUnknown/CoreDefs/ChooseFile/TextHelpers sources also listed in the
  designer archive; check symbol ownership and link settings rather than using
  forced duplicate-symbol suppression.
- Initialize list-view/common-control classes needed by palette, tray and
  inspector; Studio currently initializes tab/tree classes explicitly.
- Reuse the installed/built JSON, SimbeyCore and QuadooScript binaries/import
  libraries. Build individual affected projects or a filtered script; do not
  rebuild SimbeyCore through Studio's existing solution dependency chain.
- Keep native runtime interface IIDs/vtables and JSON unchanged. Proposed host
  interfaces and script adapter are new, separate contracts. Publish their
  signatures/ownership for review before implementation; no core ABI change is
  currently identified as necessary.

## 10. Proposed source organization

| New source area | Responsibility |
| --- | --- |
| `platform/QuadooForm/Designer/DesignerHost.h` and an outline implementation | Host-facing options/callbacks and a hierarchy independent of the internal tree. |
| `desktop/QuadooStudio/QuadooStudio/ProjectTree.*` | Typed item records, unit subtree projection and tree notification routing. |
| `desktop/QuadooStudio/QuadooStudio/StudioDesignerSite.*` | Session notifications, palette hosting, status and command bridging. |
| `desktop/QuadooStudio/QuadooStudio/FormProjectFile.*` | Paired source/definition unit, Code/Design activation and coordinated dirty/save state. |
| `desktop/QuadooStudio/QuadooStudio/FormSourceSync.*` | Managed declarations, event navigation and prepared source edits, introduced in stage 3. |
| `desktop/QuadooStudio/QuadooStudio/ApplicationTarget.*` | Application metadata, host/template selection and packaging integration. |
| `platform/QuadooForm/ScriptAdapter` and `platform/QuadooScript/AQVMT` | Separate native/VM bridge and Application executable template; reuse WQVM host/pump sources, introduced in stage 4A. |
| `desktop/QuadooStudio/IntegrationTests` | Native Studio hosting/project/persistence regressions. |

Modify existing `QuadooProject.*`, `QuadooStudio.*`, `NewProjectDlg.*`, resources,
message-loop routing and solution/project files at their integration points.
Use the existing splitter against a left-pane container; a general splitter or
tab-control rewrite is not a prerequisite. The proposed files are organizational
boundaries, not a request to duplicate existing JSON, string, file-dialog or
reference-counting utilities.

## 11. Implementation sequence and acceptance

| Stage | Deliverable | Completion criteria |
| --- | --- | --- |
| 1. Host seams and architecture builds | Designer hierarchy/status/command adapter, embedded layout, matching Win32/x64 projects | Standalone still works; a minimal second host shares a tree without touching unrelated nodes; both architectures link with existing dependencies. |
| 2. Application projects in Studio | Project metadata/scaffolding, paired source unit, Code/Design view, shared explorer/palette/status, native Preview | Create/open/save/reopen a Form and Frame; all GUI nodes under their source; no second status bar; native Preview works without loading application scripts. Application Run/Debug/Compile unavailable until later stages. |
| 3. Source synchronization | Managed members/interfaces, Create/Open Handler, coordinated edits/save/recovery | Edits reach the correct unsaved source buffer; conflicts roll back; named controls/components and handler declarations survive save/reopen; source breakpoints remain correct. |
| 4A. Scripted Application Run (delivered) | Separate adapter, AQVMT.bin, embedded catalog/definitions and in-memory startup including all project scripts | Project-named EXE creates the Default form nonmodally; Form.Create(this), eager lookup, members before OnCreate, real events/Timer and independent runtime Frames; Ctrl+F5 works with no VM or public ABI changes. |
| 4A follow-up. Uniform application lifetime (delivered) | Session Form registry, completed-destruction notification, deferred cleanup/exit and script WinMain calling a startup helper before the argument-free blocking host pump | Any Form can close first; the last fully destroyed Form ends normal execution. Helper arguments/locals are released before pumping, and WinMain returns the pump exit code. No runtime main-form role remains; future tray services have an explicit keep-alive seam. Final-destruction capability is additive; queued script destructors run before normal exit commits. |
| 4B. Local Application debugging (proposed follow-up) | Debug host using the same adapter/catalog/startup and existing debugger protocol | F5, included-source breakpoints, stepping, exceptions and call stacks work; remote mode stays deferred until resource/module staging exists. |
| 5. Export/deployment and complete validation | Compile UI reuses the Application EXE builder, complete dependency/module staging, tested compatibility | Exported application runs from another directory without original JSON/source; redistribution and debugger staging are verified or explicitly deferred. |

Stages 1 and 2 delivered native embedding first. Stage 3 now supplies managed
property variables/interfaces, Create/Open Handler, explicit source reconciliation
and coordinated source/definition/project saves. See [SourceSynchronization.md](SourceSynchronization.md).
Its compiler/VM contract tests do not constitute an application adapter or host.
Stage 4A now supplies the separate adapter, AQVMT/resource assembly and non-debug
Run, including class-based definition lookup, PostBind population, retained
interface delegates and destruction/cycle handling. The detailed
[stage-four plan](Stage4ApplicationRuntimePlan.md) records the memory-only WinMain
build and script contract. See [stage-four validation](Stage4Validation.md).
The uniform-lifetime follow-up in
[ApplicationLifetimeImplementation.md](ApplicationLifetimeImplementation.md)
is delivered. The next runtime task is local debugging, including internal
startup/cleanup helper handling in generated-source mappings. General external
factory registration and complete deployment remain explicit follow-ups.
QuadooPlugin's real runtime
component and module-object substitution are delivered. The application runtime
test project is now under `tests/desktop/QuadooStudio/ApplicationTests`.

Add a focused Studio integration harness. Test Debug/Release on Win32 and x64,
then run existing native/designer suites against the matching configuration:

- Multiple open units, ordinary scripts and closed/lazy units sharing one tree;
  add/rename/delete/refresh cannot disturb other branches or source pointers.
- Bidirectional canvas/tree/tray selection, inactive TabPages, failed inline
  commits, unavailable extension placeholders and failed designer loads.
- Palette scroll and native timer behavior for visual/nonvisual insertion;
  keyboard routing, text Delete, F5/Ctrl+F5 versus Preview, focus after switches.
- Existing status bar only; no hidden-session/cursor overwrite; bounds/zoom and
  line/column restoration; maximize, splitters, DPI and theme redraws.
- Dirty source/design in active and inactive units, save/close cancellation,
  partial write failures, recovery, external conflicts and association removal.
- Existing Console/Windowed/Web projects, default scripts, breakpoints, Run,
  Debug and Compile retain their behavior.
- Independent third-party visual/nonvisual factories, palette/tree artwork,
  observer teardown, session closure and extension unload/resource lifetime.
- Later stages add script lifecycle/binding, failed construction, event mutation,
  repeated frames, last-Form shutdown, packaging and debug staging tests.

Retain strict resource assertions. The previously documented intermittent late
GDI/USER growth in existing suites remains an issue to track, not a reason to
raise limits or treat a clean rerun as proof that it was repaired. Actual XP and
physical DPI testing must be reported separately from source review/builds.
