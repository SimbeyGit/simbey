# QuadooForm — Architecture and Implementation Specification

**Status:** Native stages 1, 2, and 4 implemented; remaining sections retain the wider roadmap  
**Companion:** `Published/QuadooForm.h` (active v2); `QuadooForm_v2.h` forwards to it  
**Target:** Native Win32 C++, QuadooScript integration, JSON-defined forms and frames  
**Date:** 2026-10-13

## Current native contract (2026-10-14)

The implemented native contracts and scope are documented in [NativeRuntime.md](NativeRuntime.md). That document and the active published header govern the current runtime; later design questions and milestones below remain roadmap material.

- Review-sequence stages 1, 2, and 4 are delivered: native component/factory/property/event infrastructure, Form/Frame/Panel/ScrollingPanel/Label/Button/TextBox, JSON loading/serialization, layout, scrolling, DPI handling, and a native harness.
- Frames are always instantiated and registered at runtime. No Form or Frame definition child array contains a Frame or FrameInstance. Serialization omits attached frame subtrees. There is no core frame script-object factory or declarative frame-resource resolver.
- QuadooScript and Quadoo Studio integration are deferred. The agreed future binder uses Form.Create(this), GetInterface, ResolveMethod, and a retained interface/ULONG ordinal delegate. The designer generates named-control members at design time; runtime PostBind populates them.
- The GUI container is IQuadooControlContainer. Persistent GUIDs are scoped to a Form/Frame definition instance; canonical IUnknown is live object identity.
- Native construction contexts, completion participants, factory lifetime checks, diagnostic reports, and serializer access are explicit in the active header. A binder is optional for native definitions without named runtime callbacks. PostBind runs once per completed root before OnCreate.
- Native Debug and Release builds use VS2008/SDK 6.0A. DPI awareness is host policy, and newer DPI functions are resolved dynamically. Physical monitor moves and older operating systems remain untested.

## 1. Purpose and scope

QuadooForm is a reusable, interface-based Win32 GUI framework implemented principally in `QuadooForm.dll`. It supplies standard-control wrappers, lightweight painted controls, nested containers, reusable frames, form lifecycle management, JSON loading/serialization, property and event metadata, DPI-aware layout, and a shared runtime/design-time component model. QuadooScript is the primary consumer, but the DLL must remain usable from native C++ and a standalone test harness without loading a script VM.

A later visual designer will first run standalone, then be embedded as an inline, scrollable document surface in Quadoo Studio (not as a separate full-size floating form). Runtime hosting and debugging will use separate executables sharing core hosting code. This specification defines architecture and staged delivery, not a mandate to implement all features at once.

**Non-goals for initial releases:** visual form inheritance, changing a control's parent after construction, STL-dependent public ABI, COM registration, a new scripting VM or debugger protocol, script-painted custom controls, complex modern constraint/flex layout, and embedding the designer UI itself in the core DLL.

## 2. Approved architectural decisions

The following decisions were explicitly settled and should not be silently changed by Codex:

