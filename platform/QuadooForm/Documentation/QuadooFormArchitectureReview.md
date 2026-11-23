# QuadooForm architecture review

Date: 2026-10-14  
Status: Historical investigation and recommendations. Follow-up native stages 1, 2, and 4 are now implemented; see [NativeRuntime.md](NativeRuntime.md) for the active contracts. Stage 3 remains deferred.

## Recommendation

Use the v2 component model as the basis of implementation, after resolving the GUI container name collision and the remaining construction/lifecycle contracts. Begin with a small native executable exercising a real JSON-loaded form, lifetime management, notifications, and failure rollback. Prove QuadooScript binding and independent runtime frame instances early, before implementing the complete control catalog or designer.

The user's override takes precedence over the architecture proposal: frames are created and attached at runtime. A frame is never a child entry in a form definition. This review also treats frames attached to other frames as runtime operations, rather than declarative composition.

## Script-class binding contract and designer assumptions

The user clarified the integration boundary after the initial review:

- QuadooScript source and published interfaces remain unchanged in this pass.
- A script class such as `CMyForm : extern(Form), IMyForm` creates its native form with `Form.Create(this)`. During that call, the adapter calls the supplied script object's `GetInterface` for its associated event interface, such as `IMyForm`, and uses `IQuadooInterface::ResolveMethod` to fulfill eager, name-only `BindEvent` calls.
- Each event delegate retains the returned `IQuadooInterface` and its resolved `ULONG idxOrdinal`. Event dispatch calls that interface's `Invoke` with the ordinal and a single script-visible event-argument object.
- At design time, the GUI designer updates the associated script class to declare a variable, potentially a property variable, for each named control. Each Form/Frame class has its own named-control members.
- At runtime, binding populates those existing members with the corresponding control wrappers before application initialization callbacks. Source generation and synchronization belong to the designer.

The earlier recommendation to add a VM method-resolution facility is withdrawn. The user supplied the concrete existing-API protocol: `GetInterface` -> `ResolveMethod` -> ordinal-based `Invoke`. No additional VM lookup facility is needed.

## Repository evidence

| Area | Finding and evidence | Consequence |
| --- | --- | --- |
| Compiler and SDK | The existing project is a VS2008 `.vcproj`, Unicode, Win32 DLL, with exceptions disabled. Syntax checks ran with compiler 15.00.30729.01 and Windows SDK 6.0A. | Preserve VC9 compatibility, custom containers, HRESULT conventions, and Win32 APIs. |
| Platform configuration | `QuadooForm.vcproj` contains only Debug/Release Win32 configurations. The solution's x64 entries map this project to Win32 and do not build it as x64. | Start with the configured Win32 target. Actual x64 support needs project configurations and validation. |
| Existing implementation | `QuadooForm.cpp` references undefined `CQuadooForm`/`CQuadooFrame` types; its frame function queries an undefined `ppForm`. `QuadooForm.def` exports only `QuadooCreateForm`, ordinal 5. | Treat the file as a skeleton, not a working baseline. Implement the exports and DLL initialization deliberately. Preserve ordinal 5 for the form export. |
| JSON | `platform/JSON/Published/JSON.h` includes `shared/JSON/InterfaceJSON/PublishedJSON.h`. That header defines `IJSONObject`, `IJSONArray`, and `IJSONValue`, including objects, arrays, numbers, booleans, strings, GUID conversion, cloning, and serialization. | The proposed JSON value types exist. Complex properties can use the existing JSON representation. |
| Strings | `shared/library/util/RString.h:9` defines the tagged `RSTRING` handle. `RStrSet` retains managed strings; `CRString` releases them. JSON getters use `RStrSet` (`JSON.cpp:45`, `:384`). | Inputs are borrowed; stored managed inputs must be retained; returned strings carry a caller-owned reference released with `RStrRelease` or held by `CRString`. Static strings must have an adequate storage lifetime. |
| Reference counting | `shared/library/core/BaseUnknown.h:17` supplies `CBaseUnknown`, `IMP_BASE_UNKNOWN`, and interface maps. | Use these facilities for the runtime objects and optional capabilities. |
| HWND lifetime | `shared/library/window/BaseWindow.cpp:472` retains an object on native creation. Final destruction clears its HWND, calls `OnFinalDestroy`, and releases the window reference (`:453`). | Reuse this pattern so a component object can survive HWND destruction. Do not implement final destruction as unconditional `delete this`. |
| Script loader | `QuadooVM.h:482` exposes `IExternalClassLoader::LoadExternalClass(className, existingScriptObject, outNativeObject)`. `VMCore.cpp:1361` passes an already-created script object to it. | The loader supplies the `extern(Form)` native adapter. Actual form creation and event-interface acquisition take place during the script's `Form.Create(this)` call. The core DLL does not construct script instances. |
| Script invocation | `IQuadooInterface::Invoke` accepts an ordinal, `QVPARAMS`, and a result (`QuadooVM.h:559`). `VMInterface.cpp:21` invokes the selected method through `CVMObject::CallImmediate` (`VMObject.cpp:282`). | A native event-argument wrapper is supplied as one script object argument. Synchronous native events finish before returning to Win32, using the existing immediate invocation path. |
| Script lookup | `IQuadooObject::GetInterface` is declared at `QuadooVM.h:574`; `IQuadooInterface::ResolveMethod` is declared at `:562` and implemented at `VMInterface.cpp:35`. | The adapter retrieves the associated event interface once per Form/Frame creation and eagerly resolves each named callback to an ordinal without executing it. This implements the user's protocol with the existing published APIs. |
| Event-interface ownership | `VMInterface.cpp:6` retains its backing script object in the interface wrapper constructor and releases it in the destructor. | A delegate's owning reference to `IQuadooInterface` retains the target instance and its method table. Ordinals are used only with the interface instance that resolved them. |
| Script destruction | `QuadooVM/DllMain.cpp:169` implements `QVMReleaseObject`, which can schedule script destruction before the last release. | Adapter ownership must follow VM conventions, rather than treating every script reference as an ordinary interface reference with no script destruction semantics. |
| Existing binder behavior | Source search found `BindEvent`/`PostBind` declarations and the QuadooForm skeleton, but no implemented binder or `PostBind` call site. | Its timing cannot be recovered from existing implementation; it needs an explicit contract. |
| Proposal copies | The requested architecture file and `Published/QuadooFormArchitecture.md` currently have identical SHA-256 hashes. | Use the requested file as the review authority; avoid maintaining conflicting copies when the design is updated. |

