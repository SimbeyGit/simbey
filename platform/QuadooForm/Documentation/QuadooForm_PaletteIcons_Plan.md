# Icon-based toolbox and component tray plan

Status: implemented; see PaletteIconsImplementation.md for the final contract and
validation. The user waived legacy-extension preservation before implementation;
no legacy adapter or fixture was added. Generic glyphs remain for declined/failed
icon requests and unresolved placeholders.

## Result

Replace the Toolbox with a native list-view control showing a 20x20 icon beside
each type name, following the supplied concept. Pointer remains first. Remove
the Visual Controls and Nonvisual Components divider rows. Preserve the current
ordering: visual types alphabetically, followed by nonvisual types alphabetically,
without headings. Form and Frame remain absent from the Toolbox.

Convert the nonvisual component tray to a list view too. It uses the same type
icons and the same editor-owned image list as the Toolbox. Keep the tray's
Nonvisual components label; it is not one of the removed palette dividers.

Factory icons describe component types. They are separate from a Form instance's
Icon property, and are not saved in form/frame JSON.

## Existing implementation

- Designer/Editor.cpp creates the text-only LISTBOX Toolbox (ID 102), enumerates
  both factory catalogs, and inserts the two divider rows in Palette().
- The tray (ID 107) is an owner-drawn, multicolumn LISTBOX. AttachComponents()
  populates it, and Refresh() synchronizes selection with the Objects tree.
- Designer/Surface.cpp paints a special Timer clock and generic system icons for
  other tray entries. It handles both lists through LBN_SELCHANGE.
- There are 15 visual factories, including Form and Frame, and three nonvisual
  factories: OpenFile, SaveFile and Timer. Thus the standard Toolbox has 16
  component entries plus Pointer after removing the dividers.
- IQuadooControlFactory and IQuadooComponentFactory already provide construction,
  metadata and type identity without constructing a component. Neither currently
  provides an icon. QuadooForm.vcproj has an empty Resource Files filter and no
  runtime .rc file yet.

## 1. Publish one optional factory icon contract

Add an independent optional interface in Published/QuadooForm.h, provisionally
named IQuadooPaletteIcon:

```cpp
interface IQuadooPaletteIcon : IUnknown
{
    STDMETHOD(GetPaletteIcon)(UINT cx, UINT cy, HICON* phIcon) = 0;
};
```

Assign a new unique IID when implementing. Do not extend an existing vtable or
change registry registration rules. Query this same interface on either visual
or nonvisual factory. Instances need not expose it: a palette must work before
there are any instances, and tray entries resolve their type through the existing
registry and reuse the factory's cached image.

Contract:

- cx/cy are requested physical pixel dimensions, with 20x20 as the normal request
  at 96 DPI. Reject zero or unreasonable sizes; document a supported bound.
- Initialize *phIcon to NULL. S_OK returns an independent, caller-owned HICON at
  the requested size. The caller always calls DestroyIcon, including when adding
  it to an image list fails. Never return a shared resource handle under this
  ownership contract.
- S_FALSE with NULL means no custom icon. E_NOINTERFACE on QueryInterface means
  an older factory without icon support. Both select a generic fallback.
- Other failures, or an unusable success result, select a fallback and a concise
  diagnostic rather than making a registered type unavailable.
- Follow existing factory thread-affinity and COM identity rules. Interface
  discovery does not construct a component, run handlers or change live counts.
- External implementations need only the published header and Win32. They may
  load resources from their own DLL or generate an icon; the designer never sees
  a DLL name, HMODULE, resource ID, disk path or concrete implementation class.

The editor holds the factory/interface while calling the provider, copies the
image into its image list, then destroys the returned HICON. The cached image
has no dependency on the provider's resource module after that copy. Retained
factory references still obey the existing external-module lifetime contract;
icon retrieval must not introduce a permanent DLL pin.

## 2. Create the icon assets and standard resources

Create platform/QuadooForm/Icons as a new sibling of Published, Designer and the
inner QuadooForm project directory. Keep all new icon artwork there.