1. `IQuadooEventsBinder::BindEvent` receives **only the callback name** and returns the wrapped delegate. The designer writes callback names such as `MyButton_OnClick`; runtime resolves them against the owning form/frame script instance. Framework lifecycle events use the same mechanism.
2. Event resolution is **eager** during form/frame construction, before application initialization callbacks. Public events use **typed, mutable** event-argument objects, supporting cancellation and results. Custom controls register event descriptors.
3. Forms and frames have independent JSON definitions, distinguished by `type`. Frames can only be created as child windows attached to an existing container. **Scripted frame instances have independent script objects and binders; native C++ use needs no script object. Frames are runtime attachments and never definition children.**
4. All child controls are constructed externally with their parent determined at creation. The native child window is attached during creation. **Parentage is immutable** and inspectable, never settable. Containers register already-created controls; they do **not** expose `CreateControl()`.
5. Runtime code can create and destroy controls dynamically. Designer cut/copy/paste between containers means serialization and **new-object** creation, not HWND reparenting.
6. Names are editable and unique throughout each form/frame definition (not merely among siblings); each separately instantiated frame has its own namespace. Controls also have immutable internal GUID identities, **serialized into JSON** so IDs survive save/load. Copy/duplicate creates fresh GUIDs.
7. Containers support direct-child enumeration and recursive name lookup **within the same form/frame namespace**; frame instances are namespace boundaries.
8. Containers publicly expose validated `RegisterControl`/`UnregisterControl`. Registration takes an ownership reference. Unregistration is not a substitute for reparenting.
9. `Close()` destroys the form HWND and descendants after any cancellable close query; `HideForm()` preserves the HWND. A referenced destroyed component remains a valid interface object; HWND-dependent operations report a destroyed-state error.
10. Property changes are observable, including batched updates; distinguish stored-property changes from effective-layout changes.
11. Scrolling panels use a dedicated **content HWND** inside the viewport HWND. Scroll offsets must not rewrite serialized child bounds.
12. Initial `AutoSize` applies to labels and selected containers. Support VCL-like Align and Anchors, plus Padding and Margins.
13. Absolute geometry is serialized in **96-DPI logical units**, independent of monitor DPI and designer zoom. Forms must be DPI-aware.
14. Visual form inheritance is deferred; reusable frames provide initial composition.

## 3. Source of truth and compatibility

`Published/QuadooForm.h` is the reviewed native v2 contract. It intentionally changes interface layouts and IIDs from the original, retained as `QuadooForm_v1.h`. `QuadooForm_v2.h` is a forwarding include. Existing custom strings/containers, JSON interfaces, reference counting, HRESULT patterns, and the VC9/SDK 6.0A toolchain were verified in the repository. See the architecture review for evidence and NativeRuntime.md for the implemented contracts. No STL, C++17, COM registration, or QuadooScript VM dependency is introduced.

## 4. Layering and module boundaries

- **QuadooForm.dll (runtime core):** component interfaces, registry/factories, property/event descriptors, native wrappers, containers, Win32 notification routing, layout, DPI conversion, JSON services, form/frame creation, and designer-facing services that do not require a designer UI.
- **QuadooScript adapter (outside core DLL):** external class resolution, script object creation, callback lookup and delegates, conversion of typed event arguments to script-visible values, reference/lifetime coordination.
- **Standalone test harness:** runs with and without QuadooScript; loads JSON, displays actual windows, tests events and lifecycle.
- **Standalone visual designer:** toolbox, property inspector, hierarchy view, selection overlay, drag/drop, undo/redo, serialization. Hosts real native controls when practical. Designed as a reusable child-window editor.
- **Quadoo Studio:** embeds that designer as a scrollable document tab; owns IDE integration, source navigation and persistence.
- **Application hosts:** release and debug executables; own process initialization, script runtime, message loop and debugger attachment, not the component model.

Avoid cyclic dependencies: core QuadooForm must not depend on the QuadooScript VM or Quadoo Studio.

## 5. Public interface model

The v2 header proposes these families (all COM-style `IUnknown`, without requiring COM registration):

| Area | Interfaces | Responsibility |
| --- | --- | --- |
| Identity | `IQuadooComponent` | GUID, name, type, properties, lifecycle, destroy |
| Visual | `IQuadooControl`, `IQuadooStaticControl`, `IQuadooWinControl` | logical/effective bounds, visibility, painting, HWND/focus/tab |
| Hierarchy | `IQuadooControlContainer`, `IQuadooPanel`, `IQuadooScrollingPanel`, `IQuadooScrollable`, `IQuadooChildWindowHost` | registration, children, layout, viewport/content HWND |
| Roots | `IQuadooForm`, `IQuadooFrame` | show/hide/modal/close, independently bound reusable frame |
| Properties | `IQuadooProperties`, `IQuadooPropertyDescriptor(s)`, `IQuadooPropertyObserver`, `IQuadooPropertyNotifications` | values, metadata, observation |
| Events | `IQuadooEventArgs`, `IQuadooEventDescriptor(s)`, `IQuadooDelegate`, `IQuadooEventsBinder`, `IQuadooMessageHandler` | named callbacks, mutable logical arguments, optional raw messages |
| Creation | `IQuadooControlFactory`, `IQuadooControlRegistry`, `IQuadooConstructionContext`, `IQuadooConstructionParticipant` | external construction, type registration, script-frame adapter |
| Persistence | `IQuadooFormSerializer` and existing JSON interfaces | serialized hierarchy |

