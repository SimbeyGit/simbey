# Palette icons and shared designer image list

The Toolbox and nonvisual tray now use native list views. The Toolbox has a
single icon/name column, Pointer first, then visual and nonvisual type names in
their existing order, without divider rows. Form and Frame remain absent. The
tray uses small icons beside instance names and exposes the type or unresolved
component reason through an infotip. Selection stays synchronized with Objects
and the property inspector.

## Factory contract

Published/QuadooForm.h adds IQuadooPaletteIcon, IID ending 0034:

```cpp
STDMETHOD(GetPaletteIcon)(UINT cx, UINT cy, HICON* phIcon) = 0;
```

Query this interface on IQuadooControlFactory or IQuadooComponentFactory. Dimensions
are physical pixels, each from 1 to 256. S_OK returns an independently owned
HICON of the requested size; the caller must call DestroyIcon. The provider must
initialize the output to NULL and must not return an LR_SHARED handle. S_FALSE
with NULL means the provider has no artwork. It follows factory thread affinity.
Retrieval never creates an instance or executes application events.

The designer uses this same call for every source, without knowing the resource
module, resource ID, filename or concrete class. Built-in factories load their
per-type resource IDs using LoadImageW without LR_SHARED. The independent test
DLL implements the contract using only published interfaces and resources in its
own module. Its visual and nonvisual factory icons are deliberately distinct.

Existing public interface declarations, export ordinals and JSON are unchanged.
There is no legacy-extension migration layer or legacy test fixture. Generic
glyphs handle a provider that declines artwork or returns an invalid/error
result, and unresolved placeholders have a separate warning glyph. A staging
out-of-memory error preserves the current image list and DPI rather than
partially replacing the UI. Icon diagnostics are nonmodal status messages.

## Assets and resources

Icons/ contains 20x20 ICO artwork for all 18 concrete built-in types, including
Form and Frame, plus Pointer, generic/warning glyphs and two extension examples.
QuadooForm/QuadooForm.rc embeds the built-in icons as ordinary ICON resources in
QuadooForm.dll. These use uncompressed 32-bit DIB data and transparency masks;
there is no dependency on PNG-compressed ICO support or loose runtime files.

Icons/GenerateIcons.py is the reproducible authoring tool; it needs Pillow only
when regenerating artwork. Normal VC9 builds use the ICO files and generated
Designer/DesignerIconData.h. The latter embeds editor-only Pointer/fallback data
in the static designer library, without requiring host EXE resources. The artwork
preview is Icons/PalettePreview.png. Palette icons are separate from a Form's
serialized Icon property.

## Ownership, DPI and input

One HIMAGELIST belongs to each editor and is attached to both list views with
LVS_SHAREIMAGELISTS. Images are copied from temporary owned HICONs, which are
destroyed immediately. Each factory has one cached image, reused by every tray
instance of that type. Selection, document refresh and canvas zoom do not reload
icons or add image-list entries.

The base display size is 20x20 at 96 DPI, scaling with editor UI DPI (30x30 at
144 DPI). Canvas zoom does not affect it. A DPI rebuild stages a complete list
before swapping both views. It preserves component selection and logical JSON.
Window shutdown detaches/destroys the image list once and releases retained
factories, even when the C++ editor object itself remains referenced.

Mouse selection arms/activates on the completed NM_CLICK notification. The
Toolbox subclass distinguishes mouse selection from keyboard/programmatic
selection, preventing mouse-down/reset/mouse-up from inserting twice. Native
keyboard selection retains the existing immediate nonvisual insertion behavior.
Pointer is selected after insertion without changing the palette's scroll
position. It is scrolled into view only during the editor's initial layout.

## Validation

The focused QuadooFormDesignerTests.exe --palette-only suite covers every built-in
factory resource/provider, native keyboard selection, completed-click delivery,
shared indexes, tray/tree/inspector selection, rename/delete, placeholders,
serialization, DPI scaling/rollback, malformed/error provider results, COM
identity, thread affinity and factory/module lifetime. It includes the unchanged
bounded warm-up and 20 measured GDI/USER teardown cycles.

