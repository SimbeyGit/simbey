# Quadoo Studio Application designer integration

This is the original stage 1/2 report. Later work is recorded in
[SourceSynchronization.md](SourceSynchronization.md) and
[Stage4Validation.md](Stage4Validation.md); non-debug Application Run is now enabled.

Stages 1 and 2 of QuadooFormApplicationIntegrationPlan.md are implemented.
Visual Studio 2008 builds are provided for Debug/Release, Win32/x64. This pass
adds native editing and Preview; Application Run, Debug and Compile are disabled.
No script adapter, generated control variables, handler declarations or application
packaging is implemented yet. QuadooScript and SimbeyCore were not changed.

## Using the integrated designer

Creating an Application project opens the same Add Form Source dialog and
NewFormUnit path used by the Form menu. The chosen filename determines the first
form's source, definition, class and interface names. That source is marked
Default and its unit ID is recorded as the startup form. Canceling the dialog
leaves an empty Application project open; the first form created afterward is
also made Default, while later forms preserve the existing Default selection.
Application startup code is generated privately when running and creates the
form associated with the Default source. The generated main code is not a
project source or a user-visible document.
File > New Form / New Frame creates a paired unit. File >
Add Existing Form/Frame Unit associates an existing JSON definition with an
explicitly selected source. Source filenames must have valid identifier stems;
unit IDs, definition paths and class names must be distinct in the project.
New files are created exclusively, without replacing existing files.

The filename determines the initial root name and script class: OrderForm.quadoo
creates OrderForm and IOrderForm. There is no automatic C prefix. Renaming a
control in Properties does not rename a source file or rewrite user source.

Each unit has one Studio document tab with Code and Design views. Open a source
node for Code, or select its Form/Frame or component nodes for Design. F12 toggles
views. The existing explorer hosts the entire component hierarchy, including
nonvisual components directly under the root. Studio owns its tree items and
image list. The active Toolbox appears immediately below that tree; in Code view
the tree occupies the full left pane. Drag the horizontal boundary to resize it.
The canvas, nonvisual tray and inspector fill the document region.

All status is routed through Studio's existing four-part status bar. The Form
menu combines the standalone View/Form commands: Code/Design, native Preview,
Tab Order, zoom, grid, snapping and pane reset. Edit contains component deletion
and ancestor selection. Designer commands are disabled outside Design view;
unit creation/import remains available for Application projects. Source editor
Options remains in Studio's View menu. Native Preview uses Ctrl+F9; F5/Ctrl+F5
retain their debugger/run meanings and are unavailable for Application projects.
Frame Preview remains disabled. Native Preview never loads script handlers.

Save File and Save All save the associated source and definition together when
a designer session is open, including dirty inactive sessions. Dirty state comes
from the documents, not delayed tab highlighting. Invalid inline edits block view
switches, commands, saves and close prompts until corrected or canceled. Export
Definition Copy writes independent JSON without changing the association or
clearing dirty state. Removing a source from the project retains both disk files,
removes its GUI nodes, and clears application.mainForm when removing that unit.

An example with a Form, nested controls, a Timer and a separate Frame is in
Examples/Application/Application.qsproj. Its source is scaffolding for later
script integration and cannot currently run as an Application.

## Hosting and ownership

Designer/DesignerHost.h supplies embedded creation options and GUID-based outline
entries. Designer.h exposes the editor's selection, status, commands, dirty state
and inline-edit/message routing. CreateHosted omits the internal Objects tree and
creates a separate Toolbox pane under the supplied host parent. The standalone
Create path retains its existing layout and behavior.

The editor retains its own observer/component index without requiring tree HWNDs.
Studio pulls outline snapshots and projects them under their owning source using
component GUIDs. Refresh retains existing tree handles for renames/property edits,
selection, expansion, other source branches and the first visible item. Saved
outlines are parsed eagerly without constructing native controls; live editor
sessions remain lazy. There is no second editable JSON model for an open unit.

Posted change messages contain a session generation and outline revision, never
an object pointer. Closed/replaced sessions advance the generation; stale
messages are ignored. Hidden designers cannot overwrite the active status bar.
Sessions remain alive across Code/Design changes and release on tab/project close.
Tree records retain identities rather than control or factory pointers. Artwork
comes from IQuadooPaletteIcon for built-in and external factories alike; Studio
copies icons into its independent tree image cache and destroys returned HICONs.
The existing editor image list remains shared by its palette and component tray.

CProjectFile continues to represent real source inputs in m_mapFiles. It owns an
optional CStudioFormUnit and retained project-entry metadata. Ordinary project
entries and unknown metadata round-trip. m_pCodeFile identifies the actual owner
of the shared source editor independently of the active tab's Design mode.
Source-only breakpoint/hover actions are guarded when Design is active.

Published QuadooForm runtime IIDs, vtables, exports and definition JSON are
unchanged. The new hosting seam is a C++ static-library contract, separate from
the runtime DLL ABI. No STL, exceptions or new runtime script dependencies were
introduced. x64 configurations were added to runtime, designer and test projects;
one bounded lodepng palette-index conversion was made explicit for x64 builds.