Optional capabilities use `QueryInterface` rather than multiple `IUnknown` inheritance diamonds. A container's logical parent and its Win32 child-host HWND are different concepts; for a scrolling panel, the child-host HWND is its dedicated content HWND.

### 5.1 Identity, names, and lookups

- Each component has a GUID assigned at construction or restored from JSON; it cannot change during the object's lifetime.
- `Name` is mutable and validated atomically against the owning form/frame namespace. Renaming updates lookup indexes and notifies observers; duplicate names fail without modifying state.
- `GetChild(index)` enumerates direct children in deterministic order. `FindControl(name)` recursively searches that root namespace but does not traverse nested frame-instance internals.
- The designer may use GUIDs to preserve selection, undo history, and references when a control is renamed. A copied component tree receives new IDs for **every** copied component; intra-tree references must be remapped.
- Persistent IDs are scoped by their Form/Frame root instance, not global registry keys. Multiple Frame instances reuse their definition GUIDs. Canonical IUnknown identifies each live object; parent namespace ID validation does not cross Frame boundaries.

### 5.2 Ownership, state, and destruction

- Containers own an `AddRef` reference to each registered child. Callers own returned interface references and `Release` them.
- A child holds a non-owning logical-parent pointer, or another proven cycle-free mechanism. `GetParent` returns an AddRef'd interface if the parent is alive; specify behavior after parent destruction.
- Lifecycle: `Constructing -> Binding -> Initializing -> Ready -> Destroying -> Destroyed`; validation/binding failure rolls back before any application initialization callbacks. A failure inside an initialization callback aborts the remaining pipeline and unwinds the subtree.
- `Destroy()` is idempotent; if already destroyed, return `S_FALSE` (subject to repository HRESULT conventions). It destroys any HWND/subtree, unregisters children, releases event bindings, and preserves interface-object memory until the final `Release`.
- Identity and state queries remain possible after HWND destruction; HWND-dependent calls fail predictably. Do not reuse stale HWNDs.
- Event callbacks may synchronously close the form or destroy the sender. Dispatchers must hold temporary strong references and re-check state after returning.
- Script-to-form and delegate-to-script references can cycle. Explicitly unbind/release delegates on destruction and verify no other cycles remain.
- UI-affecting methods are owning-UI-thread-affine. Define a consistent wrong-thread HRESULT; do not silently marshal cross-thread calls.

### 5.3 Creation and registration contract

The external factory receives a construction context, intended logical parent, and JSON definition. The factory validates the request, creates the component and HWND (if applicable) with the parent's **child-host HWND**, registers it using public `IQuadooControlContainer::RegisterControl`, completes initialization and returns the interface. A lightweight painted control skips HWND creation. Containers do **not** construct children or offer `CreateControl`.

`RegisterControl` must validate logical parent, name/ID constraints, state, duplicate registration, and appropriate native parentage; then take its owning reference and insert in child order and namespace indexes. `UnregisterControl` removes indexes and ownership but **must never change parentage**. Define whether unregistration is allowed on a still-live HWND or requires prior destruction; the preferred policy is to reject unregistration of live controls except as part of controlled destruction/rollback.

**Atomicity requirement:** If creation, registration, property application, child creation, or eager event binding fails, unwind in reverse order; no orphan HWND, partial namespace entry, or leaked reference may remain. Since `CreateWindowEx` delivers synchronous messages, defer script-visible callbacks until the whole hierarchy is bound and ready. Ensure the chosen creation/registration sequence cannot expose half-initialized objects. Consider a reservation/commit mechanism internal to the factory if name collisions must be rejected before HWND creation.

