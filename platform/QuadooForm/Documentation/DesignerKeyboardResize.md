# Designer keyboard resizing

Implemented November 5, 2026 in the shared designer core used by the stand-alone
Designer and Quadoo Studio.

| Shortcut | Action |
|---|---|
| Shift+Left | Reduce Width by one logical design pixel |
| Shift+Right | Increase Width by one logical design pixel |
| Shift+Up | Reduce Height by one logical design pixel |
| Shift+Down | Increase Height by one logical design pixel |

Left and Top remain fixed. Keyboard increments bypass grid snapping and do not
depend on DPI or zoom. Repeated key messages repeat the operation. Ctrl+Arrow
continues to move controls; Ctrl+Shift+Arrow and Alt combinations are not claimed
by this shortcut.

The shortcuts work on the design surface and component outline, including Studio's
shared project tree. Inline property editors, the inspector list, palette, source
editor and inactive designers retain their native arrow-key behavior. Keyboard
resizing is ignored while a mouse gesture owns the selection.

## Capabilities, constraints and transactions

Sizing uses the published IQuadooDesignCapabilities flags. ComboBox height,
AutoSize dimensions, docked stretch axes, TabPages and nonvisual components are
restricted through their capabilities; there are no control-type tests in the
keyboard path. Form and Frame roots can resize their requested client dimensions.

Each selected sibling receives the same width/height increment, retaining its own
size and position. The document validates the entire group before applying one
candidate definition. If any selected control cannot resize on that axis or has
reached a Min/Max limit, the group stays unchanged and the document stays clean.
Failed candidate/source edits likewise leave the previous definition intact.

Sizes cannot become negative; zero is permitted when MinWidth/MinHeight allow it.
Only the changed dimension is validated against its Min/Max limits. Constraints
can clamp an effective size while retaining a different requested size: resizing
that axis steps from the constrained size, allowing it to move back into range.
The requested size of the other axis is preserved.

TranslateEditorMessage routes Shift+Arrow to CDesignerEditor::ResizeSelection.
CDesignerDocument::ResizeControls preflights dimensions and capabilities using
64-bit intermediate arithmetic. Single-control edits reuse SetBounds; groups use
the existing serialized candidate/ApplyDefinition transaction. Selection identity,
inspector/status updates, dirty state, JSON and Studio source synchronization use
the existing paths. Mouse resize grips still operate on a single selection.

No published ABI, JSON schema, runtime control, menu or accelerator resource was
changed. The existing static designer library supplies the behavior to both hosts.

## Validation

VS2008 Win32 Debug and Release builds passed for the shared designer core,
stand-alone Designer, Designer tests, Quadoo Studio and Studio integration tests,
with no compiler errors or warnings. SimbeyCore, JSON and compiler/VM dependency
DLLs were reused.

Both configurations passed these focused suites:

| Suite | Assertions |
|---|---:|
| Shift+Arrow resizing | 210 |
| Existing Ctrl+Arrow nudging | 189 |
| Existing sibling multi-selection | 65 |
| Studio keyboard integration | 72 |

New coverage exercises all directions at 96/144/192 DPI and 50/100/150/200% zoom,
grid independence, stable positions and selection, key repeats, inspector values,
inline editing, palette/inspector context, mouse-capture exclusion, Form/Frame
roots, axis restrictions, multiple controls of different sizes, Min/Max boundaries,
constrained requested/effective sizes, zero sizes, integer overflow, rejected source
transactions, JSON round trips for both siblings, Studio shared-tree routing and
dirty flags, and switching Studio to code view.

The first broader Studio embedding run reached the existing clipboard snapshot
test after the keyboard assertions and failed at ClipboardTests.h:51 with
0x8007054F. Its log is target/ResizeKeys-Debug-studio-broad-first.log. The new
--designer-keys-only selector isolates keyboard/multi-selection integration; the
default embedding test still includes its original clipboard assertions. No full
Studio regression-suite pass is claimed for this change. XP VM/manual keyboard
validation was not performed.

Build logs: target/ResizeKeys-{Debug,Release}-build.log. Test logs:
target/ResizeKeys-{Debug,Release}-{resize,nudge,selection,studio}.log. The temporary
target/ResizeKeys-build.cmd helper vcbuilds only the five affected projects above.
The existing build-application-lifetime.cmd can also rebuild these hosts without
rebuilding dependency DLLs.

Run focused tests from the repository root after building:

```powershell
tests\results\bin\Debug\QuadooFormDesignerTests.exe --resize-only --trace
tests\results\bin\Debug\QuadooFormDesignerTests.exe --nudge-only --trace
tests\results\bin\Debug\QuadooFormDesignerTests.exe --selection-only --trace
tests\results\bin\Debug\QuadooStudioIntegrationTests.exe --designer-keys-only --trace
tests\results\bin\Release\QuadooFormDesignerTests.exe --resize-only --trace
tests\results\bin\Release\QuadooFormDesignerTests.exe --nudge-only --trace
tests\results\bin\Release\QuadooFormDesignerTests.exe --selection-only --trace
tests\results\bin\Release\QuadooStudioIntegrationTests.exe --designer-keys-only --trace
```
