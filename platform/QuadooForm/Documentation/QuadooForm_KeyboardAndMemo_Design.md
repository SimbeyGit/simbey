# QuadooForm — Form Keyboard Preview, Default/Cancel Buttons, and Memo Control

**Implementation proposal for Codex**  
**Date:** October 16, 2026  
**Status:** Proposed implementation contract; review existing published interfaces and code before modifying ABI.

## 1. Context and objectives

The standalone QuadooForm designer now has an Objects tree, dynamically populated Toolbox, embedded Form/Frame design surface, eight resize grips, Properties/Events inspector, nonvisual component tray, JSON persistence, and modal Preview. The native runtime is designed for Visual Studio 2008 (VC9), Win32, and Windows XP onward, using the repository's existing `RSTRING`, JSON, SimbeyCore, COM-style interfaces, custom containers, and HRESULT conventions. No STL or C++ exceptions should be introduced.

This proposal adds three related capabilities:

1. **`Form.KeyPreview`** — when enabled, the Form's keyboard events are offered the relevant input before the currently focused descendant handles it, with an explicit way to mark the input handled.
2. **`Button.Default` and `Button.Cancel`** — Enter and Escape can activate eligible buttons anywhere in the Form's *runtime descendant tree*, including frames nested within frames. A button may have both properties enabled.
3. **`Memo`** — an independently registered, multiline edit control, including `ES_MULTILINE` and `ES_WANTRETURN`, available in the designer Toolbox.

Maintain all current behavior and ABI wherever possible. Prefer additive properties and optional interfaces. Continue deferring QuadooScript runtime and Quadoo Studio integration; all behaviors must be exercisable in native C++ and modal Preview with application callbacks suppressed as appropriate.

### Important semantic qualification

The requested **Form-only KeyPreview** broadly follows VCL, but VCL's own documentation says navigation keys and Enter used by buttons can bypass KeyPreview. Do **not** promise that KeyPreview intercepts literally every key or that it supersedes native dialog-navigation semantics. Preserve the distinction between (a) a logical key event offered for preview and (b) a dialog command handled by Enter/Escape arbitration. Define, document, and test the order below. If exact VCL behavior conflicts with an established QuadooForm event contract, explain the difference before changing it.

## 2. Initial visual-designer observations

The supplied designer screenshot shows the expected broad composition: Objects tree over Toolbox at left; embedded Form with simulated non-client frame and resize grips in the center; Properties and Events at right; separate nonvisual component tray below the form. Keep this layout. The selected Form currently displays **`Visible = false`** in the property inspector. Audit whether this is intentionally stored document state, a design-host implementation detail, or an incorrect default: the designer must not silently change the persisted `Visible` value just to host a Form. This observation is a check, not proof of a bug.

The new properties must appear in the existing descriptor-driven property inspector, survive save/load, and be available in both standalone designer and runtime construction. `Memo` belongs under **Visual Controls**; neither Form nor Frame belongs in the Toolbox.

## 3. API/property/event additions

### Form

- `KeyPreview`: Boolean, **default `false`**, serializable; available on `Form` only, **not Frame**, Panel, TabPage, or other containers.
- Use existing `OnKeyDown`, `OnKeyPress`, `OnKeyUp` logical events if their descriptors already exist. Avoid duplicate descriptor names or changing unrelated event ordinals unnecessarily.
- Event arguments must expose appropriate key information and mutable handling state. Use existing typed `IQuadooEventArgs` and the `Handled` Boolean where supported; verify the current arguments for key events rather than guessing their exact names and types.
- The Form must not become the keyboard focus target merely because KeyPreview is true; original focus stays with the descendant.

### Button

- `Default`: Boolean, **default `false`**, serializable.
- `Cancel`: Boolean, **default `false`**, serializable.
- Both may be true on one Button.
- Maintain existing `ModalResult` behavior: keyboard activation must take the **same logical click path** as mouse activation (`OnClick` first, then modal-result completion as already specified). No duplicate `OnClick`, and no auto-close when the Form is nonmodal.
- The flags are **independent from** `ModalResult`. A default/cancel button may have `ModalResult = 0`; an ordinary click handler may perform its action without closing the Form.
- A native button style such as `BS_DEFPUSHBUTTON` can reflect the current effective default for visuals, but must not be treated as the sole behavioral implementation, especially for nested frames and dynamic focus changes.