### 5.4 Factory registry and custom controls

A factory advertises a stable type name, immutable property descriptors, immutable event descriptors, and a create method. The registry resolves built-in and third-party factories. Registration/unregistration should reject duplicate type names and unloading factories with live instances unless lifetime safety is guaranteed. Custom controls can be HWND-backed, lightweight painted, or containers; no custom control requires QuadooScript linkage.

**Initial built-ins:** Button, TextBox, CheckBox, RadioButton, GroupBox, ComboBox, ListBox, ListView, TabControl, Label (painted), Panel, ScrollingPanel, Form, Frame. For TabControl, decide whether pages are explicit container components (`TabPage`) and how native `SysTabControl32` pages are hosted; avoid designing it as merely a property-only tab strip.

### 5.5 Property metadata and values

Metadata is immutable and shared by component type. Instance values are accessed through `IQuadooProperties`. Descriptors supply name, display name, category, description, type, flags, defaults, and enum values. Indexes are efficient within one descriptor collection, but **property names** are stable across versions and serialization. Event callback names may be exposed as event-type properties for the inspector while JSON keeps events in a separate object.

Property categories: serializable design/runtime properties; runtime-only derived properties (HWND, focus); designer-only metadata (selection/lock) stored separately when necessary. Types should accommodate int32/int64, Boolean, `RSTRING`, enum/flags, float, color, icon, point/size/rect, font, object reference and collection. Reconcile complex values with the actual JSON library instead of assuming `IJSONValue` exists.

Property observer notifications distinguish stored changes from **effective bounds** changes. `BeginUpdate/EndUpdate` nesting coalesces layout and notifications, but does not suppress essential Win32 processing. Define observer lifetime, safe removal during callback, ordering, reentrancy, and how notifications are delivered for destruction or renamed components. Observer references can themselves create cycles; remove observers or use weak subscriptions where necessary.

### 5.6 Events and script binding

The designer writes a handler name (for example `MyButton_OnClick`) into JSON. The owning form/frame binder resolves **only that name** through `BindEvent(name, &delegate)`; no sender or event descriptor is passed to the binder. Multiple controls may bind to the same handler name. Resolve all nonempty callback names eagerly before application `OnCreate`. A missing or invalid handler must follow a defined failure policy (recommend fail construction for a named but unresolved handler; an empty callback name is unbound).

A delegate's `Invoke(IQuadooEventArgs*)` receives the event name, sender, and typed named arguments. Descriptors specify argument type, mutability, and event flags. Mutable arguments support `Cancel`, `Handled`, or `Result`; enforce writable flags and correct types. The adapter maps these to QuadooScript's actual method invocation conventions. `OnCloseQuery` must be able to cancel destruction; native notification handlers may require synchronous result propagation. Do not expose raw `WPARAM/LPARAM` to ordinary script callbacks. Native custom controls may optionally implement `IQuadooMessageHandler` for low-level Win32 processing.

`PostBind(control)` runs once for the completed Form/Frame root after eager binding and initial layout, before application OnCreate. The future adapter populates designer-generated named-control members there.

**Recommended lifecycle event ordering (needs ABI review):** finish component hierarchy -> bind all callbacks -> `OnCreate` for nested frames/children in a documented order -> root `OnCreate` -> Ready -> visibility events. On close: cancellable `OnCloseQuery` -> accepted `OnClose` -> `OnDestroy` while delegates are callable -> HWND/subtree destruction -> detach delegates -> Destroyed. Define reentrant close and exception/error handling.

### 5.7 Forms and frames

A form is a top-level window (possibly with an owner); a frame is always a child attached to a container. Each is defined in separate versioned JSON and is a root namespace and event-binding scope. A frame instance has its own script object and binder, created through a runtime-supplied adapter; QuadooForm.dll must not construct VM objects directly. Nested frames are added through runtime creation against live containers. Declarative frame references and their cycle/resource resolution are not part of the definition schema. Destroying a frame must not destroy its parent; destroying a parent destroys all descendant frames.

