# Nonvisual components, file dialogs and designer fallback

This milestone extends schema version 1 with an optional root `components` array.
Existing visual interfaces, IIDs, vtables and export ordinals 5–13 are unchanged.
QuadooScript and Studio have no dependencies or source changes.

## Additive native ABI

The published header adds IIDs ending in 0029–0031. Existing registrations through
IQuadooControlRegistry remain visual. Query that same registry for
IQuadooComponentRegistry to register and enumerate IQuadooComponentFactory objects.
Type names are unique across both registries. Enumeration identifies the category
without constructing an object. Neither factory interface requires a private base.

IQuadooComponentHost is exposed by Form and Frame roots. GetComponents returns an
AddRef'd IQuadooComponentCollection. Its GetAt returns nonvisual objects only;
FindComponent searches the complete visual/nonvisual namespace. GetRoot returns
the definition root, not an enclosing Frame's parent. RegisterComponent validates
ownership, metadata, lifecycle, name and GUID before committing one owning reference.
Failed interface calls and reentrant registration leave the collection unchanged.
Destroying the root during registration fails with EInvalidState before changing
the root. Independent implementations call ValidateIdentity before renaming.

Nonvisual objects expose IQuadooComponent and the separate optional
IQuadooNonvisualComponent marker/owner capability. They do not expose visual
control, layout, focus or HWND interfaces. Mandatory base metadata is Name:String,
serializable and editable. Layout properties and TabOrder are prohibited.

Collections strongly own registered objects. Components borrow their collection;
GetOwner returns an AddRef'd pointer while attached. Components unregister during
Destroy and clear the weak owner. Root teardown destroys all owned components,
clears the collection and detaches its weak root. Externally retained collection
interfaces then return EDestroyed. No component-to-root reference cycle is needed.

QuadooCreateComponent is the only new export, ordinal 14. It creates, registers,
binds, initializes and activates a single nonvisual object transactionally. Factories
return registered Constructing objects and implement the existing construction
participant protocol. Root definition loading binds every visual and nonvisual
handler, calls PostBind once on the root, initializes descendants/components before
the root's OnCreate, and activates only after all initialization succeeds. Design
and Preview suppress application callbacks. Nonempty unresolved Runtime handlers
fail construction; BindEvent remains name-only. Destroy/abort clears delegates.

IQuadooDefinitionSource optionally supplies an independent original JSON template
for lossless serialization, including third-party fields. IQuadooFileDialog exposes
native Execute. IQuadooDesignPlaceholder exposes a reason and status description.
All out interfaces are AddRef'd and all out RSTRINGs require RStrRelease. Factories,
components, mutable values and collections remain UI-thread-affine. Retained event
arguments retain their sender; factory live counts include destroyed objects with
outstanding references. An extension must account for its descriptors/arguments
when deciding whether its DLL can unload, as the independent test DLL demonstrates.

## JSON and designer behavior

Only Form/Frame definition roots accept `components`. `children` contains visuals.
The loader rejects the wrong category, duplicate names/GUIDs across categories,
invalid known values, and malformed structure. Separately instantiated Frames have
independent component namespaces. Runtime Frame attachments are still omitted from
their parent's serialization; serializing a Frame itself includes its components.

Components appear in a separate tray below the visual workspace and a Components
group in Objects. Tray and tree selections share the property/event inspector,
including inline string/event editing. The toolbox has separate visual/nonvisual
categories; choosing a nonvisual entry immediately adds it to the definition root,
regardless of the selected visual container. The tray uses automatic columns and
its own scrolling; its positions and area never become runtime bounds or form
scrolling content. Components have no resize handles or tab order. Selection never
executes an operation. Preview creates an independent real Form from the unsaved
definition, including its components.

In Design mode a missing factory produces a lossless unresolved representation.
A registered factory may explicitly return the new EDesignUnsupported HRESULT only
after complete rollback and without callbacks. Only that result permits typed
fallback; memory/resource errors, invalid properties and arbitrary failures remain
fatal and preserve the active designer document. Existing factories can continue
to return their original failures without being silently substituted.