### Memo

- New registered control type: JSON `"type": "Memo"`, displayed as **Memo** in the Toolbox.
- It is a regular HWND-backed visual control using Windows `EDIT`; **not** a property descriptor merely because an existing property type is named `Type::Memo`. Confirm what `Type::Memo` means in the current code; it may be useful for designer multiline *property editing*, but it does not itself instantiate a multiline text control.
- Initial properties: inherited geometry, font, visibility, enabled, tab order, anchors/align, `Text`, `ReadOnly`, `WordWrap`, `ScrollBars` (None/Horizontal/Vertical/Both as applicable), and optionally `MaxLength`/`WantTabs` if the current TextBox architecture supports them cleanly. Prefer shared text-edit logic with TextBox over copying the entire class.
- Default styling: `ES_MULTILINE | ES_WANTRETURN | ES_AUTOVSCROLL | WS_VSCROLL` with appropriate styles derived from `WordWrap` and `ScrollBars`; default `WantTabs=false` so Tab ordinarily navigates, not inserts a tab character. Avoid adding conflicting horizontal scrolling when WordWrap is enabled.
- `OnChange` and applicable keyboard/focus events use the same logical event conventions as TextBox.

## 4. Keyboard dispatch architecture

### 4.1 One Form-scoped dispatcher

Implement one coherent dispatch/arbitration path per **active top-level Form**. Do not independently hook each Frame as if it were a Form, and do not rely on `GetParent(hwnd)` alone to discover a logical ancestor: scrolling content HWNDs, TabPages, edit subwindows, and custom/native control implementations introduce intermediary windows. Walk or query the logical QuadooForm containment/ownership graph using public interfaces and supported internal services. Map actual focus HWNDs to the corresponding logical control and owning Form.

The dispatcher should process keyboard input **at most once** per original message. The hosting application owns the primary message loop; ensure the runtime's modal loop and standalone Preview use equivalent translation/preprocessing rules. If there is a host-facing keyboard preprocessing hook, document precisely where it is called relative to `TranslateAccelerator`, `IsDialogMessage`, `TranslateMessage`, and `DispatchMessage`. Avoid global keyboard hooks and unrestricted thread-wide interception.

### 4.2 Suggested precedence

For input sent to a focused descendant of the current Form:

1. Resolve current Form, logical focus target, and any active control that explicitly claims the key (`WM_GETDLGCODE`, `DLGC_WANTMESSAGE`, `DLGC_WANTALLKEYS`, `DLGC_WANTRETURN`, text-composition state, or equivalent custom-control capability).
2. For **ordinary key messages** that are eligible for preview, if `Form.KeyPreview=true`, invoke the Form's relevant `OnKeyDown`/`OnKeyPress`/`OnKeyUp` handler before dispatch to the focused control. If it marks the message `Handled`, do not deliver that stage to the target or activate a default/cancel button. If unhandled, allow the target to receive it normally.
3. For **Enter and Escape as dialog commands**, honor a focused control's explicit claim first; otherwise resolve eligible Default/Cancel Button (section 5) and activate it once. If no eligible button exists, preserve existing Win32/dialog behavior.
4. Do not bypass menu accelerators, system keys, Alt+F4, IME composition, or normal Tab and arrow-key navigation. Preserve native control behavior unless a documented QuadooForm event or property explicitly changes it.

**Ordering detail to resolve before coding:** VCL's documented exception for Enter on buttons means an Enter routed as a dialog command ordinarily bypasses KeyPreview. The recommended QuadooForm behavior is to retain that exception, rather than sending both a preview event and a default click for the same Enter. If the existing QuadooForm pipeline has a different correct precedence, record it and ensure it remains deterministic and single-dispatch.

### 4.3 OnKeyDown, OnKeyPress, and OnKeyUp are distinct

- `OnKeyDown`/`OnKeyUp` represent virtual-key transitions; include modifiers/repeat where already available.
- `OnKeyPress` represents **character input**, not every virtual key. Support Unicode appropriately in the existing Win32 Unicode application. Do not synthesize character events for arrows/function keys.
- Do not assume suppressing `WM_KEYDOWN` automatically removes a `WM_CHAR` already queued/generated; track consumed key sequences where necessary and test both stages.
- Avoid consuming the corresponding `WM_KEYUP` merely because a prior key-down was handled unless this is a deliberate, documented policy.
- Consumed key messages must not be processed twice by native subclass handlers or by `IsDialogMessage`.
- If a callback destroys the focused control or Form, hold safe temporary references and re-check state before dispatch, selection, or activating buttons.