Full Win32 Debug/Release results and final dependency/ABI audits are recorded in
target/PaletteIcons-{Debug,Release}-{build,native,private,designer}.log,
target/PaletteIcons-ABI-audit.log and target/PaletteIcons-Dependencies-audit.log.
The icon-conversion milestone's VS2008 Win32 builds completed with zero
errors/warnings:

| Suite | Debug | Release |
| --- | --- | --- |
| Public runtime | 7,769 passed | 7,769 passed |
| Private native | 119 passed | 119 passed |
| Focused palette/tray | 740 passed | 740 passed |
| Complete designer | 2,672 passed | Failed at the final Preview resource check, after 2,298 assertions |

The final Release designer run detected GDI growth from 27 to 28 (USER stayed 5)
after two measured Preview cycles. Earlier runs detected 26 to 28. Every earlier
functional group, including the new palette's strict 20-cycle test, passed. A
Debug designer resource failure was also retained; its fresh full run passed.
Earlier Release native attempts detected USER 23 to 24; an isolated pre-change
Subversion runtime/extension reproduced USER 23 to 24 in an earlier resource
group, and the final normal Release native run passed. The source comparison
changed only isolated project paths, reusing the same dependency DLLs.

These resource checks retain their original thresholds, warm-up bounds and cycle
counts. The late designer GDI growth also appears in earlier pre-PNG and keyboard/
Memo validation logs and remains unresolved. Preserved failures and baseline
evidence are listed in DesignerValidation.md; passing reruns do not erase them.

Modern Windows visual inspection used the computer-use skill to check actual
mouse insertion, keyboard insertion, palette scrolling, Pointer reset, shared
Timer tray icons and maximized repainting. Each gesture added exactly one Timer;
the saved scratch definition is target/PaletteIcons-VisualCheck.qform.json.
Source artwork was inspected on light/dark/selection backgrounds. Automated DPI
checks use 96/144/192 DPI. Actual XP execution and physical high-DPI/theme changes
were not performed in this pass; neither compilation nor simulated DPI is an XP
test. Existing SimbeyCore binaries are reused without rebuilding or replacing.

## Follow-up: preserve palette scrolling after insertion

ResetTool no longer calls ListView_EnsureVisible for Pointer. That operation now
runs only after the editor's initial layout. Both visual placement and nonvisual
activation keep the current viewport while returning to Pointer mode.

A new regression reproduced the jump before the fix. It checks exact scroll
positions and item geometry after repeated TextBox placement through the canvas
and Timer insertion through the completed-click path, in Forms and Frames at
96/144 DPI. It also checks actual insertion, single insertion, selected component
type and Pointer reset. The focused Debug/Release palette suites each pass 852
assertions; existing TabPage placement (74) and Memo designer workflows (109)
also pass in each build. Both builds report zero errors/warnings. Logs are
target/PaletteScroll-{Debug,Release}-{build,palette,tabs,memo}.log, with the
pre-fix failure in target/PaletteScroll-before-fix.log. Full suites were not rerun
for this focused UI fix. No public ABI, runtime control or dependency changed.

The first fix still moved LVIS_FOCUSED to Pointer. Native list-view input can
later reveal that focused row on a timer, so nonvisual insertion still jumped.
The follow-up retains the current focused row and resets LVIS_SELECTED only.
The regression now uses queued mouse-down/up input and pumps messages through
the double-click timer interval, rather than relying on a direct NM_CLICK and
an immediate queue drain. This reproduced the remaining jump before the fix.

OpenFile, SaveFile, repeated Timer insertion and subsequent arrow-key insertion
now retain their exact scroll positions in Forms and Frames at 96/144 DPI. The
Debug/Release palette suites each pass 1,068 assertions; existing TabPage and
component suites pass 74 and 874 assertions in each build. Builds have zero
errors/warnings. Logs are target/PaletteFocus-{Debug,Release}-{build,palette,tabs,
components}.log and target/PaletteFocus-before.log. Full suites and XP were not
rerun for this focused correction. Both SimbeyCore DLL hashes are unchanged.
