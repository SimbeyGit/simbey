# MainMenu, PopupMenu and MenuItem

The runtime, stand-alone Designer, Quadoo Studio and AQVMT share the same menu
component model. No existing published interface vtable changes. The new optional
interfaces are `IQuadooUniqueComponent`, `IQuadooMenuItems`, `IQuadooMenuItem`,
`IQuadooMenu`, `IQuadooPopupMenu` and `IQuadooFormMenu`.

## Designer workflow

Add MainMenu or PopupMenu from the tools palette. These components appear in the
nonvisual tray. A newly added MainMenu is automatically assigned to Form.MainMenu.
A Form may contain only one MainMenu component. Frames may contain PopupMenus;
they cannot contain MainMenus and never display a main menu bar.

Double-click Form.MainMenu or a visual component's PopupMenu value to open an
inline drop-down list of matching menus in the current form/frame. Choose
**(none)** to disconnect it. Escape cancels; choosing an entry or pressing Enter
applies the assignment. References are stored by component GUID, and the
inspector displays the component name.

The same editor handles package-defined ObjectReference properties, listing the
current document's components. MainMenu and PopupMenu additionally filter by the
published IQuadooMenu capability and menu kind. Existing unresolved values remain
unchanged until the user chooses another entry. Read-only descriptors remain
read-only. This editor requires no published ABI changes.

Double-click a menu component in the tray, or choose **Form > Menu Editor**.
The shared editor provides a hierarchical tree, Add Sibling, Add Child, Delete,
Up and Down. The fields edit Name, Caption, ShortCut and OnClick. Checkboxes edit
Enabled, Checked, Visible, Separator and AutoCheck. A separator cannot have children.
`&` in a caption specifies an access key. OK applies the complete edited definition
and Studio source transaction; Cancel leaves the original document untouched.

The assigned MainMenu's visible top-level captions appear beneath the simulated
title bar, with wrapping. This region is outside the logical client rectangle:
adding, removing or wrapping a menu does not change requested client dimensions
or child positions. The renderer uses the system menu font/metrics, optional
theme drawing, and a complete GDI fallback. Menu rendering and hit testing scale
with designer DPI and zoom.

Click a simulated caption to select that MenuItem; double-click to open its editor.
The Objects tree nests items beneath their menu parent. The normal Properties
and Events inspector edits the selected item, including inline OnClick names.
These design operations never invoke application delegates. MenuItem is omitted
from the palette and tray because its creation requires a menu parent.

Form.MainMenu and visual controls' PopupMenu properties have a menu-reference
picker. References display component names but serialize GUIDs. Deleting a menu
in the designer also removes its descendants and clears references to it. Copying
a menu branch copies all descendants as a JSON array; pasting remaps GUIDs and
names while retaining internal hierarchy. Reordering preserves GUIDs.

## JSON representation

Menus and MenuItems are ordinary nonvisual entries in the root `components`
array. Each has the normal immutable `id`, editable `name`, properties and events.
MenuItem adds a top-level immutable `menuParent` GUID. Its lifetime owner remains
the containing Form/Frame component collection, irrespective of menu nesting.
`Order` sorts siblings; equal values retain component collection order. Menu
reordering changes Order, never parentage.

```json
{
  "type": "Form",
  "name": "MenuForm",
  "properties": {
    "Width": 480,
    "Height": 300,
    "MainMenu": "{F3000000-0000-0000-0000-000000000001}"
  },
  "components": [
    {
      "type": "MainMenu",
      "name": "MainMenu1",
      "id": "{F3000000-0000-0000-0000-000000000001}"
    },
    {
      "type": "MenuItem",
      "name": "FileMenu",
      "id": "{F3000000-0000-0000-0000-000000000002}",
      "menuParent": "{F3000000-0000-0000-0000-000000000001}",
      "properties": { "Caption": "&File", "Order": 0 }
    },
    {
      "type": "MenuItem",
      "name": "OpenItem",
      "id": "{F3000000-0000-0000-0000-000000000003}",
      "menuParent": "{F3000000-0000-0000-0000-000000000002}",
      "properties": { "Caption": "&Open...", "ShortCut": "Ctrl+O" },
      "events": { "OnClick": "OpenItem_Click" }
    }
  ]
}
```