`StringCore.h` and `Formatting.h` were also inspected. Their string comparison and formatting utilities, together with `RString.h`, should be reused for names, schema fields, and diagnostics.

## Compile verification

Checks used `cl /Zs /W3` with the project include directories and Unicode definitions. These are syntax checks, not a DLL build or runtime tests.

| Translation unit | Result |
| --- | --- |
| Unmodified v2 header alone | Passed. |
| Original header together with `QuadooVM.h` | Passed. |
| Unmodified v2 header together with `QuadooVM.h` | Failed: C2011, redefinition of `IQuadooContainer`, followed by invalid base-class errors. |
| Temporary v2 copy with GUI `IQuadooContainer` renamed to `IQuadooControlContainer`, together with `QuadooVM.h` | Passed in both include orders. |

QuadooScript already defines `IQuadooContainer` at `QuadooVM.h:528` for arrays/maps. The GUI definition at `QuadooForm_v2.h:119` cannot coexist with it. The temporary renamed copy is only a compile experiment; neither published header was edited.

## Effects of the runtime-only frame rule

1. Keep independent Form and Frame root definitions, and keep `QuadooCreateFrame` requiring an existing parent container.
2. Reject `Frame` and `FrameInstance` in definition child arrays. A Frame is allowed as the root supplied to `QuadooCreateFrame`.
3. Remove declarative frame source references, instance overrides, recursive frame-file loading, resource resolution for those references, and cycle detection for declarative frame references.
4. Remove `IQuadooFrameObjectFactory` from the core header. It was proposed for automatic script-object creation during declarative frame loading. The runtime caller or external-class adapter already supplies each frame's binder.
5. Retain separate frame namespaces, separate script instances when scripting is used, parent-owned registration, immutable parentage, layout participation, event routing, and subtree destruction.
6. Serializing a form must omit attached frame instances and their complete subtrees. Serializing a Frame root serializes its own definition, while omitting any descendant frames attached at runtime. The loader must not recreate runtime composition implicitly.
7. Loading two copies of one frame definition requires distinct attachment names if they share a parent namespace. Initially the adapter can clone the definition and apply the instance name/placement before creation; this uses the existing JSON APIs and avoids modifying a shared definition. A convenience creation-options API can be considered later.

## Proposed header changes

These are review proposals, not edits to the published contract.