### 4.4 KeyPreview disabled

With `KeyPreview=false`, the Form must not receive copies of child-targeted ordinary keyboard events through the preview path. It should still receive its own keyboard events when legitimately targeted, and its Default/Cancel button resolution should still function.

## 5. Default and Cancel button arbitration

### 5.1 Search scope includes runtime-attached Frames

Eligible Buttons may be nested in Panels, GroupBoxes, ScrollingPanels, TabPages and **runtime-attached Frames, including Frames inside Frames**. Search by logical descendant relationship from the active Form, crossing Frame namespace boundaries **for keyboard routing only**. Do not change the rule that `FindControl` stops at Frame boundaries or that runtime-attached Frames are omitted from Form JSON serialization.

### 5.2 Candidate eligibility

A candidate Button must:

- be alive and part of this Form's current runtime subtree;
- have the relevant `Default` or `Cancel` flag true;
- be effectively visible and enabled, considering its entire ancestor chain;
- belong to the active TabPage at every tab boundary (do not activate a button on a hidden/inactive page);
- not be on another window, inactive modal Form, or detached subtree.

Reevaluate as focus, ancestors, active pages, flags, tab order, or component hierarchy change. Do not cache raw pointers or HWNDs without proper lifetime protection/invalidation.

### 5.3 Multiple eligible buttons: deterministic rule

Implement the following **QuadooForm-specific policy**, unless an exact VCL-compatible rule is verified and adopted explicitly:

1. If a **focused eligible Button** could receive Enter or Escape itself, give it normal native activation priority. In particular, Enter on a focused non-default Button should not unexpectedly activate a different default Button.
2. Otherwise favor a candidate within the deepest active **logical focus scope** (the nearest Frame/container ancestor holding the focused descendant); search outward through nested Frames/containers to the Form. This scopes reusable Frames naturally.
3. Within the winning scope, choose the first eligible candidate in deterministic **effective tab-navigation order**; when order ties, use stable container registration/definition order.
4. If no candidate is in the focused scope, use the first eligible candidate elsewhere in the active Form subtree under that same stable ordering.

This is a recommended policy, not a claim about VCL's exact resolution of competing buttons. **Do not invent uniqueness constraints**: permit multiple Default or Cancel flags, allow one button with both, and test precedence explicitly. If the author prefers VCL's precise behavior, identify the behavioral difference before implementation.

### 5.4 Enter: Memo and other input controls

`ES_WANTRETURN` is intended to let a multiline EDIT use Enter itself. When a focused Memo has `ES_WANTRETURN`/`DLGC_WANTALLKEYS` or reports a comparable claim, **Enter inserts a newline (unless read-only/other native conditions apply) and must not trigger the Form's Default Button**. In particular:

- Enter on a single-line TextBox normally activates the Default Button when no special claim exists.
- Enter on a Memo inserts a newline by default and leaves Default Button untouched.
- Enter on a focused Button activates that Button via its normal click path, not a second default candidate.
- Editable ComboBoxes, custom controls, multiline controls in unusual modes, and any IME composition must retain their native/input-method priority.
- Ctrl+Enter, Shift+Enter, Alt+Enter and other combinations should not be blindly treated as ordinary Enter; choose documented rules and test them.

**Implementation caution:** `ES_WANTRETURN` alone is not a complete dialog-message policy. Inspect `WM_GETDLGCODE` and the current `IsDialogMessage` preprocessing behavior; test actual Windows XP and modern Windows input paths.

### 5.5 Escape

Escape activates a qualifying Cancel Button when the focused control has not explicitly claimed Escape (e.g., a ComboBox with an open dropdown, an active custom popup, an in-progress IME sequence, or another native control that needs Escape for itself). The Cancel property does not by itself assign `mrCancel`; `ModalResult` determines whether the activation closes a modal Form. A Button with both Default and Cancel can be activated by either key, but only once per keystroke.

### 5.6 User-initiated versus programmatic clicks

