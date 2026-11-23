# QuadooForm Designer — Placeholders, Nonvisual Components, and File Dialogs

**Status:** Implementation proposal for Codex  
**Scope:** Extend the existing native QuadooForm runtime and standalone Win32 designer. Preserve the current public interfaces and ABI where possible. QuadooScript and Quadoo Studio runtime/source integration remain deferred.  
**Compatibility:** Visual Studio 2008 (VC9), Windows XP through modern Windows; existing project conventions, `RSTRING`, reference-counted interfaces, JSON library, and custom containers. No STL, C++ exceptions, or mandatory newer Windows APIs.

## 1. Background and goal

The standalone designer now supports Form/Frame editing, factory-based toolbox entries, an Objects hierarchy, property editing, JSON persistence, a design surface, runtime modal Preview of Forms, and an extensible component factory registry. The present designer deliberately fails to load a document if a control's factory cannot safely construct the control in Design mode; it preserves the current document instead of loading a potentially lossy representation.

This proposal introduces **two different, complementary features**:

1. **Design-time fallback placeholders** for visual controls that cannot be instantiated in Design mode, or whose factory is unavailable. These preserve their original types and definitions for later runtime construction.
2. **First-class nonvisual components**, intentionally present as runtime objects but with **no runtime HWND, no drawing, no layout rectangle, no focus, and no mouse/keyboard input**. They appear only as symbolic designer items, and can expose properties, operations, and eagerly bound events. The initial implementations are **OpenFile** and **SaveFile** dialog components inspired by VCL's `TOpenDialog` / `TSaveDialog`.

These features **must not be conflated**. An unavailable visual control is not a nonvisual component; a nonvisual component is not a broken visual control and should not be described as a placeholder.

## 2. Core semantics and component taxonomy

### 2.1 Visual controls

Visual controls retain their existing behavior: they implement `IQuadooControl` and the relevant static, windowed, or container interfaces. They participate in layout, visibility, placement, and hit-testing when appropriate. A visual control whose runtime factory supports safe Design-mode construction is shown normally in the designer.

### 2.2 Design-time placeholder for a visual control

If a visual component cannot be safely created for Design mode, substitute a designer-owned representation **without changing the original serialized component type**. Two cases are required:

- **Factory registered, Design construction unavailable/unsupported:** use factory descriptors and original JSON to show a typed placeholder and permit safe property editing.
- **Factory not registered:** use an unresolved placeholder, preserving all JSON and providing only safe, generic editing of known identity and geometry, when validation allows.

The original type name, GUID, component name, parent placement, properties, event-handler names, nested data, and unknown fields must survive save/load unchanged except for explicit, valid user edits. The placeholder is *not* a substitute runtime instance, does not expose the unavailable control's real capabilities, and is never serialized as `Placeholder`.

### 2.3 Nonvisual runtime components

Nonvisual components **do have runtime component identity and lifetime**, although they have no runtime graphical representation. They must be creatable from the Form/Frame JSON by the native factory/loader and registered in the appropriate Form/Frame component namespace. They expose `IUnknown`, component identity, type name, properties, construction/lifecycle and event binding capabilities where appropriate, **without falsely claiming `IQuadooControl`, `IQuadooWinControl`, `IQuadooStaticControl`, or `IQuadooControlContainer`**.

The fact that an OpenFile or SaveFile dialog *may show an OS file picker when explicitly executed* does not mean that the component itself owns a persistent HWND. The transient common-dialog window is owned by Windows/dialog code and is not a QuadooForm control attached to the Form client hierarchy.

Nonvisual components:

