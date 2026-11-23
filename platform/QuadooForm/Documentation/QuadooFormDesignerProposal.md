# QuadooForm Visual Designer — Design and Implementation Proposal

**Status:** Proposed implementation specification for Codex  
**Target:** Native Win32 C++, Visual Studio 2008 / VC9, Windows XP through supported modern Windows versions  
**Initial deliverable:** Standalone executable hosting a reusable, embeddable designer component  
**Later deliverables:** Quadoo Studio integration, QuadooScript source synchronization, and runtime script adapter

## 1. Purpose and governing architecture

Build a VCL-inspired graphical editor for JSON-defined QuadooForm **Forms** and **Frames**. Users should create and select components, change their properties and events, position/resize controls, navigate the object hierarchy, and save/reopen definitions. Forms should display an **artificial, designer-drawn non-client window frame**. Frames should **not** display any artificial window caption or border. A **Preview** command must instantiate the current Form as a separate, genuine Win32 runtime form and display it modally.

This document supplements the current published `QuadooForm.h`, implemented native runtime, `NativeRuntime.md`, and latest Codex architecture review; it is **not** permission to revert the runtime to the older, unimplemented architecture proposal. Where the sources conflict, inspect the current repository, report the discrepancy, and preserve the subsequently accepted decisions:

- QuadooForm.dll is independently usable from C++ and does not depend on QuadooScript.
- External control factories construct controls, attach their HWND to the chosen parent host, and register the resulting control with its logical parent. No reparenting of existing controls.
- Components have persistent JSON GUIDs, editable namespace-unique names, descriptor-driven properties/events, and interface-based capabilities. Avoid built-in-type enums and casts to implementation classes in extension paths.
- A Frame has its own root definition and namespace and must be attached to a parent at runtime. **Do not put Frame or FrameInstance entries inside a Form or Frame definition's `children` array.** A form's serialization excludes runtime-attached frame instances and subtrees.
- Form and Frame definitions can be edited independently. The future QuadooScript adapter will bind callbacks by name and use existing `GetInterface` -> `ResolveMethod` -> `Invoke` support; it is not in scope for the initial designer.
- Geometry stored in JSON uses **96-DPI logical units**. Designer zoom and scroll offsets must never change persisted geometry.
- Preserve Visual Studio 2008 compatibility, the project's `RSTRING`, JSON, reference-counting and custom container infrastructure, Windows XP compatibility, and existing coding conventions. Do not introduce STL, C++ exceptions, COM registration, or a modern GUI toolkit.

## 2. Proposed standalone application UI

Use a conventional native Win32 application window with a **standard Win32 menu bar** (`HMENU` and ordinary `WM_COMMAND` routing), a document/editor area, optional compact toolbar, splitters, and a status bar. The primary layout is:

```text
+--------------------------------------------------------------------------------------+
| File   Edit   View   Form   Help                                                     |
+--------------------------------------------------------------------------------------+
| [New] [Open] [Save]                 [Preview]                  [Zoom: 100%]            |
+--------------------------+----------------------------------+------------------------+
| Objects                  | Design workspace                 | Properties / Events    |
|--------------------------|                                  |------------------------|
| > MainForm (Form)        |   +--------------------------+   | [Properties] [Events]  |
|   > pnlDetails (Panel)   |   | [icon] MainForm   _ □ X   |   | Name       btnSave     |
|     - txtName           |   +--------------------------+   | Caption    Save        |
|     - btnSave           |   |                          |   | Left       130         |
|   > tabsMain            |   |    Selected button       |   | Top        90          |
|     > pageA             |   |    [o-----------o]       |   | Width      120         |
|       - cmbCategory     |   |    [|   Save    |]       |   | Height     27          |
|     > pageB             |   |    [o-----------o]       |   | ModalResult mrOk       |
|--------------------------|   |                          |   | ...                    |
| Toolbox                  |   +--------------------------+   |                        |
|--------------------------|                                  |                        |
| Pointer                  | (scrollable/zoomable canvas)     |                        |
| Label / Button / TextBox |                                  |                        |
| Panel / TabControl / ... |                                  |                        |
+--------------------------+----------------------------------+------------------------+
| Selected: btnSave  |  Client: 640 x 480  |  X: 130 Y: 90  |  100%  |  Modified        |
+--------------------------------------------------------------------------------------+
```