### Necessary corrections

- Rename the GUI `IQuadooContainer` to `IQuadooControlContainer` everywhere in v2: forward declaration, interface declaration, inheritance, `GetParent`, factory parameters, and `QuadooCreateFrame`. Keep the QuadooScript collection interface unchanged. The draft GUI IID can remain attached to the renamed interface since no released implementation has been identified.
- Remove the forward declaration and definition of `IQuadooFrameObjectFactory` and its `CreateFrameBinder` method under the runtime-only frame policy.
- Annotate the binder argument of both creation exports as optional. A native C++ caller can supply its own binder or pass NULL for a definition without bound events. A nonempty runtime handler name without a binder must fail construction. Design mode suppresses application binding and callbacks by contract.
- Annotate the factory's parent argument as optional for a Form root; require a parent for Frame and other child controls. A top-level form owner is distinct from a logical container parent.
- Document `RSTRING` ownership, null outputs on failure, and retained input-interface ownership. JSON inputs are borrowed and must not be mutated; any preserved JSON must be owned or cloned.
- Retain the new v2 IIDs, `STDMETHOD` calling conventions, `BOOL` public booleans, and `extern "C"`/`WINAPI` exports. Export both creators and the registry through the `.def` file when implementing.

### Contracts to settle before freezing

| Topic | Recommended contract |
| --- | --- |
| Identity | `GetID` returns the persistent definition GUID, scoped to a Form/Frame definition instance. Live object identity is canonical `IUnknown`, or the pair of namespace-root instance and definition GUID. Repeated frame instances may reuse definition GUIDs. GUID validation/indexes do not cross frame namespace boundaries; parent attachment names remain unique. No extra runtime GUID is needed initially. |
| Native-only use | A frame has an independent binder scope when events are bound, but direct C++ use does not require a script object. Only the script adapter enforces one script object per scripted frame. |
| Named handlers | Empty handler means unbound. A nonempty name must resolve eagerly to a delegate; `S_FALSE`/NULL for a named handler is a construction error, rather than silent success. |
| `PostBind` | Call once for the Form/Frame root after all descendants and handlers are bound, before `OnCreate`. The adapter populates the named-control variables/property variables declared by the designer in the associated script class. This timing is a proposed definition; repository evidence does not establish it. |
| Runtime handler edits | Rebind eagerly and atomically when `SetValueEvent` changes a ready control's handler: resolve the replacement before committing, and preserve the old binding on failure. |
| Creation modes | Provide and document a supported construction-context path for Runtime, Design, and Preview. The convenience creation exports currently accept no context. Also define how the designer supplies a native child-window host for its root design surface. These capabilities should not be implied by enums alone. |
| Serializer availability | Define how clients obtain `IQuadooFormSerializer`. The draft declares it but supplies no creator or documented `QueryInterface` provider. A `QuadooGetFormSerializer` export is a simple option. |
| Custom factory completion | Define how a third-party factory joins deferred binding/initialization and rollback. The current context only has getters, and the root loader cannot require undocumented concrete-class casts. A completion capability or construction-session service needs a concrete contract. |
| Diagnostic access | Specify how creation errors expose the JSON path, control, property, and handler information promised by the proposal; an HRESULT alone cannot carry that context. Prefer an optional construction report service. |

The creation-context, custom-factory completion, diagnostics, and design-host contracts may require additional signatures. They should be reviewed together before claiming the extensibility/design APIs are frozen.

## QuadooScript adapter recommendation

The external class loader supplies the native `extern(Form)` adapter. The script constructor invokes `Form.Create(this)` to create its native form. During that call, the adapter obtains the associated definition and event-interface name, calls `this->GetInterface` for that interface, constructs a binder, and calls the native creator. Each subsequent runtime frame creation uses its own script object, event-interface instance, and binder. The JSON `class` field is adapter metadata, rather than an instruction for the native DLL to instantiate VM objects. QuadooScript source and interfaces remain unchanged in this pass.

Keep `BindEvent` name-only. A delegate owns an `IQuadooInterface` reference and the `ULONG idxOrdinal` resolved on that interface. It receives one script-visible event-argument object. That argument wrapper implements `IQuadooObject` using the existing object pattern and maps property reads/writes to the typed native arguments. Writable flags and types are enforced by the native argument object. `Cancel` is read before returning from a close query; native notification results likewise remain synchronous. Retained argument wrappers must keep their underlying arguments alive safely.

The binding and invocation sequence is:

1. `Form.Create(this)` calls `IQuadooObject::GetInterface` with the associated event-interface name, for example `IMyForm`. The interface name is explicit adapter/designer metadata; the native core does not infer it from a class-name prefix.
2. For each nonempty callback name, `BindEvent(name, &delegate)` calls `IQuadooInterface::ResolveMethod(name, &idxOrdinal)`.
3. A successful binding creates a native delegate retaining that interface and ordinal. Interface retrieval or method-resolution failure aborts construction before application initialization; an empty callback name is unbound.
4. Event dispatch wraps `IQuadooEventArgs` as a script-visible object, supplies it as the single `QVPARAMS` argument, and calls `IQuadooInterface::Invoke(idxOrdinal, &params, &result)`. Clear the result with the existing variant ownership utilities after processing it. A script Boolean result remains distinct from the invocation HRESULT and the event object's mutable `Cancel`/`Handled` arguments.
5. Destruction/unbinding releases the delegates' interface references. A temporary strong reference protects an active invocation if the callback destroys its form or sender.

The script event interface declares callbacks with the agreed single `oArgs` parameter. `ResolveMethod` resolves names; it does not expose signature metadata. The existing `Invoke` implementation checks argument count. This pass adds no VM resolver or interface and does not access private `CVMObject` layouts.

The designer generates named-control variables or property variables in the associated script class and synchronizes them when controls are added, renamed, or removed. Runtime `PostBind` populates the existing declarations from the completed native hierarchy before `OnCreate`. Lookup and member population respect Form/Frame namespace boundaries, so separate runtime frame instances receive their own control references. The exact generated source syntax and editing strategy will be settled during designer implementation.

Delegates, binder, script wrapper, and native root can form reference cycles. Accepted `Close` and forced `Destroy` must detach bindings and adapter links, including failure rollback. Use the VM's release/destruction conventions for script ownership. A synchronous Win32 callback cannot leave an `E_PENDING` or asynchronous script result unresolved; verify reentrant VM invocation explicitly in the early scripting proof.

## Recommended behavior for remaining open questions

- **Lifetime and registration:** containers retain registered children. Reject ordinary unregistration of live controls. Permit controlled destruction/rollback. Reserve names before native creation, commit registration atomically, and unwind children, indexes, HWNDs, bindings, and references in reverse order on failure. HWND-held references and dispatch-held references keep objects alive. Clear weak parent links before parent final release; `GetParent` returns NULL when the original parent is gone and never attaches a different parent.
- **Lifecycle:** construct the complete definition hierarchy, bind all callbacks, call root `PostBind`, then initialize in documented child-before-parent order. Suppress application callbacks until that stage. Runtime frame creation performs the same transaction for the new frame subtree against a live parent. `Destroy` bypasses a cancellable close query; `Close` performs it. Guard repeated/reentrant destruction and keep interface identity readable after destruction.
- **Errors during close:** cancellation leaves the form alive and returns `S_FALSE`. A failing close-query handler aborts the close with its HRESULT. Once close is accepted, failures in close/destroy notifications must not prevent native teardown; return/report the first failure afterward. Dispatch must check whether the callback already destroyed its sender/root.
- **Modal behavior:** document a runtime `ModalResult` property or explicit completion operation, owner disable/restore, owner destruction, nested/reentrant calls, `WM_QUIT` propagation, and callback failures. The host retains the main message loop. A modal loop must restore owner state on every exit.
- **Names and JSON:** use exact case-sensitive name comparison initially, matching existing VM method-name resolution; validate rename before changing indexes. Enforce schema version, known types, property types, and unique names/IDs within definition scopes. Preserve unknown fields in editor-owned JSON for round trips, while runtime creation rejects unsupported behavior. Clone mutable JSON returned for defaults/values so clients cannot bypass validation/notifications by mutating shared objects.
- **Layout:** requested geometry remains in 96-DPI units and effective bounds are separate. Dock in stable child order; reserve a single `AlignClient` per container initially. Docking overrides anchors. Padding defines the content rectangle and margins reduce allocations. Resolve AutoSize and min/max in a deterministic measurement/layout pass; conflicting constraints fail predictably. Reordering must have a supported contract before a designer exposes it.
- **Scrolling:** retain the proposed viewport/content HWND split. Scroll changes the content offset, not child stored bounds. Both native notifications and painted-control input must route through the appropriate physical host to the logical component.
- **DPI and OS support:** the project does not establish an approved minimum runtime OS. SDK 6.0A lacks modern DPI declarations. Keep process DPI policy in the host, obtain newer APIs dynamically where supported, and fall back to system DPI/font metrics. Review the intended OS support matrix before implementation. Do not require a newer SDK or change toolsets implicitly.
- **Observers:** retain subscription references, snapshot them for dispatch, permit safe removal during a callback, coalesce nested updates, notify stored values before resulting effective bounds, and remove subscriptions during teardown. Observer return errors need a documented reporting policy without leaving committed properties half-applied.
- **Controls:** make `TabPage` an explicit container with its own page HWND, controlled by the tab wrapper. Treat GroupBox as a visual/button-class wrapper initially; use Panel when actual child containment is needed. If GroupBox containment is desired, define it before implementation. Design/Preview modes suppress application callbacks and use an input overlay or placeholders in the later designer.
- **Factory lifetime:** an instance retains whatever factory/module ownership is required for its code and descriptors to remain valid. Define rejection of unregistration/unloading while instances remain alive. Define registry threading separately from component UI-thread affinity; the draft's blanket affinity statement alone does not establish a process-wide registry policy.