`ShowForm`/`HideForm` preserve or change visibility. `Close` initiates cancellable destruction; an interface can survive destruction but must not recreate the same HWND implicitly. `ShowModal` requires a well-defined nested modal loop, owner disable/reenable, close result, and behavior for errors/reentrancy. The **host** owns the primary process message loop; the DLL may provide message preprocessing and modal-loop assistance.

**Script integration contract:** the script class creates its native form through Form.Create(this). The adapter retrieves its declared event interface with IQuadooObject::GetInterface, resolves names with IQuadooInterface::ResolveMethod, and retains that interface with the resolved ULONG ordinal in each delegate. No QuadooScript change is required. Runtime frames use separately supplied script instances/interfaces/binders.

## 6. Layout and DPI

- Store requested bounds (`Left`, `Top`, `Width`, `Height`) in 96-DPI logical units. Compute effective bounds separately; docking must not overwrite requested geometry.
- Support `Align=None/Top/Bottom/Left/Right/Client`, VCL-style anchors, container Padding and child Margins. Define deterministic dock order from child order (and designer reordering); define conflict rules for multiple `Client` children.
- Parent padding shrinks the available layout rectangle; margins shrink each child's allocation. Anchors operate for `Align=None`. Min/max sizes constrain output. AutoSize initially supports labels and selected containers, with explicit precedence when combined with docking.
- Scrolling panel has viewport HWND and content HWND; content dimensions derive from child bounds and padding. Scroll bars, wheel input, clipping and nested scroll panels operate independently. Scrolling changes viewport offset, not stored child bounds.
- Separate logical layout from physical HWND coordinates. Convert at the boundary using the current DPI. Handle `WM_DPICHANGED`, font/metric recreation and monitor moves as supported by OS; gracefully fall back on older Windows versions. Designer zoom is a separate transform and is never serialized as geometry.
- Layout engine should be independently testable without actual HWNDs where possible. Batch changes to avoid unnecessary repaint/layout storms.

## 7. JSON persistence proposal

Use a versioned, recursive schema. This is an **illustrative schema**, subject to compatibility with existing JSON interfaces and loader conventions:

```json
{
  "schemaVersion": 1,
  "type": "Form",
  "class": "CMyForm",
  "id": "0dcb03c4-0f5b-43df-94d9-9f6f824d6e5d",
  "name": "MainForm",
  "properties": {
    "Caption": "My Quadoo Application",
    "Width": 800,
    "Height": 600,
    "Padding": { "Left": 12, "Top": 12, "Right": 12, "Bottom": 12 }
  },
  "events": { "OnCreate": "MainForm_OnCreate" },
  "children": [
    {
      "type": "Button",
      "id": "d15c87cb-6c6c-45f1-b26f-c7e995b11ad2",
      "name": "btnOK",
      "properties": { "Caption": "OK", "Left": 20, "Top": 60, "Width": 100, "Height": 28 },
      "events": { "OnClick": "MyButton_OnClick" }
    }
  ]
}
```

A frame definition uses `"type": "Frame"` as its root and has its own class/name/ID/properties/events/children. It is instantiated only at runtime against an existing container. A frame is never a child entry in another definition. Runtime instance names/placement can be supplied by cloning and overriding the root definition before creation.

Serializer requirements: deterministic child order; names unique within root namespace; persistent GUIDs; new GUIDs for pasted/duplicated components; explicit schema version; default-value omission only when round-trip semantics are preserved; error reports with JSON path/component; policy for unknown control types/properties; and lossless preservation of unknown fields in designer editing when practical. Frame instances reuse persistent definition GUIDs within their independent namespaces; canonical IUnknown provides live identity.

## 8. Visual designer architecture

The designer is **not** part of the QuadooForm runtime DLL's main UI. QuadooForm exposes component metadata, factories, serialization and design context; a standalone editor hosts the design surface and is later embedded into Quadoo Studio as a child-window document.

