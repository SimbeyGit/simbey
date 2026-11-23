# ColorSwatch

`ColorSwatch` is a built-in windowless visual control. It uses the existing factory, layout, property, event, persistence and designer capabilities, exposes `IQuadooStaticControl`, and creates no HWND. No published interface or export changes are required. The designer discovers its standard 20 x 20 palette icon through the factory's existing icon provider.

| Property | Type | Behavior |
| --- | --- | --- |
| `Mode` | Enum | `Fixed` (0, default) or `Editable` (1). Editable requires exactly one color. |
| `Colors` | Collection | An independent JSON array of 1Ã¢â‚¬â€œ256 signed 32-bit Quadoo Color values. Defaults to system `COLOR_BTNFACE`. |
| `Selected` | Int | A zero-based color index, or -1 for no selection (default). |
| `Labels` | List | An independent JSON array of single-line strings, initially empty. Missing entries have no label; empty strings are supported. At most one entry per color, with 4,096 characters per entry. |

All four properties are editable and serializable through `IQuadooProperties`. Colors use the existing Quadoo Color encoding, including system colors; resolution occurs only at painting and picker initialization. For example, `COLOR_HIGHLIGHT` is `0x8000000D` in C++, represented as `-2147483635` in JSON. `Colors` and `Labels` use the inspector's existing JSON collection editors; `Mode` uses its enum editor. The normal Font and ForeColor properties control label text.

```json
{
  "type": "ColorSwatch",
  "name": "AccentColors",
  "properties": {
    "Width": 320,
    "Height": 64,
    "Mode": "Fixed",
    "Colors": [11844752, 13812616, 11836288, 11371870],
    "Selected": 1,
    "Labels": ["Teal", "Cyan", "Slate", "Blue"]
  },
  "events": { "OnClick": "AccentColors_Click" }
}
```

Fixed mode divides the width into equal cells. Clicking a cell or its label sets Selected, then fires the standard `OnClick` event. Repeated clicks on the selected cell also fire it. Editable mode opens `ChooseColorW` with the current resolved color; OK replaces Colors with the chosen RGB and fires OnClick. Cancel leaves both properties and events unchanged. Programmatic property edits do not fire OnClick. Design-mode clicks never open a runtime picker.

Painting reserves three logical pixels above and below the ordinary color band. Selected colors extend into that space, have their own rounded corners and a one-physical-pixel border. Each nonzero RGB channel is darkened by approximately 20 percent for the border; black uses a lighter gray. A single color and the ends of a multiple-color band are rounded. Labels reserve a separate font-height band beneath the colors. All rendering is clipped to the supplied physical bounds and preserves the caller's HDC state.

Colors, Labels, Mode and Selected are validated together before their slots change. Replacing Colors clears an out-of-range selection and trims excess labels in the same commit; observers see the complete resulting state. Invalid values or incompatible modes leave the properties unchanged. Initial definitions must already have consistent colors, labels and selection. GetValueJSON and SetValueJSON provide independent snapshots, so editing an array held by a caller cannot silently change a live control.

Picker handling retains the component through callbacks, blocks recursive picker activation, and discards an accepted result if the palette, labels, mode or selection changed during its modal loop. Destruction during the picker or its Click callback is safe. The native test harness covers these paths along with COM identity, selection boundaries, raw system colors, invalid edits, observer consistency, DPI painting, JSON round trips, construction rollback and factory live counts.

Open [ColorSwatches.qform.json](DesignerExamples/ColorSwatches.qform.json) in the designer to try an unlabeled palette, a labeled palette and an editable single color. The example is also copied into each build's DesignerExamples directory.

The implementation remains compatible with the Visual Studio 2008 toolset and Windows XP APIs. SimbeyCore and QuadooScript do not require changes or rebuilding.

## Validation (2026-10-17)

All four VC9 builds (Win32/x64, Debug/Release) completed with zero compiler errors or warnings. Final focused runs pass **92 native ColorSwatch assertions**, **18 designer assertions**, **671 private storage/metadata/failure-injection assertions**, and **87 Studio integration assertions** in each configuration. The native ColorSwatch tests include 200 consecutive paints with stable GDI counts and unchanged HDC font, pen and brush selections. Colors containing null, invalid tags, invalid selections, incompatible modes, invalid labels, oversized arrays and a failing public JSON input are rejected without changing live properties or notifying observers.

The complete suites were also run in every configuration. The initial full-run results, before the last null-entry and defensive paint checks, are preserved below. Those final changes affect only ColorSwatch; the focused control, designer, private and Studio suites were rebuilt and rerun afterward.

| Configuration | Full public native suite | Full designer suite | Private suite | Studio |
| --- | --- | --- | --- | --- |
| Win32 Debug | 8,028 assertions; TabControl USER-count failure | 3,215, pass | 671, pass | 87, pass |
| Win32 Release | 7,456; choice, item and tab USER-count failures | 1,134; palette USER-count failure | 671, pass | 87, pass |
| x64 Debug | 7,974; TabControl USER-count failure | 3,215, pass | 671, pass | 87, pass |
| x64 Release | 8,046, pass | 3,215, pass | 671, pass | 87, pass |

All ColorSwatch groups passed in every full native run. The resource-count failures occur in the existing bounded warm-up checks and match the intermittency recorded in QuadooColorsImplementation.md and NativeValidation.md. No thresholds or assertions were relaxed. Full failure logs are retained as `target/ColorSwatch-full-*`; final focused results use `target/ColorSwatch-final-*`, and final builds use `target/ColorSwatch-build-final-*`.

Manual inspection of the example in the designer and runtime Preview confirmed the rounded strip, raised selection, centered labels, single-color shape, changed selection after clicking another cell, and the standard color picker. The inspected application was closed afterward. Actual Windows XP execution was not performed; the implementation uses XP-compatible GDI/common-dialog APIs. All four reused SimbeyCore hashes match the binaries inspected at the start of the task; hashes are recorded in `target/ColorSwatch-SimbeyCore-hashes.json`.