Visual representations are generic design windows with dashed canvas outlines,
names/types, tree tooltips and inspector status. Their generic host is solely for
the representation; no specialized original-control interface is claimed. Nested
objects retain their identities and definitions; native layout below an unavailable
parent is not inferred, and descendants are also represented conservatively.
Nonvisual representations stay in the tray and never become visual controls.

Typed fallback uses factory descriptors. Primitive values/events and known geometry
can be edited with type/enum/flag validation. Opaque compound/reference collections
are read-only in the placeholder inspector because custom validation is unavailable.
Unresolved types expose only generic identity/geometry; original unknown properties,
events and nested/vendor data survive unchanged. Saving writes the original type,
never Placeholder, and only explicit edits modify placeholder property data. Missing
GUIDs are generated once and persisted. Reopening after registering the factory
resolves the representation with the same GUID. Runtime/Preview never substitute
representations or omit unavailable objects.

## OpenFile and SaveFile

These are two concrete factories with shared native implementation. Both expose
Name, Title, Filter, FilterIndex, InitialDir, DefaultExt, FileName and Options, plus
OnCreate, OnDestroy, OnAccept and OnCancel. Filter is VCL-style `label|pattern`
pairs; empty means all files. At most 4,096 pairs and 65,530 characters are accepted.
FilterIndex is 1-based. Invalid filters/indexes/flags
fail rather than being ignored. DefaultExt omits the leading period.

Options is a descriptor-driven flags property:

| Value | Option | Win32 behavior |
|---|---|---|
| 1 | FileMustExist | OFN_FILEMUSTEXIST |
| 2 | PathMustExist | OFN_PATHMUSTEXIST |
| 4 | OverwritePrompt | OFN_OVERWRITEPROMPT |
| 8 | NoChangeDir | OFN_NOCHANGEDIR |
| 16 | NoReadOnlyReturn | OFN_NOREADONLYRETURN |

OpenFile defaults to 11, SaveFile to 14; both use OFN_EXPLORER and hide the readonly
checkbox. Multi-select is deferred. CChooseFile already supported InitialDir. Its
new compatible Choose operation additionally exposes initial filename, DefaultExt,
FilterIndex, explicit flags, and a distinguishable HRESULT cancellation result.
Display now uses CommDlgExtendedError instead of stale GetLastError and clears its
Unicode buffers in bytes. Existing OpenSingleFile/SaveFile overloads remain intact.
No duplicate GetOpenFileName/GetSaveFileName implementation was introduced.

Execute(NULL, &accepted) resolves the definition root's HWND and then GA_ROOT;
an explicit owner must be a live window on the component's UI thread. Missing root
windows fail with EInvalidState/EDestroyed; no component-owned window is fabricated.
Execute is synchronous, requires Ready, and rejects Design mode and nested Execute.
Explicit native Execute in Preview is allowed but application callbacks are suppressed.

S_OK/TRUE means accepted; S_FALSE/FALSE means cancelled. Cancellation preserves
FileName and FilterIndex and raises OnCancel. Acceptance stages both results before
publishing them, then raises OnAccept. Failed chooser calls, invalid results,
allocation failures and callback errors preserve/restore the prior stored results.
A strong self-reference protects execution and dispatch when a callback destroys
the sender/root. Native calls after destruction fail. Selecting a path never reads,
creates or writes that file; the caller performs file I/O separately.

## Validation and examples

DesignerExamples/Dialogs.qform.json demonstrates both dialog components, Timer and event
assignments. The independently built QuadooFormTestExtension DLL supplies
ExternalService using only published interfaces and its own metadata/events.

The focused harness/designer `--components-only` mode exercises runtime binding,
absence of visual interfaces, root/Frame namespaces, mixed-category rejection,
property editing, persistence, duplicate names/GUIDs, nine registration failures,
reentrant root destruction, retained event arguments and DLL lifetime. Designer
tests cover tray/tree/property selection, Form/Frame documents, missing/unsupported
visual/nonvisual representations, nested/vendor data, typed editing, reopening to
resolve a factory, failed Preview and transactional construction errors.

Private native tests inject an optional internal chooser driver through a wrapped
construction context. This test seam is not a public dependency or third-party
requirement; production contexts always use CChooseFile. It exercises acceptance,
cancellation, errors, reentrancy, owner resolution, wrong-thread calls, invalid
results, callback failure rollback and destruction during OnAccept without showing
dialogs. Final Debug/Release counts and manual OS dialog checks are recorded
after the complete milestone below.