The editor uses a scrollable/zoomable viewport, not a full-size floating form. It should use real Win32 controls when practical, with a selection/input overlay and resize grips; use placeholders for controls that cannot initialize safely in design mode. Provide component palette, hierarchy tree, property/event inspector generated from descriptors, direct manipulation, multi-selection, grid/snap/alignment guides, clipboard via serialize/recreate, and undo/redo via commands or snapshots. A control cannot be reparented: moving across containers is a reconstructive operation with new identity.

Design-time mode is a context (`Runtime`, `Design`, `Preview`) rather than a mutable per-control parent or ad hoc window mode. Preview permits real interaction but should not execute arbitrary application script handlers by default. Designer zoom and DPI are independent. Changes to properties/geometry should be observed and reflected without hardcoding each control type in the inspector.

## 9. Win32 control and notification implementation

Initially wrap standard classes/styles: `BUTTON` (push, checkbox, radio, group box), `EDIT`, `COMBOBOX`, `LISTBOX`, `SysListView32`, `SysTabControl32`; label is painted and need not use `STATIC`. Common Controls v6 visual styles should be used where available. QuadooForm translates `WM_COMMAND`/`WM_NOTIFY` into logical events and routes notifications to the owning wrapper; low-level `LRESULT` requirements must be preserved. Implement focus/tab navigation, keyboard accelerators where relevant, and parent/control notifications without relying on the script VM.

Do not grow the public `IQuadooWinControl` interface for each Win32 message. Internal C++ message handlers or optional `IQuadooMessageHandler` provide raw interception. Carefully handle child controls whose notifications are delivered to a content HWND versus a logical container; route to the appropriate owner regardless of physical HWND hierarchy.

## 10. Error handling, validation and diagnostics

Use `HRESULT` for failure-capable public operations and clear output pointers on failure. Document errors for duplicate names/IDs, unknown type, invalid JSON, unsupported property, wrong type, destroyed object, wrong UI thread, missing handler, recursive frame reference, invalid parent, duplicate registration, and failed HWND creation. Do not swallow script exceptions; propagate through the existing VM/error conventions. Emit useful diagnostics including JSON path, component name/type, event handler name, and native error when applicable.

Validate that registration, event binding and deserialization are exception-safe and rollback-safe. Avoid calling application script during partially constructed states. Make failure tests first-class, not an afterthought.

## 11. Implementation plan and acceptance tests

**Milestone 0 — Repository reconnaissance and ABI review.** Inspect original and proposed headers, `RSTRING`, JSON APIs, external class loader, `IQuadooObject`, compiler/SDK/build system, window and event conventions. Produce an ABI reconciliation note and revised header proposal. **Do not implement until review.** Acceptance: every `QF-VERIFY` item has evidence and proposed resolution; no unsupported types remain.

**Milestone 1 — Core components and factories.** Implement identity/state, refcount/lifetime, property descriptors and values, registry, construction context, validation, container child collection and namespace index; add minimal native test executable. Acceptance: unit tests for unique names, rename rollback, persistent IDs, direct/recursive lookup, reference counts, destruction, factory errors, batched notifications.

**Milestone 2 — Basic Win32 wrappers.** Implement forms/panels, painted labels, buttons, edit, checkbox/radio/group, combo/list, and message routing. Acceptance: real visible windows, focus/tab order, native notification-to-logical-event tests, no script runtime required.

**Milestone 3 — JSON and layout.** Load/save versioned forms, recursive children, reusable frame definitions, 96-DPI geometry, Align/Anchors/Padding/Margins, initial AutoSize. Acceptance: deterministic JSON round trip, nested panels/frames, duplicate-name/unknown-type failures, layout tests at 96/144/192 DPI.

**Milestone 4 — Scrolling and comprehensive harness.** Implement content HWND, auto-scroll, nested scroll panels, clipping, wheel input, DPI transitions; add ListView and TabControl including page containers. Acceptance: no serialized-geometry drift on scrolling/DPI change; test harness can load and interact with sample JSON.