These are recommended decisions, not repository-established behavior. The highest-priority remaining decisions are context/completion services, OS target, and the lifecycle/ownership contract. Event binding through `GetInterface`/`ResolveMethod` and delegates retaining the interface/ordinal is the user-specified protocol. Designer-generated named-control members are an accepted user assumption.

## Implementation sequence

1. **Reconcile and review the contract.** Apply the approved frame rule and script-class/designer assumptions to the architecture, settle the necessary header corrections and construction/lifecycle contracts, then replace the old header when implementation begins. Preserve the original header before replacement; read-only Subversion status currently marks the entire QuadooForm directory unversioned. No runtime implementation is part of this review.
2. **Build a native vertical slice.** Implement component identity/state, descriptors/property storage, registry/context, namespace and registration, Form/Panel, Label/Button/TextBox, named events, and JSON construction. Add a small VC9 native harness. Acceptance: a visible JSON-defined form, C++ binder callbacks, hide/show, cancellable close, deterministic destruction, and no script runtime dependency.
3. **Prove the script bridge and runtime frames.** Implement the external-class adapter's `Create(this)` path using `GetInterface`, eager `ResolveMethod`, and interface/ordinal delegates; populate declared named-control members, wrap event arguments, and support Frame creation. Acceptance: two copies of a frame created at runtime with separate script state/control references, shared method names, a missing-handler failure before initialization, close cancellation, and sender destruction during a callback. Test shutdown/cycle release and synchronous calls during VM reentrancy. QuadooScript remains unchanged.
4. **Complete layout and persistence.** Align/anchors/padding/margins, measurement/AutoSize, property batching, deterministic serialization, scrolling/content HWND, and DPI handling. Acceptance: expected bounds at 96/144/192 DPI, no geometry drift from scrolling, duplicate/rename rollback, and serialization omitting runtime frame attachments.
5. **Expand the native control catalog.** Checkbox/radio/group, combo/list, ListView, TabControl/TabPage, keyboard/focus navigation, and notification/result routing. Keep each addition usable directly from C++.
6. **Build the reusable designer surface.** Child-window design host, palette, hierarchy, property/events inspector, selection overlay, clipboard reconstruction with fresh GUIDs, undo/redo, JSON preservation, and script-class source updates declaring/synchronizing each named control's variable or property variable. Form and Frame definitions are edited separately; runtime frame instances are not saved inside form definitions.
7. **Integrate with Studio and application hosts.** Embed the designer in document tabs, add source navigation and preview, and share launch/message-loop/shutdown logic between release and debug hosts.

Run VC9 Debug/Release builds and meaningful lifetime, rollback, event, and layout tests as those behaviors are added. Failure cases belong in the first harness, rather than waiting for the final control catalog. Later x64 coverage requires actual x64 project configurations.

## Review scope and limitations

Only this review note was added to the project. Existing runtime source, project files, proposal files, and published headers were left unchanged. Compile experiments were written in the chat's scratch output directory and used syntax-only compilation.

No DLL build or window/script behavior test was run because the implementation is still a skeleton. The first Subversion status attempt received access denied while opening the client's registry configuration. Retrying with a scratch configuration directory succeeded and reported `platform/QuadooForm` as unversioned; its child directories consequently could not be queried as versioned nodes. No Subversion mutation was attempted.