The sketch is illustrative, not a pixel-perfect specification. Use resizable splitters. The **left column is split vertically**: Objects tree at the top, Toolbox at the bottom, with a resizable divider. The design workspace gets the largest area. The property inspector occupies the right column. Persist user editor layout/preferences separately from the form/frame definition.

### 2.1 Menu structure and commands

At minimum:

- **File > New > Form**: create a fresh `type: "Form"` JSON definition, assign a new persistent root GUID, and open it on the designer surface with simulated window chrome.
- **File > New > Frame**: create a fresh `type: "Frame"` JSON definition; show only its editable client/content rectangle **without** a simulated caption, system buttons, or top-level non-client border.
- **File > Open...**: select a Form/Frame JSON file, read and validate it, and open the appropriate designer view.
- **File > Save** and **Save As...**: serialize the current definition, preserving IDs and metadata, and write it as text. Save uses the existing path or invokes Save As for an untitled definition.
- **File > Exit**: offer Save / Don't Save / Cancel if edits are unsaved.
- **Form > Preview** (also toolbar button and optional F5): enabled **only when editing a Form**, disabled when editing a Frame. Preview runs from the **current in-memory definition**, whether or not it has been saved to disk.
- **Edit** should at least include Delete and select operations in the first phase; undo/redo/cut/copy/paste can be staged, but reserve commands and design accordingly.
- **View** may contain grid, snap, zoom, pane visibility, and reset-layout commands as features mature.

Keep menu enablement and toolbar button state synchronized with the current document and selected component. All menu actions need real implementations, not inert placeholder commands.

### 2.2 Required repository utilities

**Inspect and reuse these existing files rather than introducing new file-picker or text-file infrastructure:**

- `D:\dev\projects\trunk\shared\library\ChooseFile.cpp` — `CChooseFile`: choose Form/Frame JSON files for Open and Save/Save As; also choose `.ico` files for Form icon editing. Consult the corresponding headers and actual signatures before calling it.
- `D:\dev\projects\trunk\shared\library\util\TextHelpers.cpp` — existing text-file loading and saving helpers for the JSON documents. Consult the corresponding header, encoding and ownership conventions; preserve Unicode correctly and report I/O errors.

These paths identify files to inspect, **not confirmed APIs**; do not invent method names, signatures, or encoding behavior. Keep filename extensions and JSON encoding policy explicit after inspecting current project conventions. File dialogs should apply appropriate filters (Forms, Frames, JSON; icons for the icon picker).

## 3. Design surface and visual composition

### 3.1 Embedded surface, not a floating top-level Form

The editor lives inside a native child-window design host that can later be embedded in Quadoo Studio. It has a scrollable/zoomable workspace. Within that workspace, the Form's *client area* is a QuadooForm **design-mode** host; the simulated non-client frame surrounding it belongs entirely to the designer. For a Frame root, show its content area directly without any simulated top-level chrome.

Avoid using `SetParent()` to embed a previously created top-level Form HWND. Use the supported design construction/child-host mechanism, maintaining immutable logical and HWND parentage. The designer must not dispatch ordinary application events while editing. Host real controls when feasible; render safe placeholders when a custom control cannot safely initialize in Design mode.

### 3.2 Simulated Form non-client area

Provide a reusable private renderer (for example `CDesignerWindowFrame`) that paints the **caption, form icon, title text, border, and the applicable minimize/maximize/close buttons** around the Form's design-mode client surface. The drawn buttons are decorative; do not invoke real minimize/maximize/close behavior. Clicking the title bar selects the Form; resizing is handled by designer gestures.

**Preferred rendering:** a hybrid of Windows system/theme data and GDI:

1. Obtain border/caption metrics, caption font and colors from appropriate Win32 system APIs (`GetSystemMetrics`, `SystemParametersInfo(SPI_GETNONCLIENTMETRICS, ...)`, and DPI-aware equivalents when available).
2. Where dependable for the running theme, use `OpenThemeData` / `DrawThemeBackground` for applicable visual parts; fall back to `DrawCaption`, `DrawFrameControl`, and custom GDI painting. Check API availability and use dynamic resolution for newer APIs absent from the Windows XP/SDK 6.0A target.
3. Do **not** assume UxTheme reproduces an exact DWM-composited Windows 10/11 window frame. Visual fidelity is best-effort, and Preview is the authoritative native appearance on the host OS.
4. Handle theme, system-color, font, and DPI changes; repaint and recalculate frame metrics without changing stored client geometry.
5. Consider a fixed/classic GDI simulation mode later if helpful, but **do not hardcode one Windows version** as the only appearance.

The Form's serialized `Width` / `Height` represent **client-area** dimensions only. The artificial caption and borders lie outside that rectangle. Resizing the simulated outer bounds must compute and store the correct client dimensions. Caption appearance and available system buttons must reflect Form properties (see §4).

### 3.3 Form icon and Base64 persistence

A top-level Form must have a designer-editable **Icon** property. It should be exposed in the Properties inspector with an icon preview and a browse/replace/remove affordance. Browse uses the existing `CChooseFile` helper and initially accepts an `.ico` file.

- Read the **original ICO file bytes** and encode them as Base64 for JSON persistence; do not serialize a transient `HICON`, resource handle, or local filesystem path as the only source of truth.
- Proposed encoding inside the Form's properties:

```json
{
  "schemaVersion": 1,
  "type": "Form",
  "name": "MainForm",
  "properties": {
    "Caption": "My Application",
    "Width": 640,
    "Height": 480,
    "Icon": {
      "format": "ico",
      "encoding": "base64",
      "data": "AAABAAEA..."
    }
  },
  "events": {},
  "children": []
}
```

This is a **proposed schema**, not a claim that today's runtime already supports `Icon` or that the JSON library exposes a built-in ICO decoder. Codex must reconcile it with existing property types, descriptor/JSON validation, serializer, and public ABI, preferably with a reusable icon-value representation rather than bespoke JSON parsing in the designer.

- Decode and validate the ICO header, directory entries, lengths, and bounded total size; reject malformed or excessive Base64/icon data with actionable errors. Restrict accepted formats for the first release to formats actually supported and verified on Windows XP.
- Extract the appropriate icon-image resource from the ICO container before passing data to `CreateIconFromResourceEx` (it does **not** accept an entire `.ico` file directly). Another verified Windows-XP-compatible loader is acceptable.
- Use the decoded icon in the simulated caption (scaled to appropriate small-icon metrics), and set the genuine Form HWND's small/large icons when Preview creates it (`WM_SETICON` or the equivalent runtime-owned method). Define ownership and destroy owned icon handles safely after replacement/destruction.
- Save/load must preserve the full embedded ICO bytes, including multiple sizes/depths where applicable. Support unset/cleared icon with a sensible OS fallback; changing the icon should update the simulated caption immediately.
- Forms have an Icon property; **Frames do not have a top-level title-bar icon** in the initial designer.
- Ensure editing an icon and saving a document does not depend on keeping the original `.ico` file accessible afterward.

## 4. Form-specific runtime properties and behaviors

The designer will exercise real runtime Form behavior, so **the native framework must support these properties before the designer claims full support**. Descriptor-driven inspectors should discover and edit them rather than hardcoding ad hoc runtime fields. Inspect current implementation and add/extend property descriptors and JSON serialization with backward-compatible defaults as feasible.

### 4.1 Form border, caption, and window state

Provide VCL-like Form configuration (names below are suggested, not mandatory ABI names):