Windows XP execution has not been performed in this task. VC9/Win32 and
XP-compatible APIs are retained. No claim of tested XP/theme/DPI coverage should
be inferred from compilation alone.

## Timer

Timer is a separately registered concrete nonvisual factory built on the same
component base and owner collection. It exposes Name, Enabled:Boolean (true),
Interval:Int (1000 milliseconds), OnTimer, OnCreate and OnDestroy; the typed
IQuadooTimer interface provides Enabled/Interval accessors. It owns no HWND,
layout rectangle, focus state, font or graphics resources. The tray paints a clock
glyph. It works in both Form and Frame definitions and serializes under components.

Enabled=false or Interval=0 stops ticking. Positive intervals through INT_MAX are
accepted; Win32 applies its timer minimum and message coalescing. This is a UI-thread
message timer, not a worker-thread or precision scheduler. Activation waits until
all initialization, including the root OnCreate, completes. Design and Preview never
start timers. Changing Enabled or Interval restarts the countdown with a fresh
private ID; stale queued messages do not call the handler. A failed restart preserves
the old timer and rolls back the logical property. No duplicate logical event is
raised when a property is edited.

The definition root routes timer messages to optional component message handlers
through a strong snapshot of public component interfaces. A Timer callback holds
its sender/delegate alive, suppresses same-timer nested delivery during a pumped
event, and can disable/destroy itself or destroy its owner. Teardown and abort kill
the timer before releasing bindings; rollback suppresses destruction callbacks, as
for visual children. Runtime Frame timers use their own namespace/root and binder.

Timer tests cover eager binding, initialization gating, enable/disable/zero/maximum
intervals, invalid input, thread affinity, reentrancy, Frame routing, Preview
suppression, callback/root destruction, retained arguments/factory lifetime, and
32 create/tick/destroy cycles. Private start failure injection verifies activation
rollback without OnDestroy callbacks, property rollback with the old timer still
running, and absence of OS timer starts in Design/Preview. The interactive harness
`--show-components` combines OpenFile, SaveFile and Timer without QuadooScript.

## Milestone validation (2026-10-16)

VS2008 Win32 Debug and Release builds completed with zero errors and warnings.
Both configurations passed the complete public harness (7,403 assertions),
private native tests (116), and designer tests (1,750), including the unchanged
strict GDI/USER resource limits. Results are also recorded in NativeValidation.md
and DesignerValidation.md. Logs are target/Components-{Debug,Release}-{build,
native,private,designer}.log. Release ran in target/QuadooFormDesignerValidation
with the corrected SimbeyCore dependency; the normal Release dependency is still
in use by another application and was not replaced.

COM tests reuse the existing full interface-path matrix to check canonical
IUnknown, symmetric QueryInterface, one-reference increments, balanced releases,
NULL outputs and unsupported interfaces for the registry, root host, collection,
factories, both dialogs, Timer, independent service and both placeholder categories.
Injected persistence/host QueryInterface failures abort serialization instead of
silently dropping original third-party data or owned components.

Manual modern-Windows tests exercised real CChooseFile dialogs: Open accepted an
existing task-owned text file; Cancel retained the old filename; Save accepted a
new path without creating it; its subsequent Cancel retained that selected path.
The input contents were unchanged. Owner focus and enablement were restored. The
native sample's Timer kept ticking while the common dialogs ran their modal loops.
In the designer, adding Timer selected its clock glyph and tree entry, showed only
Name/Enabled/Interval and lifecycle/OnTimer events, and displayed no canvas grips.
Independent modal Preview created/closed normally without spontaneous pickers or
callbacks. A separate QA definition saved all three components under components,
without nonvisual bounds, children or native handles.

Remaining limits: Windows XP execution and physical per-monitor/theme changes
were not tested in this milestone. Runtime timers use the UI message queue and
are intentionally suppressed in Design/Preview. Opaque placeholder compound
properties remain read-only until their real factory can validate them; reopening
resolves available factories. Dialog multiselect, designer Execute actions,
QuadooScript/Studio integration and the other previously deferred controls remain
outside this milestone.