Enter/Escape activation should use the same logical path as a user Button click. Setting `Default` or `Cancel` programmatically must not invoke an event. Native double-notification or reentrancy must not cause duplicate `OnClick` or modal completion. Form close-query cancellation retains the modal loop and restores the pending modal result according to existing runtime semantics.

### 5.7 Visual treatment

Where supported, visually distinguish the current effective Default Button using the appropriate native button drawing state (`BS_DEFPUSHBUTTON`, `BM_SETSTYLE`, `DM_SETDEFID`, or framework-managed equivalent as appropriate for a non-dialog top-level Form). Do not mutate serialized `Default` values merely because focus changes. Do not require setting a Win32 dialog ID as the sole source of truth for nested Frames.

## 6. Memo control behavior

### 6.1 Construction and properties

Build Memo as a dedicated native component type/factory with common text-edit code shared with TextBox. Use a multiline `EDIT` HWND with `ES_MULTILINE`, `ES_WANTRETURN`, and a vertical scrollbar initially. Respect existing name/ID, immutable parent, DPI, layout, JSON, font, lifecycle, focus, event, and property-notification contracts.

The Memo's default height should be suitable for multiple lines rather than the current single-line TextBox height. Provide appropriate horizontal/vertical scrollbar semantics, `WordWrap` default true, and read-only mode if practical. Preserve line breaks in JSON as a string and round-trip them without losing characters. Normalize CR/LF only at the Win32 edit-control boundary where necessary, preserving the logical text contract. Clearly document the chosen text newline representation.

The designer should permit resize grips appropriate for a multiline editor, including vertical resizing (unlike height-constrained single-line ComboBoxes). Support design mode without executing application callbacks.

### 6.2 Tab, Enter and keyboard handling

- Enter inserts a newline, normally consuming the dialog command.
- Tab navigates to the next control by default unless an explicit supported property permits tab characters.
- Ctrl+A, clipboard shortcuts, selection movement and IME input should work as normal for a native multiline Edit.
- Avoid routing every edit child/`WM_CHAR` into the Form twice.
- No accidental Default/Cancel activation while an input control is legitimately consuming the key.
- `OnChange` fires once for a logical user edit, with the property state already reflecting the change. Programmatic changes should follow the framework's established event-notification policy.

### 6.3 Designer property type versus visual control type

`QuadooProperty::Type::Memo` (or the actual enum spelling in the repository) is a **metadata/editor property type** if used that way in current code. Do not assume it produces a Memo HWND. The visual **Memo** factory and an optional multiline property editor can share utility code but remain separate abstractions.

## 7. JSON examples

### 7.1 Form and buttons

```json
{
  "schemaVersion": 1,
  "type": "Form",
  "name": "OptionsForm",
  "properties": {
    "Caption": "Options",
    "KeyPreview": true,
    "Width": 500,
    "Height": 340
  },
  "events": {
    "OnKeyDown": "OptionsForm_OnKeyDown"
  },
  "children": [
    {
      "type": "Memo",
      "name": "NotesMemo",
      "properties": {
        "Left": 20, "Top": 20, "Width": 450, "Height": 220,
        "Text": "First line\r\nSecond line",
        "WordWrap": true
      }
    },
    {
      "type": "Button",
      "name": "OKButton",
      "properties": {
        "Caption": "OK", "Left": 300, "Top": 270,
        "Width": 80, "Height": 28,
        "Default": true, "ModalResult": 1
      },
      "events": { "OnClick": "OKButton_OnClick" }
    },
    {
      "type": "Button",
      "name": "CancelButton",
      "properties": {
        "Caption": "Cancel", "Left": 390, "Top": 270,
        "Width": 80, "Height": 28,
        "Cancel": true, "ModalResult": 2
      }
    }
  ]
}
```

The numeric `ModalResult` values in this illustration assume the existing runtime uses `1` for OK and `2` for Cancel; Codex must verify its actual published constants and replace the example if needed. A runtime-attached Frame with nested default/cancel buttons is **not** embedded in this Form's saved `children` array.

### 7.2 Nested Frames

At runtime, creating `FrameA` within a Form and `FrameB` within `FrameA`, then creating `ButtonX` inside `FrameB`, must make `ButtonX.Default=true` participate in Form-level Enter arbitration when eligible. Neither `FrameA` nor `FrameB` gains a `KeyPreview` property; their independent event binders and component namespaces remain unchanged.