- **BorderStyle**: meaningful fixed and sizable variants (e.g., `None`, `Single`, `Sizeable`, `Dialog`; add additional VCL variants only where the native implementation has clear, tested behavior). This controls whether the runtime window may be resized. Differentiate the border style from the caption/system menu button flags.
- **BorderIcons** or equivalent flags: system menu, minimize button, maximize button; Close availability should follow a deliberate native-window contract. The simulated caption displays only buttons enabled by those properties.
- **WindowState**: Normal, Minimized, Maximized where meaningful. A design-time Form should remain editable even if its runtime initial state is minimized/maximized; indicate the configured value in the inspector instead of collapsing the design surface.
- **FormStyle**: ordinary top-level behavior first; if emulating VCL `fsStayOnTop` or similar, map to actual Win32 semantics and state which combinations are supported. Do not silently claim MDI support without implementing it.
- **Caption**, optional **Icon**, **MinWidth/MinHeight/MaxWidth/MaxHeight**, and the existing client-size and position properties must remain consistent with these styles.

Use correct style flags during native creation and recalculate non-client geometry on relevant property changes. Define which style changes require controlled HWND recreation versus `SetWindowLongPtr`/`SetWindowPos(... SWP_FRAMECHANGED)`; preserve interface identity, child hierarchy, and event lifecycle. Test Windows XP and modern Windows.

### 4.2 Automatic scrolling on Forms

**Forms must be able to scroll like ScrollingPanel when their content exceeds their client viewport.** Reuse the existing scrolling/layout machinery rather than writing a second unrelated system.

- Prefer an `AutoScroll` property on Form (with explicitly chosen default compatible with existing saved definitions). When enabled and content extends beyond the available client size, show horizontal/vertical scrollbars as necessary.
- Use the viewport/content HWND model or another equivalent validated architecture. Scroll movement changes **effective viewport offset**, never the controls' stored logical coordinates or saved JSON bounds.
- Include nested scrolling containers, wheel handling, focus navigation/scroll-into-view, TabControl pages, and resizes; avoid feedback loops where a scrollbar changes the viewport enough to require another scrollbar.
- Form scrolling should work in both genuine runtime/Preview and the designer's client host. Keep the designer workspace's own scrollbars/zoom separate from the Form's runtime content scrolling.
- Do not conflate scrollable content extents with stored Form client Width/Height. Min/max bounds and DPI scaling must remain correct.

### 4.3 Button ModalResult — VCL-like dialog behavior

Add a **ModalResult** property to Button, serializable and editable through the property inspector. It must behave like a VCL `TButton.ModalResult` when used inside a modal Form:

- `mrNone`/zero means clicking the button does not automatically finish the modal dialog.
- A nonzero result (e.g., `mrOk`, `mrCancel`, `mrYes`, `mrNo`, etc.) requests completion of the **containing modal Form**, not closure of an enclosing Panel or Frame. Resolve the containing Form through logical ancestors, without violating frame namespace boundaries for lookup or altering parentage.
- The normal button `OnClick` callback runs first; if it destroys/changes the button or Form or invalidates completion, handle that safely. After the event, a still-valid nonzero result triggers modal completion via the runtime's established modal-result and close path. Avoid sending duplicate `OnClick`, close, or destroy events.
- Respect Form `OnCloseQuery` cancellation and report modal results coherently. If closing is cancelled, keep the dialog open and reset or otherwise reconcile the pending completion value so a subsequent interaction behaves correctly. **Codex must verify and document the exact chosen ordering and compatibility with current `ShowModal()`/`Close()` implementation.**
- Programmatic modification of Button.ModalResult does **not** automatically close the dialog. Buttons with nonzero ModalResult clicked on a nonmodal Form should **not** silently destroy the Form; document that contract.
- Reuse the existing Form runtime-only `ModalResult` property/behavior rather than inventing a second result system. Do not serialize the result of a completed modal session as design-time Form data.
- Preview must demonstrate a modal Form containing OK/Cancel buttons, show the returned result in the designer status/log, and handle both cancellation and errors.

## 5. Preview behavior (Forms only)

**Preview is a real, modal runtime window**, not simply hiding selection handles on the existing design surface.