- Are declared inside the Form/Frame definition and belong to that definition's namespace, with persistent GUID and unique editable `Name`.
- Are instantiated for Runtime and Preview as native objects without a child HWND, independent of visual layout and scrolling.
- Are displayed in a dedicated **nonvisual component tray** in Design mode (typically beneath the visual form/frame canvas), with a small glyph/icon and component name, and in the **Objects** tree as a separate grouping under the Form/Frame root.
- Can be selected through the tray or Objects tree to edit properties and events. The tray position is editor-only UI state, **not** a runtime `Left`/`Top` or control bounds property.
- Are absent from the runtime Form or Frame's graphical tree, tab order, hit testing, painting, HWND hierarchy and scrolling area; they are not merely hidden visual controls.
- Participate in eager event binding (`BindEvent` remains name-only), `PostBind`/named-component population when the script adapter is implemented, lifecycle callbacks, destruction/rollback, and optional native C++ method invocation.
- Can later be exposed to QuadooScript through their concrete component interfaces; do **not** change QuadooVM or require Quadoo Studio integration in this milestone.

**Important architectural check:** Existing interfaces and loader code may assume every registered component implements `IQuadooControl`. Codex must inspect and correct that assumption rather than shoehorning nonvisual components into `IQuadooControl` with fabricated bounds or fake HWNDs. Extend the component registry/namespace and JSON loading model with appropriate component-centric abstractions. If a public ABI addition is necessary, make it additive with new IIDs/exports; do not alter released vtables, IIDs, existing export ordinals or visual-control behavior. Present the proposed interface additions and their ownership semantics in the implementation report.

## 3. JSON schema, identity and namespace

### 3.1 Proposed structure

Use a clear dedicated collection for nonvisual components at each **Form/Frame definition root**, for example `"components"`. Keep the existing `"children"` array reserved for visual controls; do not insert nonvisual objects into visual child/layout arrays.

Example (proposed schema extension; verify actual active serializer and schema before implementation):

```json
{
  "schemaVersion": 1,
  "type": "Form",
  "name": "MainForm",
  "id": "b2509f3a-5bb9-4882-a833-e78474d96440",
  "properties": {
    "Caption": "Import Data",
    "Width": 640,
    "Height": 420
  },
  "children": [
    {
      "type": "Button",
      "name": "btnImport",
      "properties": {
        "Caption": "Import...",
        "Left": 24,
        "Top": 24,
        "Width": 110,
        "Height": 28
      },
      "events": {
        "OnClick": "btnImport_OnClick"
      }
    }
  ],
  "components": [
    {
      "type": "OpenFile",
      "name": "OpenFile1",
      "id": "3eb4638c-c3fc-4ee2-8316-30b6479d7215e",
      "properties": {
        "Title": "Select a file",
        "Filter": "Text files (*.txt)|*.txt|All files (*.*)|*.*",
        "FilterIndex": 1,
        "DefaultExt": "txt",
        "FileName": ""
      },
      "events": {
        "OnAccept": "OpenFile1_OnAccept"
      }
    }
  ]
}
```

IDs above are illustrative. Codex should use the current GUID formatting conventions. Do not silently bump the schema if v1 can be safely extended; otherwise document and implement explicit versioned compatibility.

### 3.2 Schema requirements

- The `components` array is optional; existing JSON files with only `children` continue to load unchanged.
- `components` must contain only nonvisual component definitions. Reject a visual control in this collection and a nonvisual component in `children`, with useful diagnostics.
- Names and GUIDs must be unique within the complete Form/Frame namespace, **across both** visual controls and nonvisual components; independent Frame definitions/instances have separate namespaces. Avoid duplicate names even if one is visual and one nonvisual.
- Nonvisual components are attached to the Form/Frame root namespace, not to a pixel-positioned Panel or TabPage. The Objects tree can group them under a virtual "Components" node; that node is not a serialized component.
- Frame definitions remain independent; **runtime-attached Frame instances are never declared as child entries in Form JSON** and are omitted by Form serialization. A Frame definition may declare its own nonvisual components.
- Serialize component GUIDs, names, properties and event-handler assignments deterministically. Unknown fields remain intact in designer round trips. Do not serialize runtime dialog HWNDs, transient handles, last error codes, or nonpersistent results unless a property is explicitly marked serializable.
- Undo/redo or clipboard support added later must preserve GUIDs for an edit/undo cycle and allocate new GUIDs when duplicating a component.
- Preserve form/frame definitions even when a component's factory is unavailable; unknown nonvisual components should be represented in the tray and Objects tree with an unresolved icon, not discarded.