MenuItem defaults: Caption and ShortCut empty, Enabled/Visible true,
Checked/Separator/AutoCheck false, Order zero. MainMenu and PopupMenu share the
ordinary nonvisual Name and lifecycle events. OnClick uses normal event binding,
with the MenuItem as Sender. Full graph validation rejects missing or cyclic
parents, separator parents and invalid/wrong-scope menu references before user
initialization callbacks. Runtime main-menu construction also rejects duplicate
active shortcuts. The safety limits are 128 hierarchy levels and 4096 native items.

## Runtime and script behavior

Runtime and modal Preview create genuine HMENUs. Form.MainMenu attaches through
SetMenu. Menu changes replace the native snapshot while keeping the client
viewport dimensions, including when native menu rows wrap. If replacement or
layout fails, the prior menu is retained. During an active menu loop, existing
captions/enabled/checked states synchronize immediately; structural replacement
waits until the loop exits, preserving the handles Windows is tracking.

Preview suppresses application callbacks using the existing Preview contract.
Runtime OnClick dispatch uses the same delegate binding as other components.
AutoCheck toggles before OnClick. Disabled, invisible or separator items do not
click, and disabled/invisible ancestors suppress descendants. A submenu parent's
OnClick runs when its submenu opens. Callbacks may edit menus or close the form.

Named MenuItems become ordinary Studio-generated script members. The adapter
supports `Item.Enabled`, `Item.Checked`, `Item.SetEnabled(bool)`,
`Item.GetEnabled()`, `Item.SetChecked(bool)`, `Item.GetChecked()`, `Item.Toggle()`
and `Item.Click()`. Form.MainMenu and Control.PopupMenu accept their component
wrappers or null. `PopupMenu1.Popup(x, y)` uses screen pixel coordinates; assigning
a PopupMenu to a visual control also provides automatic WM_CONTEXTMENU handling.

Shortcut strings accept optional Ctrl/Control, Shift and Alt modifiers, plus
letters, digits, F1-F24, arrows, Home/End, PageUp/PageDown, Delete/Del, Insert/Ins,
Enter/Return, Escape/Esc, Space, Tab and Backspace. Matching is case insensitive.
Leaf main-menu shortcuts use a native accelerator table. Popup shortcuts are
also considered by the existing Form keyboard preprocessor, including popups in
visible, enabled frame scopes. The main menu takes precedence, then popup
collection order. Hosts must continue calling IQuadooFormKeyboard before ordinary
message translation; AQVMT and Preview already do this.

Native callers obtain the optional interfaces through QueryInterface. Returned
component/interface pointers are AddRef'd. The runtime owns its HMENUs/HACCELs
and releases them on replacement or Form destruction, even if callers retain
component interfaces. IQuadooPopupMenu::Popup returns S_FALSE on dismissal.
Native handles are implementation resources and are not serialized.

## External components and uniqueness

IQuadooUniqueComponent returns an immutable non-null capability key. Registration
rejects a duplicate key in the same Form/Frame namespace before changing the
container. This applies to visual and nonvisual controls, including visual
subtrees, and does not depend on a concrete runtime class. Runtime frame instances
start independent scopes. A future status bar can use its own key.

Every non-popup IQuadooMenu implementation must also expose this capability with
QuadooForm::MainMenuUniquenessKey and belong to a Form. Both menu roots and items
expose IQuadooMenuItems, including leaf items with zero children. Items expose
IQuadooMenuItem and the documented property descriptors. External implementations
may use only the published interfaces; none of the rendering, layout or native
construction paths requires a private component pointer. After changing an
external main-menu model, call IQuadooFormMenu::RefreshMenu, and deliver ordinary
property notifications for designer updates.

## Validation

The menu tests are part of the full native harness, private failure-injection
suite, designer suite, Studio integration suite and AQVMT script suite. Focused
native/designer/Studio invocations support `--menus-only`. Tests cover nested
dispatch, programmatic state, shortcuts, immutable IDs, serialization, clipboard
remapping, native wrapping/client geometry, simulated painting at 96/144/192 DPI,
selection synchronization, editor OK/Cancel, source generation, modal Preview,
frame popups, failed replacements, unique registration and native resource cleanup.

The build matrix uses VS2008 for Win32/x64 Debug/Release. It reuses the existing
SimbeyCore binaries. Actual execution on a Windows XP VM remains a host-side check;
the implementation uses XP-compatible APIs and dynamically loads optional themes.