1. Serialize/clone the **current unsaved in-memory Form** to a separate definition object. Do not mutate the active designer document.
2. Create a new, independent runtime Form using the supported runtime construction path and normal top-level HWND styles; use **Preview context mode** as needed to suppress application/QuadooScript callbacks that are unavailable in the standalone designer while retaining native input and built-in behavior.
3. Invoke `IQuadooForm::ShowModal()` (not just `ShowForm()`), passing the standalone designer window as owner as supported by the runtime. The designer should be disabled/restored appropriately by the modal implementation.
4. Window caption, border/resizing behavior, icon, window-state policy, scrolling, and Button.ModalResult must all reflect current property values.
5. Closing through the native system close button or through an appropriate button returns control to the editor. Display resulting modal return value/error without changing the design session.
6. Repeated previews must not leak HWNDs, GDI objects, image handles, factories, or event bindings. On malformed JSON or native creation failure, report a useful error and keep the design session intact.
7. Disable Preview when editing a Frame; a Frame is not a standalone dialog and must not be temporarily masqueraded as a Form just for preview.

Clarify `ShowModal()` return conventions with the actual existing header (HRESULT plus an output `LRESULT`) before implementation. The standalone designer must not create or modify a QuadooScript object for preview.

## 6. Objects tree, toolbox, and selection synchronization

### 6.1 Objects tree (upper-left)

Use native `SysTreeView32` with a root node for the current Form or Frame and children corresponding to the **logical component containment hierarchy** (not arbitrary HWND ancestry). Examples:

```text
MainForm (Form)
  pnlCustomer (Panel)
    lblName (Label)
    txtName (TextBox)
  tabsMain (TabControl)
    tabGeneral (TabPage)
      btnSave (Button)
    tabAdvanced (TabPage)
```

- Include root itself; selecting it edits root properties and events.
- Expand/collapse nodes; preserve expanded state and selection where practical on rebuild.
- Synchronize selection bidirectionally: tree -> canvas -> inspector and canvas -> tree -> inspector.
- Selecting a control on an inactive TabPage must activate that page in **design mode**, without raising application selection events. Scroll the control into view when possible.
- Support invisible, overlapped, tiny, or clipped controls via tree selection. Show descriptive names/types and prevent stale pointers after delete or reconstruction.
- The tree reflects runtime-defined child relationships. **Do not enumerate separate runtime-attached Frame instances as if they were serialized children of a Form definition.** The standalone editor edits a Form or Frame definition in isolation.
- Preserve stable selection by the persistent component GUID, scoped to the current definition instance. Treat name changes as edits, not identity replacement.

### 6.2 Toolbox (lower-left)

Populate from the registered **control factories**, not a hardcoded list or `Kind` enum. Offer Pointer/Select plus eligible control types; indicate which parent/container kinds accept each type. Exclude creation of a Frame as a declarative child; the File > New > Frame action edits its separate definition. TabPage insertion must obey TabControl's child contract.

Support click-a-tool then place, and drag-from-toolbox then drop, as practical. A new control receives a fresh GUID, unique default name, validated parent, and reasonable default bounds. Creation uses the external factory/construction system and logical container registration.

## 7. Selection overlay, dragging, and resizing

### 7.1 Overlay and eight resize handles

Use a **designer-owned interaction/selection layer** independent of individual controls, capturing selection, hit-testing, drag, and resize interactions without triggering application callbacks. Draw a selection rectangle and **eight grab handles**:

```text
    o-----------o-----------o
    |                       |
    o    selected control   o
    |                       |
    o-----------o-----------o
       NW   N   NE / W E / SW S SE
```

Precisely the four corners and four edge midpoints. Cursor shapes reflect the available resize directions. The selected root Form/Frame should also be resizable from its design outline (the Form simulated chrome is included in *rendering*, but stored dimensions stay client-based).

### 7.2 Resize capabilities and constraints

Do **not** assume every control supports arbitrary horizontal and vertical resizing. The designer should determine restrictions from descriptor/capability metadata or an explicit optional design-time capability interface, not from hardcoded comparisons against control type names.