## 8. Designer integration

- Add `KeyPreview` to **Form** property descriptors (Boolean, default false). Do not expose on Frame.
- Add `Default` and `Cancel` to **Button** property descriptors (Boolean, defaults false). Preserve existing `ModalResult`.
- Register **Memo** in factory enumeration and Toolbox under Visual Controls. Instantiate via existing design-mode pathways, not special-case hardcoded palette creation.
- Objects tree displays Memo and allows selection, rename, deletion and property changes. The eight-grip overlay should honor the existing resize-capability metadata. Memo permits horizontal/vertical sizing; single-line ComboBox restrictions remain unchanged.
- Properties and Events panels use descriptors and typed editors. If Type::Memo is a multiline editor descriptor, use it appropriately for long string values; do not conflate it with the Memo control class.
- Save/load and undo-ready document mutation must preserve IDs, names, event handler strings, properties and unknown Design-mode fields.
- The designer itself may implement editor-level keyboard shortcuts (delete, arrows, grid movement, etc.), but **Design mode must not execute the Form's application KeyPreview handlers or activate Default/Cancel buttons**.
- **Preview** creates an independent real top-level Form using current unsaved JSON, runs `ShowModal()`, and suppresses application script callbacks as currently specified. Native Default/Cancel activation and Memo editing must work during Preview. For a preview without an event binder, default/cancel + ModalResult behavior must still be testable.
- Frame definitions remain edited without simulated non-client chrome. Frame preview is not a standalone top-level dialog.

## 9. Native host and API compatibility

Inspect the current message pump, `IsDialogMessage`, control subclassing, `IQuadooMessageHandler`, `IQuadooEventArgs`, `ShowModal`, `Button.ModalResult`, and factory metadata before choosing hook points. Establish one host/runtime contract for keyboard preprocessing so a custom host doesn't lose KeyPreview/Default/Cancel semantics that work inside the test harness. Ensure standalone designer Preview, normal Form display, and modal Form display use the same logical routing algorithm.

A host may need to call a new **optional** keyboard preprocess interface/function before translating/dispatching messages; prefer that over changing existing vtables or intercepting all messages globally. If implementation is possible transparently through the current native dispatch infrastructure, preserve existing exports. If a new host-facing API is unavoidable, document its ordering obligations and adopt it in all native hosts/harnesses.

Test backward compatibility with existing control factories, external custom control DLLs, SimbeyCore, the VS2008 solution, and previous JSON definitions. No new required QuadooScript interface or dependency.

## 10. Acceptance tests

Tests should use actual keyboard messages/input where practical, not only direct calls into helpers. Run clean **Win32 Debug and Release** builds, existing harness and designer suites, and repeat the relevant interactive tests on **Windows XP**. Add focused test cases:

1. Form `KeyPreview=false` keeps ordinary child keyboard input from Form handlers; `true` routes the eligible input to Form before child.
2. Form preview handler sees sender/keys/modifiers correctly, can set `Handled=true` to block intended child processing, and `Handled=false` preserves it.
3. `OnKeyDown`, `OnKeyPress` (characters), and `OnKeyUp` are not duplicated or incorrectly conflated; no duplicate `WM_CHAR` or native notifications.
4. KeyPreview leaves Tab, arrow navigation, accelerators and system keys working. Enter dispatched as a dialog command follows the documented VCL-like exception.
5. Enter activates one eligible Default Button from a focused single-line TextBox, including buttons in nested Frames; Escape activates one eligible Cancel Button.
6. A Button with both flags handles either key; two eligible defaults/cancels follow the documented deterministic precedence; inactive TabPages, invisible/disabled descendants and unrelated Forms are excluded.
7. Enter on a focused nondefault Button activates **that Button**; Enter in a Memo inserts a newline and does **not** activate Default; test `ES_WANTRETURN` and `WM_GETDLGCODE` on XP.
8. Escape closes an open ComboBox dropdown (or is handled by other explicitly claiming controls) before considering Cancel, where supported by native behavior.
9. Button keyboard activation produces exactly one OnClick; `ModalResult` is applied after OnClick, cancellation preserves the Form, nonmodal Form does not auto-close.
10. Focus, tab ordering, active TabPage, component deletion, and dynamic Frame attachment/destruction cause candidate recomputation without use-after-free.
11. A callback can destroy sender, candidate Button, ancestor Frame, or the Form without duplicate dispatch or stale-window access.
12. Memo multiline text, CR/LF JSON roundtrip, Unicode, read-only, scrollbar, resize, focus, navigation, notifications, and resource cleanup work.
13. Designer inspector edits, new Memo Toolbox insertion, Objects selection, eight-grip resizing, JSON round trips, and modal Preview are correct.
14. No app callbacks execute in Design mode; modal Preview still supports native button completion and Memo input.
15. Existing third-party controls do not require inheritance from private runtime classes, and can explicitly claim Enter/Escape through supported native message contracts.
16. Run leak/handle-count and reference-lifetime tests under repeated Form/Frame/Memo creation and destruction in the existing test framework.
17. Verify the designer's displayed `Visible=false` for a new Form is not caused by an unintended persistent mutation when setting up Design mode.