## Persistence and recovery

The project target is application, with application.schemaVersion = 1 and an
optional application.mainForm unit GUID. A source entry's designer object contains
unitId, kind (form/frame), definition, class and eventsInterface. The root's
component GUID is independent of unitId. Class/interface values must match the
source filename convention. Root-kind mismatches and failed JSON loads cannot
replace the currently active document; its source remains available for repair.

Paired saves prepare source and JSON temporary siblings, create backups, then
record the two known paths in a .studio-save recovery record before replacing
either target. Backups remain until both replacements and record removal succeed.
Ordinary failures attempt rollback and retain dirty state. Reopen/retry detects
unfinished saves and restores the pair from deterministic .studio-backup siblings.
Recovery validates the record against the unit's explicit paths; it does not
execute paths supplied by a journal. Project saves use a temporary sibling and
MoveFileEx. Source-only saves of units with no open designer also use a temporary
sibling, allowing source repair when the definition is malformed or missing.

Decoded source/definition snapshots detect external content changes before save.
A conflicting save returns ERROR_REVISION_MISMATCH, preserves disk contents and
in-memory dirty state, and displays an explanation. Copy/export unsaved edits,
then close and reopen the project to accept the disk version. Automatic merge or
reload/conflict-resolution UI is not implemented. There is no filesystem watcher.
Recovery records are not a guarantee of power-loss atomicity: corrupt records or
missing backups require manual recovery and are reported without deleting the
remaining recovery evidence. Failure-injection tests cover ordinary write/lock
failure, deferred recovery and a successful retry, not a physical power failure.

## Build and validation

Use tests/desktop/QuadooStudio/build-studio.cmd [Debug|Release] [Win32|x64] [--test]. It builds only the
changed QuadooForm projects, Studio and the integration harness, reusing existing
JSON, CodeEdit, QuadooParser and SimbeyCore binaries/import libraries. It does not
build the full historical solution dependency chain. SimbeyCore DLL SHA256 hashes
are unchanged in all four output folders. No Subversion commit was made.

Builds have zero errors and warnings in all four configurations. Studio's native
integration suite covers project scaffolding/naming, shared-tree routing and
stable identity, Code/Design buffer retention, separate palettes and exact scroll
retention after deferred native input, failed inline commits, status routing,
Frame sessions, stale notifications, native Preview, independent external visual
and nonvisual factories/icons/unload, paired save failure/recovery, external
conflicts, metadata retention, malformed definitions, duplicate class rejection,
association removal and final-tab closure. It also verifies existing Console,
Windowed and Web project source trees and command availability.

The complete default runtime/private/designer suites ran in every configuration.
The runtime harness additionally ran with --continue-on-failure so a strict
resource failure would not prevent later item, TabControl, component and keyboard
tests from executing. This option retains the first failure and a failing exit
code; the default behavior and resource limits are unchanged.

| Configuration | Studio integration | Private storage/metadata | All runtime cases | Full standalone designer |
| --- | --- | --- | --- | --- |
| Debug Win32 | 87 passed | 119 passed | 7,701; USER-count failure in choice resources | 2,758; late GDI-count failure |
| Debug x64 | 87 passed | 119 passed | 7,565; USER-count failure in sizing/focus | 2,582; late GDI-count failure |
| Release Win32 | 87 passed | 119 passed | 7,769 passed | 3,000 passed |
| Release x64 | 87 passed | 119 passed | 7,752; USER-count failure in choice resources | 2,868; late GDI-count failure |

Assertion counts vary with bounded resource warm-up/measurement cycles. The
original default runtime runs stopped at their choice-resource failures in all
four configurations; those logs remain intact as well. Every ordinary control,
layout, event, serialization, rollback, external-DLL, Timer and keyboard test case
completed successfully in the continue-on-failure matrix. The remaining failures
are native resource-count checks, not an all-green validation result. Prior
isolated pre-change runtime/designer baselines already reproduced these classes
of USER/GDI increases (see the existing native/designer validation notes).

Logs under target:
- StudioEmbedding-{Debug,Release}-{Win32,x64}-build.log
- StudioEmbedding-{Debug,Release}-{Win32,x64}-integration-final.log
- StudioEmbedding-{Debug,Release}-{Win32,x64}-runtime-all.log
- StudioEmbedding-{Debug,Release}-{Win32,x64}-QuadooFormNativeTests.log
- StudioEmbedding-{Debug,Release}-{Win32,x64}-QuadooFormDesignerTests.log
- StudioEmbedding-validation-summary.log (original full matrix)
- StudioEmbedding-final-validation-summary.log (all runtime cases)
- StudioEmbedding-focused-validation-summary.log (final integration/standalone checks)
- StudioEmbedding-SimbeyCore-{before,after}.log