**Milestone 5 — QuadooScript integration.** Adapt external class loader, eager named binding, frame script-object factory, mutable event arguments, lifecycle events, unbinding and cycle prevention. Acceptance: script-created form, two instances of same frame with isolated state, shared named handler, cancellable close, destroy-during-event, missing-handler errors.

**Milestone 6 — Standalone designer.** Component palette, inline scrollable surface, hierarchy, property/events grid, drag/resize, save/load, clipboard reconstructive paste, undo/redo. Acceptance: design and round-trip a form with nested panels, frames and events; paste creates new GUIDs and never reparents.

**Milestone 7 — Quadoo Studio integration.** Embed reusable design surface in editor document, link event handlers to source, preserve IDE state. Acceptance: open/edit/save/preview form and frame JSON inside Studio.

**Milestone 8 — Release/debug hosts.** Create shared host logic, release runner, debugger-aware runner, message loop/modal behavior. Acceptance: standalone scripted application launch, debugging, shutdown without leaked windows/references.

Each milestone must build and test using the repository's actual toolchain. Codex should report changed files, tests run, failures, design deviations, and unresolved issues; stop for approval before changing public interface contracts.

## 12. Critical open questions for Codex investigation

These are **not settled** by the conversation or v2 header and must be explicitly investigated rather than guessed:

1. Actual `RSTRING` memory ownership, reference semantics and return convention.
2. Whether `IJSONValue` exists; which JSON interfaces support nested complex values and serialization.
3. Script VM method lookup, delegate invocation, event argument object exposure and frame script-object lifetime.
4. Whether the v2 header's `IQuadooFrameObjectFactory` must return a script-instance handle in addition to the binder.
5. How a serialized GUID behaves across multiple simultaneously instantiated copies of a frame definition; design identity vs live-instance identity.
6. Exact meaning and timing of `PostBind`, including whether called per control or per root.
7. Whether `UnregisterControl` on a live component should be rejected, and how registration rollback works after native creation.
8. Missing named event-handler policy and script exception propagation.
9. Layout precedence for conflicting Align/Anchors/AutoSize/MinMax; docking order; multiple `AlignClient` children.
10. Runtime support matrix and fallback DPI strategy for older Windows targets.
11. Stable native ABI details: SDK annotations, calling conventions, `BOOL` versus C++ bool, exported names, UUID generation, supported compiler syntax.
12. Modal return semantics, owner handling, shutdown order and reentrant destruction.
13. Resource resolution for frame JSON, recursive references, and designer preservation of unknown fields.
14. Observer subscription lifetime, potential cycles, batching order and reentrancy.
15. Native TabControl page-host model, group box semantics and design-time event interception.

These issues are legitimate review tasks, **not permission to silently override approved decisions**.

## 13. First Codex assignment

> Review `QuadooForm.h` (original), `QuadooForm_v2.h` and `QuadooFormArchitecture.md` together. Inspect the existing QuadooForm project and relevant QuadooScript, JSON, `RSTRING`, windowing and build infrastructure. Produce an evidence-based interface/ABI reconciliation report, enumerate unresolved questions, and propose precise changes to the v2 header where necessary. Preserve the approved architectural decisions, especially external creation plus container registration, immutable parentage, independent frame script objects, eager name-only event binding, 96-DPI JSON geometry, and persistent design IDs. **Do not implement the runtime or alter public interfaces yet.** Wait for architectural approval before Milestone 1.

## 14. Acceptance criteria for the architecture review

The design is ready to freeze when: (a) all proposed public types exist and compile in the actual project toolchain; (b) every interface has documented ownership, threading and destruction behavior; (c) script-object/frame creation and event argument marshalling have a concrete adapter contract; (d) GUID persistence and frame-instance identity are unambiguous; (e) construction/registration and rollback are testable; (f) JSON schema and layout rules are precise enough to implement without guesses; and (g) no approved decision has been changed without explicit review.
