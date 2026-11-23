# Application source synchronization — stage 3

Implemented 2026-10-19; extended for runtime construction on 2026-10-20.
Application Run is available in stage 4A. Debug and Compile remain deferred.
Native Preview continues to suppress script handlers.

## Using the feature

New Form and Frame units start with synchronized declarations. Opening an older
unit in Design view prepares the same declarations in its actual source buffer;
the resulting changes are saved with the unit. There is no generated Main.quadoo
project entry. MainForm.quadoo remains Default in a new Application.

Each named child control and nonvisual component receives a property variable:

```cpp
interface IMainForm
{
    // Designer-owned events region
    virtual function AcceptButton_Click (oArgs) = 0;
};

class MainForm : extern(Form), IMainForm
{
    // Designer-owned members region
    property __QuadooUnitId { get { return "{unit-guid}"; } }
    property var AcceptButton;
    property var Timer1;

    // User-owned implementation
    virtual function AcceptButton_Click (oArgs)
    {
        return true;
    }
};
```

The real generated regions include unit-GUID ownership markers and component
GUID comments. Class names continue to match source-file stems; the events
interface adds I. Stage 3 generated the readonly __QuadooUnitId property as an
explicit catalog key. The refined stage-4 plan instead resolves resources from
the script class name already supplied to IExternalClassLoader; the property can
remain as unit metadata without being required for runtime lookup. Component
properties can be populated through SetProperty. No runtime construction or
Form.Create constructor was generated in stage 3. Stage 4A now generates a
parameterless Form constructor, or a Frame constructor taking Owner, when a
constructor is missing. An existing user constructor is preserved and must call
Create itself. Frames use extern(Frame) and Frame = Frame.Create(this, Owner).

The managed members region now also contains __QuadooRoot, whose getter/setter
access Form or Frame. PostBind uses this property to install the distinct live
wrapper before OnCreate; the external definition loader itself only implements
Create. These names are reserved against control/member collisions. New scaffolds
include #pragma once, and the conservative scanner accepts that directive.

QuadooScript's existing grammar uses virtual function, including in interfaces.
The earlier virtual bool notation in the proposal was illustrative; generated
stubs use the supported syntax and return true. The runtime meaning of that
Boolean remains a stage-4 decision, separate from writable Cancel/Handled args.

Event names remain editable inline. Select an Events row and use its context
menu, Ctrl+Enter, or Form > Create/Open Event Handler. An empty binding proposes
ComponentName_EventName, omitting an initial On from the event name. A compatible
method is opened; otherwise an implementation is inserted outside managed
regions and Studio navigates to it. Incompatible declarations stop the action.

Pasting preserves event binding names. A missing method is declared in the
destination interface and reported in the status bar. Create/Open Handler can
create it explicitly; paste never copies another class's handler body.

Control renames update owned property declarations and GUID mappings. They do
not rename handler methods or rewrite references in user code. Deleting a
container removes its owned descendant declarations. Unused owned interface
declarations are removed; user methods and user-authored interface declarations
are preserved. Built-in, independently registered and package-backed types use
the same path.

## Source ownership and identifier validation

Only Studio calls QuadooIsValidIdentifier. Its updated implementation checks for
an identifier followed by end of input. Studio delegates identifier syntax,
keywords and length limits entirely to that API, without a separate character
allowlist or length limit. Empty names and invalid identifiers are rejected
before a designer mutation. Existing invalid definitions remain viewable for
repair; standalone Designer naming is unchanged.

FormSourceSync.cpp scans comments, quoted text and balanced scopes. It locates
the associated class/interface in the in-memory source, handles constructor
initializer lists and property/variable declarations, and rejects conflicts or
ambiguous source. It does not execute include directives or use disk definition
lookup as a substitute for unsaved source. Macro/conditional declarations and
incomplete scopes require manual correction before structural synchronization.