After the final embedded-tree repaint guard, all four Studio suites pass 87
assertions again. Focused standalone TabPage and keyboard/Memo suites pass 74 and
109 in each configuration. Release palette suites pass 1,070 (Win32) and 1,068
(x64). Debug palette runs fail their strict USER count check after 1,062 (Win32)
and 1,053 (x64), with GDI unchanged and USER increasing by one; these additional
resource results are preserved in the focused logs rather than hidden by reruns.
The guard skips tree HWND operations only when no internal tree exists; the standalone
path continues to use its own tree. Interactive Release x64 checks also verify
the integrated layout, Form menu, Code/Design switching, shared status bar and
Toolbox visibility. The final visual check confirms initial Pointer visibility,
expanded root children and a clean maximized repaint. The verification window
was closed without changing the example definitions or scripts.

The 2026-10-18 Application scaffolding update removes automatic Main.quadoo
creation and its default-script project entry. Updated integration checks verify
its absence on disk and in the project, the MainForm-only initial tree, saving
and reopening, and legacy project behavior with an explicit test source.
All four VS2008 builds completed without errors/warnings, and each Debug/Release,
Win32/x64 integration suite passed 113 assertions. Logs are
`target/ApplicationScaffold-<configuration>-<platform>-{build,tests}.log`.
SimbeyCore binaries remain unchanged.

The follow-up Default-form update marks MainForm.quadoo as Default in new
Application projects. Integration checks verify the JSON flag, initial loaded
state, and persistence after saving and reopening. All four VS2008
Debug/Release, Win32/x64 builds completed without errors/warnings; each
integration suite passed 114 assertions. Logs are
`target/ApplicationDefault-<configuration>-<platform>-{build,tests}.log`.
SimbeyCore binaries remain unchanged. The plan now makes the Default form
authoritative for future private startup-code generation.

## Remaining work

Stage 3 source generation, handler navigation, managed-region validation and
coordinated edits/saves are implemented; see [SourceSynchronization.md](SourceSynchronization.md).
Script construction/binding, application
message pumping, Run/Debug and packaging belong to later stages. Export is a copy
operation; coordinated source/definition Save As and file rename are deferred.
The left tree/Toolbox divider position is session-local. Saved tree outlines are
eagerly parsed; lazy branch parsing can be added if project sizes justify it.

Actual Windows XP execution, physical monitor/DPI changes, live IME composition
and power-loss recovery were not performed in this pass. Existing late native
USER/designer GDI count intermittency remains tracked in NativeValidation.md and
DesignerValidation.md. Strict resource assertions and warm-up limits are unchanged.

## Shared design-time packages (2026-10-18)

Form > Packages is always enabled at the Studio main-window level, including
Code view and projects without an active designer. Both hosts compile the shared
code and dialog resources from platform/QuadooForm/Packages directly into their
EXEs and share the per-user DesignTime store. Project-wide usage checks include
unsaved/inactive designers and unopened form/frame definitions; per-type leases
protect other running hosts. The focused Studio package suite passes 18 assertions
in all four configurations. See [package validation](../../platform/QuadooForm/Packages/Validation.md)
for the complete results and the existing clipboard snapshot limitation.

## Source synchronization (2026-10-19)

Studio generates property variables for named visual and nonvisual components,
managed events interfaces and explicitly requested user handler implementations.
Identifier validation uses the new QuadooIsValidIdentifier API in Studio only.
Source preflight covers the shared designer mutation paths, inactive source
buffers, rollback, breakpoints and three-file save/recovery. New units begin
synchronized. Stage 4A subsequently added runtime scripting and non-debug Run;
Debug and Compile remain deferred.
See [source synchronization](SourceSynchronization.md) and [stage 3 validation](Stage3Validation.md).

## First-form naming (2026-10-24)

New Application projects save the empty project structure, open it, and invoke
the existing NewFormUnit method. The Add Form Source dialog, source generation,
definition generation, relative-path handling, project registration, save and
designer activation are shared with ordinary Form-menu creation. There is no
production MainForm filename assumption. NewFormUnit assigns Default and the
startup unit ID when creating a form in a project without a Default source.

Canceling the first-form dialog leaves the empty project open. A frame does not
become Default. A later first form becomes Default, while additional forms keep
the existing Default selection. A failed save removes the uncommitted source,
definition and project entry without leaving a startup-form reference behind.

VS2008 Win32 Debug/Release Studio and integration-test builds complete with zero
errors and warnings. Focused suites pass in both configurations: creation UI
(44 assertions), settings (51), menus (45), packages (19), source synchronization
(106), Application build/run (150), TreeView (13) and ImageList (14). Existing
fixtures explicitly choose MainForm through NewFormUnit rather than relying on
project scaffolding. All SimbeyCore DLL hashes remain unchanged.

The full Debug Studio run stopped at the existing clipboard snapshot helper
(DesignerTests/ClipboardTests.h:51, 0x8007054F) after 137 assertions. Explorer owned
a file-transfer clipboard with deferred formats; the test could not materialize
all formats. The clipboard was left untouched. This is separate from the passing
creation and focused regression suites. Logs use the target/ApplicationCreation-
prefix; the dedicated UI test is --creation-only.