- An ordinary button/panel/textbox generally allows two-axis resize, subject to minimum/maximum sizes, `Align`, `Anchors`, and `AutoSize`.
- Some ComboBox styles have OS-managed height. For a traditional non-owner-draw standard Win32 ComboBox, **disable vertical resize grips or constrain their behavior** if the visible height cannot be meaningfully changed; allow horizontal resizing. Different styles may require distinct restrictions.
- Labels with AutoSize may have effective dimensions determined by text; the inspector and overlay should show actual effective geometry while keeping the stored requested geometry distinct.
- Docked controls may have one or both effective dimensions controlled by `Align`; show constrained handles or provide a clear inspector message rather than offering misleading edits.
- Never allow negative/invalid dimensions, violate hard min/max constraints, or write out-of-range logical coordinates. Snapping, Shift/Alt modifiers, and aspect-lock policies can be added later.

Constrain drag/resize by the **control's documented capabilities** and reflect them visually (disabled handles, cursor and inspector behavior). Do not force native Win32 windows into unsupported heights simply because the JSON `Height` value changed.

### 7.3 Moving, containment and immutable parentage

Dragging within the same parent adjusts stored logical `Left`/`Top` as allowed by layout. Dropping over another container must **not call `SetParent()`**. Such a move is a reconstructive operation: serialize the selected subtree; validate the target; instantiate under the new parent with appropriate coordinate conversion; commit only if successful; then destroy the original. Follow the agreed policy for IDs when moving across containers (a reconstruction is a new component identity unless an explicit safe-preservation contract is approved). Clipboard duplication always assigns fresh IDs.

A drop target should be visually indicated. Correctly target nested Panels, ScrollingPanels, GroupBoxes if they truly support logical containment, and TabPages; avoid treating mere decorative borders as actual containers. Selection/grips need to remain visible even when native child HWNDs cover parts of the workspace.

## 8. Property and event inspector

- Populate editable properties/events using `IQuadooProperties`, property/event descriptors, and registry metadata. Do not duplicate every built-in control's property set in the editor.
- Display `Name`, `Caption`, requested/effective geometry, layout, font, color, Boolean, enum/flags, collection and icon values with suitable editors. Read-only and runtime-only properties must not be writable through the designer.
- The **Form Icon editor** supports browsing `.ico`, previewing the chosen icon, replacing/removing it, validating and saving the Base64 representation. The simulated caption updates immediately.
- Provide appropriate editing for ComboBox/ListBox `Items` (leveraging the agreed `IRStringArray` snapshot interface), selected tab/page properties, Button.ModalResult, Form border/style/window-state/scrolling properties.
- Event pane shows logical event names and saved handler names. Preserve event entries through serialization even before a QuadooScript adapter is available. Designer mode should not insist that unresolved application callback names exist.
- Property changes synchronize the canvas, Objects tree, status bar, and modified flag, with batching and rollback for invalid changes.
- The eventual Quadoo Studio integration owns generated QuadooScript declarations/handler stubs. **The standalone first phase does not need to generate or run QuadooScript.**

## 9. Persistence, document state, and validation

- Open/save the existing versioned Form/Frame JSON using the runtime serializer and the specified `CChooseFile` / `TextHelpers` utilities. Preserve persistent component GUIDs, names, event bindings, icon bytes and supported properties.
- Preserve unknown editor-owned metadata where the runtime's Design context permits it. Do not serialize effective bounds, designer zoom, panel splitters, workspace scroll, selected component, native HWNDs, or modal preview results as Form geometry/state.
- Use deterministic child order/descriptor ordering and successful round trips. On failed load, keep the active document unchanged. On failed save, do not clear its dirty flag. Save As updates the document path only after a successful write.
- Track document path, dirty state and root kind (Form/Frame). Prompt about unsaved edits on New/Open/Exit; Preview never silently saves and should never clear the dirty flag.
- Name uniqueness and GUID uniqueness are scoped to the current Form/Frame definition; invalid edits must roll back atomically. Controls inside inactive tabs and hidden containers are still serialized.
- Define size limits/error handling for Base64 and JSON, and ensure non-ASCII captions/items survive file round trips.