Owned region contents must match their recorded baseline. Manual changes or
source Undo that diverge from the design are diagnosed rather than overwritten.
Form > Synchronize Script Declarations explicitly regenerates intact, safely
located owned regions from the current definition. Missing/ambiguous boundaries
or conflicting user declarations can still require manual repair. Layout-only
position/size and ordinary property editing remain available without source
rewriting.

Source input/output is bounded to 8 Mi UTF-16 characters. Identifier candidates
use the parser's own limits.

## Transactions, documents and saving

The designer core has an optional native source site, separate from its published
runtime ABI. Shared document operations prepare source changes before addition,
deletion, Name/event edits or snapshot replacement. This includes palette,
nonvisual tray, inspector, cut and object/array paste paths. Standalone editing
has no source site and no QuadooScript dependency.

Prepared edits use CodeEdit's grouped operations at descending offsets. A native
failure rolls the source text back; successful publication updates the unit's
metadata without further allocation. The source owner and its view are retained,
including inactive buffers. Breakpoint line changes are applied to the owning
file instead of whichever editor happens to be visible. Reentrant source changes
and project close/view commands are rejected while an edit is pending.

designer.sourceSync stores version 1, component-GUID/member mappings and exact
managed-region baselines. These fields belong only to project metadata. Native
form/frame definition JSON is unchanged. Ordinary project saves use the last
saved mappings and breakpoint positions for units with pending generated changes;
they cannot publish unsaved generated mappings or line shifts by themselves.

A synchronized unit save stages source, definition and project metadata, keeps
backups, and writes a version-2 recovery journal beside the project. All three
replacements must succeed before dirty snapshots are cleared. Failures attempt
rollback and retain evidence if recovery is blocked. Reopen validates journal
paths against the project's explicit associations before recovery. Existing
version-1 paired unit recovery is retained. External source, definition and
project content conflicts stop the coordinated save.

This is ordinary failure/restart recovery, not a claim of power-loss atomicity.
Malformed journals or missing backups require manual recovery. A complete
designer/source Undo history is also deferred. Grouped source Undo/Redo can
diverge from the design; subsequent structural edits/saves validate the regions.
An aborted text edit can remain in CodeEdit's Redo history, subject to those same
checks and explicit reconciliation. If an edit reports failure after writing,
rollback verifies the original text, attempts Undo, and falls back to restoring
the retained source snapshot. If restoration also fails, the original remains
owned by the unit and it stays dirty; Synchronize Script Declarations retries
restoration before reconciliation. Normal source saves are blocked until that
recovery completes.

## Source organization and dependency policy

- FormSourceSync.h/.cpp: conservative scanner, identifiers, ownership, edit plans,
  member/event generation and deterministic source failure checkpoints.
- StudioSource.cpp: document ownership, grouped edits/rollback, breakpoints,
  handler navigation and project-save snapshots.
- FormProjectFile.*: optional designer source site and coordinated recovery.
- ApplicationProject.cpp: synchronized Form/Frame scaffolding.
- Shared designer Document/EditDefinitions/Inspector/Hosting/Surface: optional
  preflight hooks and event actions; no VM/parser calls.
- IntegrationTests/SourceSyncTests.h: scanner, compiler/VM contract and native
  Studio regressions. Only the test harness adds a QuadooVM import dependency.

No published QuadooForm IID, vtable, export or JSON schema changed. Stage 3 added
no built-in control, new DLL, runtime adapter or application host. VS2008 and XP API
compatibility are retained. The user-provided parser export was rebuilt in all
four configurations; no handwritten QuadooScript source was changed. SimbeyCore
was reused without rebuilding it.

The standalone parser project relies on solution-provided link inputs. This pass
built it directly against the existing SimbeyCore import library, using its local
tokenizer tables. Its generated tokenizer files are shared by configurations:
regenerate once and build parser configurations sequentially. Other affected
projects continue to use tests/desktop/QuadooStudio/build-studio.cmd.

Validation results are recorded in Stage3Validation.md.