Author original, consistent 20x20 icons with transparent backgrounds, crisp small
details and restrained color, using the screenshot as a visual reference. Save
the runtime files as ordinary .ico files with an uncompressed DIB image and an
AND transparency mask, so Windows XP does not need PNG-compressed icon support.
Review them at actual size on light, dark and selection backgrounds.

Provide one icon for every concrete built-in type:

| Visual types | Nonvisual types |
| --- | --- |
| Form, Frame, Button, CheckBox, ComboBox, GroupBox, Label, ListBox, Memo, Panel, RadioButton, ScrollingPanel, TabControl, TabPage, TextBox | OpenFile, SaveFile, Timer |

Form and Frame receive resources and provider support but no Toolbox entries.
Also create Pointer and generic visual/nonvisual/unavailable-component glyphs.
Pointer is an editor tool, not a registered component factory.

Add QuadooForm/QuadooForm.rc and a private resource-ID header, referencing the 18
built-in type icons with normal ICON resource statements. Add resources/assets to
the VS2008 projects in Debug and Release. No loose files are needed at runtime.

Each concrete built-in type supplies its own private icon resource ID through
its factory/type metadata. CFactory and TComponentFactory implement the optional
interface using that data and a common LoadImageW helper with g_hInstance and
without LR_SHARED. Avoid a central type-name/Kind switch in the designer or
runtime. Check positional TypeSpec initializers when adding an internal field.

Pointer and fallback glyphs belong to the embeddable designer core. Embed their
icon data in that library, generated from the Icons assets, so arbitrary hosts do
not need to provide matching EXE resources. Standard component artwork remains
ordinary resources in QuadooForm.dll. Add a distinct icon resource to the
independent extension test DLL as proof that external resources use the same API.

## 3. Own and share one image list per editor

Introduce a small designer icon cache using the repository's containers and
reference-counting conventions. Cache by retained factory identity and requested
dimensions; type names remain the registry lookup key. Toolbox rows and tray
instances of that type share an image index. Multiple Timer instances do not add
multiple Timer images. Reserve stable entries for Pointer and fallbacks.

Use an ILC_COLOR32 | ILC_MASK image list with 20x20 cells at 96 DPI. Both list
views attach it as LVSIL_SMALL and use LVS_SHAREIMAGELISTS. The editor owns the
image list and destroys it exactly once after detaching it from both views or
after both windows have been destroyed. Temporary HICONs and validation bitmaps
must be released on every success/failure path.

Keep source artwork 20x20. Scale the requested pixel size with the editor's UI
DPI, independently of canvas zoom. At a DPI change, stage a new image list/cache
and both views' image mappings before swapping. A failed rebuild preserves the
previous list and selection. Do not repeatedly load icons during paint, document
refresh or selection changes. Ordinary editor shutdown releases cached factories.

Older factories use generic visual/nonvisual icons. Unresolved design placeholders
use the unavailable glyph, even if a registered factory happens to have the same
type name. Preserve placeholder text/reason diagnostics and lossless persistence.

## 4. Migrate the Toolbox and tray together

Toolbox:

- Use WC_LISTVIEWW in report mode with one hidden-header column, single selection,
  full-row selection, persistent selection indication and native double buffering.
  This produces one vertical icon-and-text column, without list-mode horizontal
  wrapping. Size the column to the client width and keep native vertical scrolling.
- Put the stable palette entry ID in LVITEM.lParam. Do not use the image index as
  a factory ID. Keep the visual/nonvisual classification for construction, even
  though it no longer produces divider rows.
- Preserve Pointer/reset behavior, compatible-container placement, TabPage parent
  constraints, keyboard/type-to-select navigation and focus handling.
- Preserve existing activation semantics: selecting a visual type arms placement;
  selecting a nonvisual type adds one component and resets to Pointer. Ensure the
  LVN_ITEMCHANGED/reset notifications cannot create a component twice. A subsequent
  deliberate selection can add another instance of the same nonvisual type.

Tray:

- Use a small-icon list view with names beside 20x20 icons, horizontal arrangement
  and scrolling suitable for the shallow pane. Keep the instance name as the main
  label and the type/reason available in an infotip; avoid a Vista-only tile view.
- Store node identity in item data and synchronize the selected GUID with Objects
  and the inspector. Preserve naming, deletion, keyboard navigation, document
  switching and the existing single-selection model.
- Remove the Timer drawing special case and the old tray WM_DRAWITEM/
  WM_MEASUREITEM handlers once native icon rendering replaces them.
- Rebuild nodes/rows under the existing change-depth/redraw guards. Suppress
  programmatic notification feedback and preserve the selected component.

Update Surface.cpp, Editor.cpp, Designer.h and layout/selection helpers together.
Audit all LB_* messages and synthetic LBN_SELCHANGE notifications in designer
tests; migrate only Toolbox/tray usage. The tab-order dialog's list box stays as
it is. No Objects-tree icon conversion is required in this pass.

## 5. Validate resources, extensibility and lifetime

Extend the independent test DLL using only published interfaces. Exercise both
visual and nonvisual icon providers, and retain a legacy factory with no provider.
Verify a custom icon appears in the Toolbox, and the nonvisual factory's same icon
appears for its instances in the tray, without source-specific designer code.

Automated coverage:

- All 18 built-in factories return a usable 20x20 owned icon; DLL resources can be
  loaded directly. Form/Frame stay absent from the Toolbox, and there are exactly
  17 standard rows including Pointer and no divider rows.
- Factories without the interface, S_FALSE, failed loading and invalid results
  use fallbacks. Icon retrieval never constructs live components or fires events.
- COM identity, AddRef/Release, repeated independent HICON ownership, external
  module pinning/unload and repeated editor create/destroy behave correctly.
- Both views attach the same image list; all instances of a type reuse its index.
  Destroying a child view does not destroy the shared image list. Editor teardown
  releases it once. Repeated refresh does not grow the image count.
- Toolbox mouse/keyboard selection, one-shot nonvisual insertion, placement,
  Pointer reset, tray/tree/inspector synchronization, placeholders and deletion
  retain their behavior. Programmatic selection generates no duplicate actions.
- Failed icon/cache/list allocation and DPI rebuild roll back cleanly. Exercise
  document switching, empty trays, multiple instances and long labels.
- Preserve JSON round trips, dirty state, TabPage placement and modal Preview.
  Images and image indexes never enter saved documents.
- Run the complete native, private and designer suites in Win32 Debug and Release,
  retaining strict GDI/USER resource checks. Reuse existing SimbeyCore binaries.

Visually inspect normal/selected/inactive rows, scrolling, pane resizing,
maximizing/repainting and high DPI on modern Windows. Exercise the actual XP VM
when available, including alpha/mask rendering and shared-image-list teardown;
report separately if XP execution or particular theme/DPI cases were not run.

## Implementation order and compatibility

1. Finalize the additive interface/ownership contract and add resource/icon tests.
2. Author assets, embed built-in resources and implement both factory providers.
3. Implement the shared editor image cache and independent-DLL coverage.
4. Convert both controls and update notification/selection/placement tests.
5. Run full validation, inspect the UI and document results/remaining issues.

Existing public vtables, factory registration, descriptors, lifecycle, event
binding and JSON stay unchanged. Older extensions remain usable with fallbacks.
No new built-in controls, runtime image-list component, QuadooScript/Studio work,
STL, exceptions or SimbeyCore changes are needed.

## Native API references

[List-view styles](https://learn.microsoft.com/en-us/windows/win32/controls/list-view-window-styles)
define hidden report headers and shared image-list ownership.
[ImageList_ReplaceIcon](https://learn.microsoft.com/en-us/windows/win32/api/commctrl/nf-commctrl-imagelist_replaceicon)
documents that adding an icon copies its image rather than retaining the HICON.
[LoadImageW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-loadimagew)
documents module resources, requested dimensions and LR_SHARED ownership.
Check the configured SDK 6.0A and actual XP behavior as well; current documentation
support tables alone are not proof of XP compatibility.