## 4. Designer behavior and UI

Retain the previously established standalone application structure:

- **Standard Win32 menu** with New Form, New Frame, Open, Save/Save As, and Preview; Preview is available only for Form documents, and launches a separate real top-level Form through `ShowModal()` using a snapshot of unsaved in-memory JSON.
- **Left pane:** Objects tree in the upper portion; factory-driven Toolbox in the lower portion. Form and Frame are **File > New** document types, never toolbox items.
- **Center:** scrollable native design surface. A Form displays a designer-painted simulated non-client frame (caption, borders, caption buttons, configurable icon); a Frame has **no simulated non-client frame**.
- **Right pane:** descriptor-driven property and event inspector.
- **Nonvisual component tray:** a dedicated clearly separated region under the client design surface, with labeled glyphs for nonvisual components. It may scroll/wrap independently. Do not position these glyphs in the real Form's client coordinates or include their area in Form auto-scroll calculations.
- Object/tree/tray/property selection stay synchronized. Choosing `OpenFile1` in the tree selects its tray glyph and opens its properties. Nonvisual components have no resize handles; drag gestures may rearrange their tray icons for editor convenience but cannot change runtime bounds.
- The Toolbox has distinct categories **Visual Controls** and **Nonvisual Components**, populated from registered factory metadata/capabilities. Add `OpenFile` and `SaveFile` under Nonvisual Components. Selecting or dragging such a toolbox entry creates a new nonvisual component in the root Form/Frame namespace and a tray glyph regardless of which visual container happens to be selected.
- Placeholders for unavailable visual controls remain on the design surface with a dashed/visually distinguishable border, original name and type, and a reason/status tooltip. Their eight resize handles and placement restrictions are based on known metadata; when metadata is missing, edit only values that can be safely validated.
- Unavailable nonvisual component factories produce **tray placeholders**, not fake visual controls.
- Preview instantiates the real available runtime components and visual controls; if a required runtime factory is missing or creation fails, report a clear error without corrupting the open designer document. Do not silently omit required components.

### 4.1 Fallback placeholder behavior and transactional loading

Current failure-safe transactional document loading must be preserved. Introduce placeholder fallback in a candidate document session before committing that session to the editor; a malformed JSON document, duplicate identity, corrupted known property, or unrecoverable structural violation still fails without changing the active document.

A missing/unsafe factory is not by itself a fatal load error if its definition can be safely represented, indexed, and preserved. Use a reason code to distinguish `MissingFactory`, `DesignConstructionUnsupported`, and genuine invalid definitions. Do **not** catch arbitrary factory construction failures (including memory/resource failures, partially created HWNDs, or invalid property errors) and blindly convert them into placeholders; only fall back for a documented safe, intentional inability to build at design time, with proper rollback.

For visual placeholders with children, retain the recursive original JSON subtree, identities and event assignments. Display descendants in the Objects tree when safe; do not assume their native layout/coordinates can be calculated accurately without a functioning parent host. Preserve all nested JSON on save.

When a missing factory is later registered, support refresh/reload (at minimum reopening the document) to resolve the placeholder into the real control without changing its GUID or definition.

## 5. Nonvisual component contract and extensibility

### 5.1 Recommended interfaces (conceptual only)

Codex must inspect the current active `QuadooForm.h` and implementation before choosing names/signatures. The following conveys *behavior*, not frozen ABI:

```cpp
// Nonvisual components still implement IQuadooComponent and IUnknown.
// They MUST NOT claim IQuadooControl simply to fit the visual loader.
interface IQuadooNonvisualComponent : IQuadooComponent
{
    // Marker or optional nonvisual-specific capabilities only.
};

interface IQuadooFileDialog : IUnknown
{
    STDMETHOD(Execute)(HWND hwndOwner, BOOL* pfAccepted) = 0;
    // FileName/result exposed through typed properties or optional accessors.
};
```

Maintain a shared component-level source of `GetID`, `GetName`, `GetTypeName`, `GetProperties`, `GetState`, `Destroy`, and the same construction/event binding contract. Provide a discoverable factory capability such as `IsVisual` / `ComponentCategory` or QueryInterface-based classification. Classification must not require constructing an instance merely to populate the toolbox.

The visual control parent/child model can remain as-is; add a root-owned component collection or small generic component-store service that retains native nonvisual objects and shares the namespace/ID index. Do not make nonvisual components fake child windows, add an HWND, or enter the layout traversal. Ensure the root's destruction and failed-load rollback destroy/release them and their event delegates correctly.

The initial built-in factories must coexist with independently implemented third-party **nonvisual** factories. Test the public factory/registry contract from a separate extension DLL using only published interfaces; avoid `CControl`, `CFactory`, `TypeSpec`, or other private dependencies in the external test.

### 5.2 Event binding and script expectations

- Event names remain separate from property values in JSON, and the existing `IQuadooEventsBinder::BindEvent(RSTRING handlerName, ...)` stays **name-only**.
- Eagerly resolve every nonempty nonvisual event handler before the root Form/Frame `OnCreate`. An unresolved named handler fails Runtime construction transactionally, as for visual controls. Design and Preview suppress application callbacks according to current policy.
- When QuadooScript integration is implemented, generated Form/Frame script classes must declare named nonvisual component members exactly as they do for named visual controls. Root `PostBind` populates both types before application initialization events.
- The script bridge will continue to use the established `GetInterface` → `ResolveMethod` → `Invoke` protocol, with typed mutable event arguments and distinct script instance/binder per runtime Frame instance. **Do not implement or modify QuadooScript or Studio as part of this assignment.**
- Event callbacks can release/destroy their sender or close the Form; hold references safely and prevent object/delegate cycles.

## 6. OpenFile and SaveFile nonvisual components

Implement **two separately registered concrete component types**: `OpenFile` and `SaveFile`. They appear in the Nonvisual Components toolbox category and the designer tray, not in the visual canvas. Their common behavior should be implemented through a reusable native dialog-component base rather than duplicating file picker logic.

### 6.1 Reuse existing repository infrastructure

**Required**: inspect and reuse `CChooseFile` from:

`D:\dev\projects\trunk\shared\library\ChooseFile.cpp`

Codex has previously verified the available operations `CChooseFile::Initialize()`, `OpenSingleFile(HWND, title, filter)`, `SaveFile(HWND, title, initialName, filter)`, and `GetFile(0)`. Inspect the corresponding header, overloads, filter representation, owner handling, flags, and actual result/cancellation conventions before implementing wrappers. Do **not** introduce a second, independent file-dialog implementation by calling `GetOpenFileName`/`GetSaveFileName` directly unless an unavoidable capability requires a documented extension to CChooseFile.

For designer Open/Save `.qform.json` and `.qframe.json` files, continue to use `CChooseFile` and `Text::LoadFromFile` / `Text::SaveToFile` from:

`D:\dev\projects\trunk\shared\library\util\TextHelpers.cpp`

Do not confuse **the designer's own Open/Save commands** with the **OpenFile/SaveFile nonvisual components placed on the designed form**. The former manipulate design documents; the latter are components that an eventual application script or native C++ caller invokes at runtime.

### 6.2 Proposed shared properties

Use descriptor-driven, serializable properties when the existing `CChooseFile` functionality supports them. Initial minimum:

| Property | Type | Meaning |
|---|---|---|
| `Title` | String | File dialog caption |
| `Filter` | String | VCL-style pattern groups, e.g. `Images (*.png)|*.png|All files (*.*)|*.*` |
| `FilterIndex` | Integer | 1-based selected filter index; persist if supported |
| `InitialDir` | String | Initial browse directory, if supported |
| `DefaultExt` | String | Suggested/default file extension, if supported |
| `FileName` | String | Suggested filename before execution and selected filename after success |
| `Options` | Flags/JSON | Optional file-picker options, restricted to features verifiably supported by `CChooseFile` |

**Important:** Do not claim that the existing CChooseFile API supports `InitialDir`, `DefaultExt`, `FilterIndex`, or all flags until Codex has inspected it. If those capabilities are absent, either extend `CChooseFile` carefully in its own library with backwards-compatible behavior and tests, or document/defer them in the first pass. Do not implement silently ignored property setters. Pick sensible default behavior compatible with XP.

For SaveFile, filename/extension handling must match the actual dialog semantics. A successful chooser selection does **not** itself create or overwrite a file; the caller remains responsible for writing its contents. Avoid implicit file I/O in the component.

### 6.3 Execution and results

Provide a native callable `Execute(...)` operation on the dialog interface. Resolve the owner HWND from the current Form/Frame's appropriate top-level runtime Form when not explicitly supplied, with a documented fallback/error for nonvisual-only contexts. Never require or fabricate a child HWND on the component itself.

- `Execute` presents the system Open or Save file dialog modally and blocks until the user accepts or cancels.
- On acceptance: set `FileName` to the chosen path, return an accepted result, and raise an optional logical `OnAccept` / `OnSelected` callback through the normal typed event system (if the runtime binder is present).
- On cancellation: return a distinct nonerror cancellation result (`S_FALSE` or a documented `pfAccepted=FALSE` convention); preserve the prior `FileName` unless an explicit, documented alternate contract is approved. Optionally raise `OnCancel` (but do not require it if the agreed initial API only exposes acceptance).
- On failure: return an HRESULT failure and preserve the previous properties; report useful diagnostics. Do not conflate user cancellation with an error.
- Use a strong self-reference during synchronous dialog execution and callback delivery, because application callbacks may destroy the containing Form or sender.
- Reject wrong-thread calls consistently with other QuadooForm UI-affine components. Ensure dialog owner enablement and focus are restored on every exit path.
- No file picker is launched during form construction, design-time display, document load, serialization, or Preview initialization. Preview may create the component but suppresses application script event callbacks per existing policy; explicit native invocation during a preview session should follow documented Preview behavior.

The designer should expose an **Execute** action only if a safe preview/test affordance is deliberately provided later. Merely selecting the tray component does not display a file chooser.

### 6.4 Suggested event model

At minimum, components should participate in standard lifecycle events and support logical dialog result events if the public event descriptor model can represent them safely. Suggested events:

- `OnAccept`: raised after a successful selection with selected filename available; optionally include `FileName` in the typed event arguments.
- `OnCancel`: optional, raised if the user explicitly cancels.

Do not invent new QuadooScript invocation conventions. Allow events to bind through the current native C++ binder and later through the script adapter.

## 7. Designer placeholders vs. nonvisual tray icons: required distinctions

| Characteristic | Visual design placeholder | Nonvisual component | Nonvisual missing-factory placeholder |
|---|---|---|---|
| Purpose | Represents visual control unavailable at design time | Real component with no runtime visuals | Preserves unavailable nonvisual definition |
| In JSON | Original visual `children` entry | Root `components` entry | Original root `components` entry |
| Runtime HWND | Actual visual control may have HWND | Never owns persistent HWND | Depends on restored actual component, ordinarily none |
| Design surface | Bounding placeholder | No bounds on canvas | No bounds on canvas |
| Designer UI | Canvas rectangle + Objects tree | Tray glyph + Objects tree | Tray warning glyph + Objects tree |
| Resize handles | When applicable | Never | Never |
| Runtime binding | Actual control's events | Real nonvisual object's events | Actual component required at runtime |
| Save | Preserve original control type/JSON | Preserve original component type/JSON | Preserve original type/JSON |

