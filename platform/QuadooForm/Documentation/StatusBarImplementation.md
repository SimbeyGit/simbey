# StatusBar implementation

`StatusBar` wraps the Win32 status bar. It can be placed directly on a Form or
Frame. Each Form/Frame namespace permits one status bar, using the existing
`IQuadooUniqueComponent` registration contract and a separate uniqueness key.
A runtime frame has its own namespace and may have a status bar even when its
containing form also has one. Panels and group boxes are not eligible parents.

## Properties and panels

| Property | Behavior |
| --- | --- |
| Panels | Live ordered collection; serialized as an array of panel definitions. |
| Images | Optional ImageList component reference in the owning Form/Frame namespace. |
| SizeGrip | Requests the native size grip; can be changed at runtime. |
| Height | Logical height at 96 DPI; defaults to 22. |

Each panel exposes `Text`, `Width`, and `Icon`. Width is a nonnegative 96-DPI
logical value, scaled when sending native part coordinates. The last panel
extends to the right edge; its saved Width is retained when it becomes an
intermediate panel. `Icon` is an image-list index; `-1` clears it. An index
outside the current image list draws no icon and remains saved, so it can become
valid after the image list grows. Without Images, panels draw text only.

The collection supports `Count`, indexed access, `Add()`, `Insert(index)`,
`Delete(panelOrIndex)`, `Move(panelOrIndex, finalIndex)`, and `Clear()` in script.
Native callers use `IQuadooStatusPanels`; its Insert method returns a panel that
can then be edited. Native Delete/Move accept panel references. Panel references
retain their identity as indexes change. Deleted or owner-destroyed panels
return `EDestroyed` for value/index operations; their component ID and destroyed
state remain readable. Retained entries keep the factory/module lifetime pinned
without keeping the live status bar or parent alive.