## 10. Reusable designer architecture

The standalone executable should be a **host** around a reusable, embeddable native design component. Keep core designer logic separate from standalone menu/file plumbing so Quadoo Studio can later embed the same surface in a document tab.

Suggested internal modules (illustrative, not imposed class names):

- `CDesignerDocument`: current cloned JSON, root identity, path/dirty state, save/load, undo boundary.
- `CDesignerSurface`: child-window host, zoom/scroll transforms, preview of design-mode clients, coordinate conversion, selection overlay and grips.
- `CDesignerWindowFrame`: artificial Form chrome, theme/GDI fallback and Form icon rendering.
- `CDesignerObjectsTree`: hierarchy view and selection synchronization.
- `CDesignerToolbox`: factory-driven palette and target validation.
- `CDesignerProperties`: descriptor-driven property and event editing.
- `CDesignerHost`: standalone menu, toolbar, splitters, status bar and modal Preview entry point.

These are **responsibilities**, not a demand for a particular inheritance graph. Use COM-style interfaces where needed for host/editor integration, preserve actual QuadooForm.dll ABI and prefer an internal designer API until it is proven. The designer should observe component changes instead of maintaining an unrelated shadow representation that can diverge from the form hierarchy.

## 11. Suggested implementation phases and boundaries

### Phase A — Runtime prerequisites and design contract

1. Inspect current published header and implementations for Form icon, Button.ModalResult, Form BorderStyle/BorderIcons/WindowState/FormStyle, Form AutoScroll, ComboBox resize constraints, TabControl/TabPage handling, and runtime modal semantics.
2. Propose narrow descriptor/interface extensions where missing, including Base64 Icon schema and resize-capability reporting. **Explain consequential public ABI additions before changing them.** Do not revert the current extensible factory hierarchy.
3. Implement and test necessary runtime support independently in the native harness first (including actual Form Icon display, modal OK/Cancel buttons, Form scrolling, and XP behavior).
4. Inspect `CChooseFile` and `TextHelpers` APIs and project patterns; settle JSON file extensions/encoding.

### Phase B — Standalone designer minimum useful version

1. Native app with standard File/Edit/View/Form menus, toolbar Preview, resizable panes and status bar.
2. New Form/New Frame, Open/Save/Save As, dirty-state prompting and JSON error handling.
3. Design host with artificial, icon-capable Form chrome and undecorated Frame root.
4. Objects tree (top left), Toolbox (bottom left), descriptor-based inspector (right), synchronized selection.
5. Add/select/move/resize controls, eight grips, capability-aware restrictions and proper nested TabPage targeting.
6. Preview from unsaved Form JSON via genuine modal runtime Form using `ShowModal()`; Preview disabled for Frames.
7. Interactive harness/demo and automated tests for editor workflows and rollback.

### Phase C — Designer usability

Add keyboard editing, grid/snap, selection cues, multi-selection where safe, clipboard, reconstructive paste/move, undo/redo, alignment/order operations, context menus, property collection editors and improved diagnostics. Preserve the initial architecture's ability to support these rather than hard-coding away extensibility.

### Phase D — Quadoo Studio and scripting (later, separately authorized)

Embed the reusable designer in Studio document tabs; add source navigation and automatic synchronization of QuadooScript class members and event-handler declarations. Subsequently implement the QuadooScript runtime adapter using existing `GetInterface` / `ResolveMethod` / ordinal-based `Invoke`. No changes to QuadooScript VM are implied by Phases A/B.

**Deferred:** ListView, optional ImageList/ComboBoxEx, advanced control catalog, full themed cross-version chrome emulation, form inheritance, and unrelated IDE refactoring.

## 12. Acceptance tests

Codex should extend the VC9 Win32 native/interactive test suite. Minimum cases:

1. VS2008 Debug/Release compile with no warnings/errors; native harness passes on Windows XP VM and a current Windows host where available.
2. New Form shows artificial caption/border; New Frame shows **no** artificial non-client region. Selecting either root exposes its properties.
3. Menu New/Open/Save/Save As preserve GUIDs/names/events/geometry; invalid JSON fails without replacing the current document; dirty prompts function.
4. Objects tree correctly represents Panel and TabPage containment; selection in either tree or canvas updates the other and the inspector; inactive page selection activates it in Design mode.
5. Eight resize handles appear and hit-test correctly at all corners/edges. Resize restrictions for ComboBox, docked, minimum-size and AutoSize controls are enforced, including at non-100% designer zoom.
6. Factory-provided custom controls can appear in toolbox/tree/inspector and be selected, saved and recreated without designer-specific control-type switches.
7. Form icon picker loads a valid ICO; simulated caption changes; JSON contains Base64 bytes; save/reload reproduces identical icon; genuine Preview uses it; corrupt/oversized Base64 is rejected without leaking handles.
8. Changing Form caption, border/style/buttons, resizability and initial window state reflects both the simulated representation (where appropriate) and a real Preview window.
9. A small Form with overflowing children scrolls in Preview using the same logical scrolling behavior as ScrollingPanel; saved child coordinates do not drift after scroll/DPI/resize.
10. Form Preview opens a separate **modal** top-level Form from unsaved JSON. The editor returns to its prior selection/layout/dirty state after closing. Frame Preview command is disabled.
11. Button.ModalResult=`mrOk` and `mrCancel` finish Preview with the expected result; `mrNone` does not. Modal close cancellation preserves the dialog; event ordering is documented and tested. Nonmodal buttons do not accidentally close their forms.
12. Repeated Preview/icon replacement/open/close produces no leaked component references, HWNDs, GDI/USER objects, observers or image handles after expected system cache warm-up.
13. Copy/reconstructive move (when implemented) respects immutable parentage and GUID policy, including target-validation failure rollback.
14. Design mode suppresses application script callbacks and does not require QuadooScript; Preview supports built-in interactions with application callbacks suppressed as agreed.
15. Physical DPI transitions and actual Windows theme differences get manual follow-up testing; automated 96/144/192 DPI tests are not a substitute.

## 13. Requested Codex process and deliverables

- **First produce a short feasibility/ABI review** identifying what is already implemented, what Form/Button/scrolling properties are missing, which existing interfaces suffice, where narrowly scoped additions may be needed, and the signatures of the `CChooseFile`/`TextHelpers` helpers. Distinguish verified repository facts from proposals.
- Preserve current QuadooForm runtime behavior, public GUID identity, JSON compatibility and factory extensibility. Do not build the designer on implementation casts or a central built-in-control type enum.
- Implement **Phase A**, then **Phase B**, in reviewable steps, with native build and test results after each. If an incompatible public ABI change is necessary, report it and request approval before applying that change. If a narrower additive contract suffices, document it.
- Supply the standalone executable, any reusable designer library/component, example JSON Form and Frame definitions, and user-facing build/run instructions.
- Report changed files, any changes to the Form/Icon/ModalResult JSON schema, test coverage, Windows XP results, known gaps, and any deferred work.
- **Stop at the Phase B milestone.** Do not automatically begin Quadoo Studio integration, QuadooScript source generation/runtime integration, ListView, or advanced designer features without a separate instruction.

---

### Suggested initial instruction to Codex

> Use `QuadooFormDesignerProposal.md` together with the current `Published/QuadooForm.h`, `NativeRuntime.md`, and the actual QuadooForm sources as the design authority for the standalone QuadooForm Designer. First inspect existing runtime support and the exact `CChooseFile` and `TextHelpers` APIs. Reconcile the proposed Form icon, Form style/scrolling, Button.ModalResult, and resizing-capability contracts with the current implementation. Then implement Phases A and B, preserving Windows XP/VS2008 support and public ABI wherever possible. Build and test on the available environments, and stop after a working standalone designer and modal Preview are demonstrated. Do not begin Quadoo Studio or QuadooScript integration.