## 8. Preview and modal behavior

The standalone designer's **Preview** command remains available only for Form documents and continues to create an **independent real top-level Form window**, invoking `IQuadooForm::ShowModal(LRESULT*)` with the latest in-memory form JSON. It does not preview Frame documents as standalone windows. Buttons' existing `ModalResult` semantics continue unchanged.

Preview must include and instantiate the Form's nonvisual components, not ignore them. Because application callbacks are suppressed in Preview, the mere presence of an OpenFile or SaveFile component must not trigger a chooser. A required factory missing in Runtime/Preview must be reported clearly; *do not* silently drop it or substitute a designer placeholder in the actual preview Form.

Existing form icons, simulated non-client rendering, BorderStyle/BorderIcons, Form auto-scrolling, VCL-style modal buttons, eight resizing grips and ComboBox resizing restrictions are unaffected. Nonvisual components have no resize grips and no client geometry.

## 9. Compatibility, ownership and error handling

- Build with VC9 and SDK 6.0A, on Windows XP and current Windows. Reuse native windows/common dialogs supported on XP; do not require COM file dialog interfaces introduced after XP.
- Respect `RSTRING` ownership; use existing SimbeyCore facilities, custom containers and the project's `CBaseUnknown` patterns. Preserve canonical `IUnknown` identity, `QueryInterface` symmetry, and correct `AddRef`/`Release` balance.
- Document ownership of the new component collection and its weak/strong links to the Form/Frame. Avoid cycles between root → component → owner/binder → root.
- Keep nonvisual components alive until explicitly destroyed/root teardown, even though they have no HWND. They must not be destroyed merely because their tray glyph is hidden or the designer changes pages.
- Ensure component destruction unregisters its name and GUID, releases event delegates and metadata references, and tolerates reentrant event callbacks.
- Preserve the existing transactional loading, replacement and JSON rollback behavior. New failure modes must not corrupt the active document.
- A third-party design-safe component and an unavailable component must use the same public extensibility rules as built-in controls.
- Verify factory code lifetime while component instances and event argument/descriptor objects remain referenced; unloading a factory's DLL prematurely must remain prohibited.

## 10. Testing and acceptance criteria

Expand standalone designer and native harness tests. At minimum:

1. Existing Form/Frame documents without `components` still load/save and pass all existing tests unchanged.
2. Create Form and Frame documents; insert OpenFile and SaveFile from the toolbox; they appear in the tray and Objects tree and support property editing.
3. Save/reload: names, persistent GUIDs, property values, event assignments, component order and unknown fields remain intact, with no fake bounds or HWND fields.
4. Reject duplicate name/GUID collisions across visual and nonvisual components within one Form/Frame namespace; preserve separate Frame namespaces.
5. Runtime construction creates real nonvisual component objects and eagerly binds native callbacks before root `OnCreate`, but creates no component-owned HWND and no keyboard/focus/layout node.
6. Native C++ calls to OpenFile/SaveFile `Execute` invoke `CChooseFile`; cancellation returns a distinguishable nonerror result and preserves prior filename; success updates `FileName`, and event dispatch and destruction during callbacks are safe. Interactive manual dialog tests are appropriate; automate all non-UI paths via an injectable test chooser where feasible.
7. Design and Preview construction never display dialogs spontaneously; Form modal Preview includes component objects, doesn't modify the design document, and remains correctly modal.
8. A registered unsafe-for-Design visual factory yields a typed canvas placeholder with original JSON preserved; a missing visual factory yields an unresolved canvas placeholder. Reopening after registration resolves it normally.
9. A registered/missing nonvisual factory unable to construct in Design mode yields a tray placeholder, not a canvas control. Its full JSON and events survive save/reload.
10. Malformed JSON, duplicate IDs, corrupt known values and genuine factory/runtime failures still cause transactional rejection without replacing the active designer document.
11. Placeholder copies of nested visual subtrees preserve children and unknown data; no silent loss of serialized content.
12. Independent extension DLL adds a new nonvisual component via published factory APIs; no private base-class derivation is required.
13. Confirm all existing Debug/Release harness assertions plus new tests pass; no leaks of HWNDs, event delegates, `RSTRING`s, factories, or component objects after destruction.
14. Run smoke tests on Windows XP (the user has an XP VM) and on modern Windows; explicitly report untested OS/theme/DPI cases.