There are at most 255 normal panels (indexes 0 through 254). Win32 reserves
index 255 as `SB_SIMPLEID` for simple-mode text, so it cannot be used as an
ordinary independently editable panel. See [SB_SETTEXT](https://learn.microsoft.com/en-us/windows/win32/controls/sb-settext)
and [SB_SETPARTS](https://learn.microsoft.com/en-us/windows/win32/controls/sb-setparts).

Example script:

```text
StatusBar1.Images = ImageList1;
var panel = StatusBar1.Panels.Add();
panel.Text = "Ready";
panel.Width = 140;
panel.Icon = 0;
StatusBar1.Panels.Move(panel, 0);
StatusBar1.Panels[0].Text = "Working";
StatusBar1.SizeGrip = false;
```

## Layout and native ownership

The optional published `IQuadooViewportDock` capability reserves the bottom
strip before ordinary docking, independent of creation order, margins, and
padding. AlignClient children therefore receive the remaining space even when
created first. The native bar is attached to the viewport HWND, rather than a
scrolling content HWND, and its height is excluded from the scrollable viewport.
It remains fixed while content scrolls. Native positioning uses the actual parent
pixel rectangle to avoid fractional-DPI rounding gaps at the right/bottom edge.
The usual requested/effective-bounds distinction remains intact. Studio disables
editing the bar's position, width and alignment; height remains editable.
Shared MinHeight/MaxHeight constraints are applied when reserving that height.

Images are acquired through `IQuadooImageList::Acquire(dpi)`. Each displayed icon
is copied with `ImageList_GetIcon` into a separately owned HICON. The snapshot can
then be released: no image-list handle is attached to the native status bar.
Native icon replacement prepares all new icons before changing parts, and frees
old HICONs after detaching/replacing them. Image-list notifications, DPI changes,
panel changes, and native SizeGrip recreation rebuild the icons. Source
destruction clears the Images reference and displayed icons. Weak image-list
observers are disabled before removal, and reentrant mutation during native
image acquisition is rejected.

During SizeGrip HWND replacement, the old HWND's HICONs remain owned until it
has been destroyed or restored on failure. Collection mutations roll back
failed preparation/native updates, and preserve already committed state when a
later observer returns an error. Teardown removes image subscriptions after the
owner has entered Destroying, so third-party callbacks cannot restart teardown.

Panel JSON replacement validates and allocates the entire candidate collection
before publishing it. Invalid widths, icons, duplicate panel IDs, owner/group ID
collisions and excessive counts are rejected. Existing panel references remain
usable when validation fails. Accepted replacement, removal, or destruction
detaches the corresponding old entries.

## Designer and Studio

Both hosts use the new 20x20 StatusBar palette icon, embedded as an ordinary
ICON resource in QuadooForm.dll. The source PNG/ICO and artwork generator live
under `Icons`.

The Objects tree contains `StatusBar > Panels > [index] Text`. Select the status
bar or its Panels branch and choose **Add Status Panel** from the tree context
menu or Form menu. The command also works with a panel selected. Select a panel
to edit Text, Width or Icon in the regular inspector. Text edits inline. Icon is
the numeric image index. Use **Move Panel Left/Right** to reorder; boundary
commands are disabled. Delete removes the selected panel. Select Parent moves
from a panel to Panels, and from Panels to the status bar. Double-clicking the
Panels property selects the Panels tree branch.

Entries use the optional `IQuadooComponentOutline` capability and ordinary
`IQuadooProperties`; the inspector has no separate panel editor. They are
collection objects, not registered Form components or visual children. They do
not receive generated QuadooScript member variables. The status bar itself
receives the usual generated member. Panel GUIDs are serialized; the Panels
group has an identity derived from the status bar ID. Clipboard duplication
regenerates panel GUIDs. Saved Studio outlines also show panels before opening
the designer.

## Public ABI

Additive interfaces only; existing interface IDs, vtables, exports and factory
contracts are unchanged:

- `IQuadooComponentOutline` (`...0050`): parent ID and inspectable collection entries.
- `IQuadooViewportDock` (`...0051`): viewport docking capability, currently AlignBottom.
- `IQuadooStatusBar` (`...0052`): panels, ImageList and SizeGrip access.
- `IQuadooStatusPanel` (`...0053`): index, text, logical width and image index.
- `IQuadooStatusPanels` (`...0054`): collection insertion, deletion, movement and clearing.

QuadooForm.dll remains independent of QuadooScript. The AQVMT script adapter
wraps the published native interfaces. No compiler/VM changes are required.

## Validation

Build affected projects without rebuilding SimbeyCore, JSON or compiler/VM DLLs:

```powershell
tests\platform\QuadooForm\build-application-lifetime.cmd Debug Win32
tests\platform\QuadooForm\build-application-lifetime.cmd Release Win32
tests\platform\QuadooForm\test-statusbar.ps1 -Configuration All -Full
```

The focused runner includes native HWND tests, shared Designer tree/property
editing, palette regression tests, Studio project/source/persistence checks and
real QuadooScript collection access. `-Full` additionally runs the complete
native harness and private metadata/storage tests. The new tests are also part
of ordinary full harness, Designer and Studio integration runs.

Native coverage includes bottom docking before an earlier AlignClient control,
separate Form/Frame uniqueness, ineligible parents, stable panel references and
COM identity, invalid JSON rollback, native part indexes/capacity, wrong-thread
access, DPI-scaled widths and copied icons, fractional-DPI scrolling geometry,
SizeGrip recreation, JSON round trips, ImageList destruction, GUI resource
counts and retained factory lifetime.

Logs are under `tests\results\StatusBarLogs`; build summaries are
`target\StatusBar-Debug-build.log` and `target\StatusBar-Release-build.log`.

Both Win32 configurations built with VS2008 with zero errors and warnings.
SimbeyCore.dll, JSON.dll and compiler/VM dependency DLLs were reused.

| Validation | Debug | Release |
| --- | ---: | ---: |
| Native StatusBar, panels, icons, docking and cleanup | 143 passed | 143 passed |
| Shared Designer tree, inspector and actual inline text editing | 16 passed | 16 passed |
| Palette regression | 1,468 passed | 1,468 passed |
| Studio StatusBar source/tree/save/reopen integration | 8 passed | 8 passed |
| Application runtime and real script adapters | 139 passed | 139 passed |
| Complete native harness | 9,760 passed | 9,760 passed |
| Private storage/metadata and existing rendering regression | 1,544 passed | 1,544 passed |

Successful final outputs are `target\StatusBar-final-focused.log` and
`target\StatusBar-native-final-tests.log`. The latter records a fresh full
native/metadata rerun. One earlier native harness attempt stopped before the
StatusBar cases when Windows created OLE/SystemUserAdapter helper windows after
the existing sizing/focus resource monitor had recorded its baseline. Unchanged
reruns passed; the failed attempt remains in `Debug-native-full.log`.

PE inspection
confirmed subsystem version 5.0 for QuadooForm.dll, QuadooStudio.exe, AQVMT.bin
and QuadooApplicationTests.exe in both configurations. The palette PNG is RGBA
20x20, and the ICO is an uncompressed DIB with an AND mask.

A separate complete Designer regression attempt stopped in the existing
clipboard-preservation helper at `DesignerTests\ClipboardTests.h:51`, returning
`0x8007054F` while reading an existing clipboard format. The clipboard was left
unchanged. The StatusBar-specific Designer/Studio tests above passed. A private
window-station experiment could not run the suite's focus-dependent dropdown
checks, so that experimental test path was removed. The complete Designer and
Studio UI suites remain outside the successful validation totals above; rerun
them when the clipboard can be fully preserved:

```powershell
tests\results\bin\Debug\QuadooFormDesignerTests.exe --trace
tests\results\bin\Release\QuadooFormDesignerTests.exe --trace
tests\results\bin\Debug\QuadooStudioIntegrationTests.exe --trace
tests\results\bin\Release\QuadooStudioIntegrationTests.exe --trace
```

Windows XP VM validation remains a manual check. Copy the matching built runtime
and dependencies, then run `QuadooFormHarness.exe --status-bar-only --trace`.
Also open `DesignerExamples\StatusBars.qform.json` and verify the panel text,
icons, resizing, tree editing and modal Preview under XP common controls.


## Grip and inline numeric editing follow-up (2026-11-03)

The native status bar keeps its requested SizeGrip appearance, but its grip
cannot start a sizing operation unless its immediate parent is a non-maximized,
sizeable top-level window. Hit testing and nonclient/system sizing messages are
filtered in CStatusBar::Message(). The current native parent style is checked on
each operation, so changing BorderStyle between Sizeable and Single, None or
Dialog takes effect without recreating the status bar. A frame's status bar and
a design-mode status bar also cannot resize their child HWNDs.

The shared inspector now edits Int, Long and Float properties inline, including
panel Width and ordinary control Width/Height. Enums with descriptor choices
continue to use dropdowns; an enum without choices accepts inline numeric text.
Flag editors, collections, multiline text and other specialized editors retain
their existing UI. Tab navigation includes numeric fields. The existing property
and multi-selection transactions still validate and commit the resulting JSON.
Rejected edits remain visible for correction or Escape, and accepting an unchanged
or untouched mixed value does not dirty the document. Parsing rejects trailing
junk rather than accepting only a valid numeric prefix.

VS2008 Win32 Debug and Release builds completed with zero errors and warnings;
dependency DLLs were reused. Focused results:

| Validation | Debug | Release |
| --- | ---: | ---: |
| StatusBar native, fixed grip and dynamic BorderStyle | 177 passed | 177 passed |
| StatusBar tree and numeric inspector editing | 55 passed | 55 passed |
| Studio StatusBar integration | 8 passed | 8 passed |
| Existing inline fields, dropdowns and menu editing | 289 passed | 289 passed |
| External packages, including inline Long/Float values | 365 passed | 366 passed |
| Multi-selection, including mixed numeric edits | 65 passed | 65 passed |

The package suite's assertion count varies with its bounded resource-cycle
monitor. The Debug palette regression also passed (1,468 assertions).
Commands, using either target\Debug or target\Release:

```powershell
QuadooFormHarness.exe --status-bar-only --trace
QuadooFormDesignerTests.exe --status-bar-only --trace
QuadooFormDesignerTests.exe --editing-only --trace
QuadooFormDesignerTests.exe --packages-only --trace
QuadooFormDesignerTests.exe --selection-only --trace
QuadooStudioIntegrationTests.exe --status-bar-only --trace
```

Logs: target\StatusBar-hardening-final.log,
target\StatusBar-hardening-Debug-tests.log and tests\results\StatusBarLogs.
The broader Debug ApplicationTests Runtime.quadoo run failed before its StatusBars
case: VirtualLists did not return the expected value, and the trace reported
E_NOINTERFACE in ScriptAdapter\RuntimeObjects.cpp:422, in the existing indexed
property dispatcher. That source was unchanged in this follow-up. This does not
constitute a passing full application-script run. XP VM validation remains manual.