Report assertion counts and distinguish tests executed on modern Windows from tests *actually executed* on XP; do not claim XP validation solely from compilation or simulated DPI values.

## 11. Implementation plan and scope

**Step A — Inspect and write a short technical decision note.** Identify the current message routing/host integration and confirm key-event arg semantics, inherited descriptors, existing control factories, style flags, and ModalResult constants. Explicitly settle the KeyPreview/Enter precedence and default-candidate selection policy before changing dispatch code.

**Step B — Add and test Button.Default/Cancel arbitration.** Keep support for deeply nested runtime Frames, inactive tabs, control claims to keys, focus changes, and modal click semantics.

**Step C — Add Form.KeyPreview** through the same shared keyboard-routing path. Ensure it doesn't break Tab, accelerators, ComboBox behavior or TextBox editing.

**Step D — Implement Memo** with shared text-edit internals where possible and without regressions to TextBox. Register factory, metadata, JSON support, resize capabilities and toolbox entry.

**Step E — Integrate with the standalone designer and expand tests.** Verify inspector, objects tree, design-mode suppression, preview, rollback and XML/project build configurations.

Do **not** implement ListView, image lists, Quadoo Studio, QuadooScript adapter, or unrelated redesigns. Avoid changing the established architecture or public ABI without explaining why.

## 12. Codex assignment (copy/paste)

> Please implement this proposal in the existing QuadooForm repository: `QuadooForm_KeyboardAndMemo_Design.md`.
>
> Add Form-only `KeyPreview` (default false), Button `Default` and `Cancel` (defaults false, both allowed), and a registered multiline Memo control using `ES_MULTILINE` and `ES_WANTRETURN`. Default/Cancel buttons must work through nested runtime Frames, including Frames within Frames, while respecting focused controls that claim Enter/Escape, TabPages, effective visibility, enabled state, reentrancy, and the existing Button.ModalResult lifecycle. Use one coherent keyboard-routing path rather than scattered special cases.
>
> Preserve VCL-inspired behavior, but explicitly document the precedence of KeyPreview versus Enter/Escape dialog commands, which have VCL-documented exceptions. Integrate Memo into the existing native designer Toolbox/Objects/Properties system, property descriptors, JSON, and modal Preview. Preserve compatibility with VC9/VS2008, Windows XP, SimbeyCore, existing custom factories, and current public interfaces wherever possible.
>
> Execute and extend Debug/Release tests and perform XP validation where available. Report architectural decisions, API changes, test results, and any remaining behavioral differences from VCL. Do not start later milestones or unrelated controls.

## 13. Technical references

- Embarcadero RAD Studio API: [TForm.KeyPreview](https://docwiki.embarcadero.com/Libraries/Athens/en/Vcl.Forms.TForm.KeyPreview) — form receives keyboard events before active control; default false; navigation keys and button Enter are documented exceptions.
- Embarcadero UI guidance: [VCL dialog Default, Cancel and ModalResult conventions](https://blogs.embarcadero.com/how-to-design-a-great-ui-part-3/).
- Microsoft Win32 documentation (verify against installed SDK 6.0A and Windows XP): `WM_GETDLGCODE`, `DLGC_WANTALLKEYS` / `DLGC_WANTMESSAGE`, `ES_MULTILINE`, `ES_WANTRETURN`, `IsDialogMessage`, and button styles.