## 11. Recommended implementation phases

**Phase 1 — Component-model and JSON design review**

Inspect the current published/runtime APIs, factory registry, serializer, designer session, tree model, and construction transactions. Propose minimal additive interface changes to support root-scoped nonvisual components, factory classification, and safe placeholder representations. Establish deterministic JSON migration/backwards-compatibility and failure reasons. **Do not fake nonvisual instances as controls.**

**Phase 2 — First-class nonvisual components**

Implement construction, registry, namespace/identity index, eager binding, lifecycle, root ownership, JSON load/save, designer tray, tree/property/event editing, and factory-driven toolbox discovery for nonvisual components. Verify with a native mock before the file dialogs.

**Phase 3 — OpenFile and SaveFile**

Implement two built-in dialog factories/components using `CChooseFile`, common metadata, executable native interface, selection/cancellation semantics, and optional logical result events. Add the two toolbox entries and tray glyphs. Test dialog paths and XP behavior.

**Phase 4 — Safe design-time placeholders**

Implement registered-but-unsafe and missing-factory fallbacks for visual controls and nonvisual components, with accurate unresolved status, lossless original JSON, Objects tree navigation, conservative property editing, independent preview behavior, and transaction-safe failure handling.

**Phase 5 — Documentation and regression**

Update runtime/designer usage docs, sample JSON, extension-DLL examples, API/ABI notes, and Debug/Release tests. Confirm XP smoke tests where possible. Keep Studio/QuadooScript integration, ListView, and other deferred controls out of scope.

Codex may reorder internal development steps if dependencies demand it, but must deliver all five phases as part of the requested milestone, and must not silently omit placeholder support or true nonvisual semantics.

## 12. Codex execution instruction

> Please implement the QuadooForm extensions described in `QuadooFormDesigner_ExtensibilityAndDialogs.md` against the current repository. Treat this document as a new milestone after the standalone designer implementation. Preserve the existing form/frame JSON behavior, visual component ABI, Visual Studio 2008 and Windows XP targets, and the current designer's transactional editing and modal Preview behavior.
>
> First inspect the actual public interfaces and component factory architecture. In particular, do **not** represent nonvisual components as invisible `IQuadooControl` instances: they must be real nonvisual `IQuadooComponent` objects with identity, property/event descriptors, eager binding, persistence and lifetime, but no runtime HWND or visual control behavior.
>
> Add a designer nonvisual component tray and Objects-tree entries, and expose `OpenFile` and `SaveFile` in the factory-driven Nonvisual Components toolbox category. Implement both file dialog components using the repository's `CChooseFile`; do not duplicate its dialog implementation. Preserve valid VCL-style behaviors where supported and explicitly document any properties that require extending `CChooseFile`.
>
> Implement design-time visual and nonvisual placeholders for unsafe or unavailable factories, with original JSON/GUID/event preservation, safe property editing, and no lossy substitution. Keep genuine malformed document and construction failures transactional.
>
> Test the resulting native runtime and standalone designer in Debug and Release, including a separately built third-party extension DLL, and verify Windows XP compatibility where the available environment permits. Update the API/usage/validation documentation, report all additive ABI changes and deferred capabilities, and leave QuadooScript and Quadoo Studio untouched.
