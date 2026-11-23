# Quadoo colors and inspector swatches

`QuadooProperty::Color` retains its 32-bit value through property storage,
notifications, JSON serialization and designer insertion:

* `0x00BBGGRR` is an ordinary Win32 COLORREF.
* `0x80000000 | COLOR_*` stores a GetSysColor index in the low 24 bits.

The property API and JSON use signed 32-bit integers. For example, system
`COLOR_BTNFACE` is `0x8000000F`, represented in JSON as `-2147483633`.
Built-in normalization rejects other high-byte tags. This is a color tag, not
image opacity or an alpha-blending channel. Existing RGB values remain valid.

JSON parses `-2147483648` (the raw `COLOR_SCROLLBAR` value) as a LongInteger.
Built-in Color properties and ColorSwatch palettes accept that representation,
check the signed 32-bit range and store canonical Integer values. Independent
factory loaders should perform the same range check when reading color numbers;
do not truncate a 64-bit value to COLORREF. The serialized numeric format is
unchanged.

The new public export is:

```cpp
COLORREF WINAPI ResolveQuadooColor(COLORREF color);
```

It is appended at ordinal 15. Existing exports/ordinals, all interface declarations,
IIDs, vtables and property-type numbers are unchanged. The helper calls GetSysColor
for a system value; otherwise it returns the low 24 RGB bits. A system index has
the same validity and result semantics as GetSysColor. No framework object or
thread-affinity context is required for this helper.

Defaults now store system-color references: ForeColor uses COLOR_WINDOWTEXT,
ordinary BackColor uses COLOR_BTNFACE, and TextBox/Memo/ComboBox/ListBox BackColor
uses COLOR_WINDOW. Defaults supplied by derived factory descriptors are honored,
including RGB defaults and custom Color properties with other names. The generic
Color normalizer and external descriptor validator enforce the same tag contract.

Resolution occurs only at native GDI painting/WM_CTLCOLOR calls, inspector swatch
painting, and standard color-picker initialization. Containers, windowless Label
and Image, edit controls, buttons and item controls retain encoded values in their
model. Native system/theme notifications invalidate affected windows and children;
no resolved RGB or system brush is retained in the property model or image cache.

The descriptor-driven inspector handles NM_CUSTOMDRAW for Color value cells.
Each cell shows a bordered, DPI-sized swatch beside the original stored numeric
value, preserving ordinary text/accessibility information. It clips painting to
the value cell and respects selected/unselected and disabled colors. Other cells
and property types retain their native list-view painting. Swatches resolve system
colors at every paint without modifying the document.

Editing uses the Quadoo Form color picker from QuadooForm.dll: a Vista
palette, categorized system swatches, a single editable preview and hex entry.
Choosing a system swatch preserves its raw reference. The preview's editable
ColorSwatch opens ChooseColorW with the resolved RGB; accepting that nested picker
chooses ordinary RGB. Cancel preserves the preceding value in either dialog.
See [ColorDialogImplementation.md](ColorDialogImplementation.md) for the dialog resource,
hex formats, native entry point and tests.

When the designer places a visual control, it copies writable BackColor and
ForeColor properties from the immediate logical parent before factory creation.
The values are copied through published property interfaces without resolution.
This applies to Forms, Frames, nested Panels/GroupBoxes/ScrollingPanels and
TabControl/TabPage containers, and to external factories exposing those properties.
Missing color descriptors are skipped. A failed read leaves the document unchanged
and creates no child. This is an insertion-time copy, not continuing inheritance;
later parent edits do not rewrite existing children or loaded JSON definitions.
Runtime factory creation and explicit saved colors retain their existing rules.

## Validation

Native color tests cover name/ordinal export lookup, all system-color indexes,
RGB passthrough, tagged defaults, direct container/Label/Image/native control
painting, system notifications, rejection rollback and JSON reconstruction.
Private tests exercise a separately named Accent Color property, large RGB values,
factory default overrides and invalid descriptor defaults. Designer tests cover
Form/Frame insertion, nested containers, one-time copies, native swatch pixels and
selection, persistence, theme notification, standard color-picker initialization,
Cancel/tag retention, and RGB commit.

All VC9 Win32/x64 Debug/Release builds have zero errors/warnings. Focused runtime
color suites pass 68 assertions and focused designer color suites pass 98 in every
configuration. Entire private suites pass 671 assertions; Studio integration passes
87 in every configuration. Independently built extension-DLL coverage remains in
the full suites. No Studio or QuadooScript source was changed.

| Configuration | Full runtime | Full designer |
|---|---|---|
| Win32 Debug | 7,963 passed on unchanged rerun | 3,163 passed |
| Win32 Release | 7,997 passed | 3,189 passed on unchanged rerun |
| x64 Debug | Failed existing choice-control USER-count check; all other groups, including colors, passed | 3,189 passed |
| x64 Release | Failed existing choice/tab USER-count checks; all other groups, including colors, passed | Initial palette USER-count failure; rerun reached existing Preview GDI-count failure |

Assertion totals vary with resource warm-up cycles. Full runtime runs use
`--continue-on-failure` to execute all groups. Initial Win32 Debug full runtime
also encountered sizing/focus and tab USER growth. Initial Release designer runs
encountered palette USER growth; the x64 designer rerun encountered Preview GDI
growth from 26 to 28. These logs remain intact. Resource thresholds, measured
cycles and warm-up limits were not relaxed, and the overall full-suite failures
are not hidden by the passing focused color suites.

To distinguish old resource behavior from the color change, a separate harness
was built against the saved, pre-color QuadooForm/extension DLLs under
`target/QuadooColorsBaseline`. Only diagnostic test selection/continuation was
added to that copied harness. The DLL hash matches the archived baseline exactly.
Its full run reproduced sizing/focus USER growth (5 to 23) and choice-control USER
growth (25 to 27). Its isolated tab resource test passed 446 assertions. The
current isolated tab resource test likewise passes 446 assertions in all four
configurations. The older designer resource intermittency and baseline evidence
are also recorded in DesignerValidation.md and PaletteIconsImplementation.md.
The remaining full-suite resource instability is unresolved.

Logs are `target/QuadooColors-{Debug,Release}-{Win32,x64}-*.log`: `build` and
`harness-build` are compiler output; `native`, `private`, `designer`, and `studio`
are full-suite runs; `colors-native`/`colors-designer` are focused suites;
`tab-resources` is the isolated existing resource group. Full reruns use
`native-repeat`/`designer-repeat`. Baseline evidence is in
`target/QuadooColors-Baseline-{build,native,tab-resources}.log`.

The computer-use skill was used to inspect actual swatch rendering, selected-row
painting and the ordinary RGB color dialog in the built designer. The inspection
instance was closed without saving a document. No system color/theme settings
were changed. Automated tests verify Cancel, RGB commit and system-tag retention.

The ABI audit verifies all 51 existing interface declarations and all 10 previous
exports/ordinals, with only the new export at 15. All four SimbeyCore DLL hashes
match their pre-build values; SimbeyCore was not rebuilt or replaced. The audit
log is `target/QuadooColors-ABI-audit.log`. Actual XP execution and physical theme
or monitor changes remain untested; this pass uses existing VC9/XP-compatible APIs.
