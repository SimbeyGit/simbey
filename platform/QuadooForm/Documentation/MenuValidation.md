# Menu implementation validation

Validated with Visual Studio 2008 on the development Windows host. All affected
runtime, extension DLL, designer, Studio, AQVMT and test projects build in all four
configurations. No existing published interface vtable changed; menu capabilities
are additive QueryInterface interfaces. SimbeyCore DLL hashes are unchanged.

| Configuration | Native harness | Private native tests | Designer | AQVMT scripts | Studio integration |
| --- | ---: | ---: | ---: | ---: | ---: |
| Debug Win32 | 8,536 passed (rerun) | 684 passed | 21,310 passed | 106 passed | 389 passed |
| Debug x64 | 8,536 passed | 684 passed | 21,310 passed (rerun) | 106 passed | 389 passed |
| Release Win32 | 8,536 passed | 684 passed | 21,310 passed | 106 passed | 389 passed |
| Release x64 | 8,536 passed | 684 passed | 21,310 passed | 106 passed | 389 passed |

The focused package suite also passes 318 assertions in each configuration after
correcting the command-line string handling in its cross-process test helper.

## Coverage

- Native menu hierarchy, item states, separators, checked-state toggling,
  command routing, nested commands and normal delegate binding.
- Main-menu accelerators and popup shortcuts, disabled state and ancestry.
- Main-menu assignment/clearing, caption/visibility changes, deferred replacement
  during native menu loops, and destruction from event callbacks.
- Exact client dimensions at narrow and wide widths, native wrapping, unchanged
  child coordinates, and border-style transitions.
- Simulated menu measurement, wrapping and painting at 96/144/192 DPI; direct
  caption selection; tree/inspector selection synchronization.
- Menu editor insertion, deletion, sibling order, fields, flags, named handlers,
  OK/Cancel behavior and Studio source generation.
- JSON GUID persistence, malformed hierarchy rejection, clipboard array copying,
  descendant preservation and GUID remapping on paste.
- Modal Preview with a real attached menu; frame-owned popups without frame menu
  bars; native menu/accelerator resource cleanup.
- Failure-injected layout rollback preserving the old native menu and definition.
- Generic uniqueness using an unrelated capability key, duplicate registration
  rollback and independent runtime-frame scopes.
- Published-interface COM identity paths for menu roots and items.

## Corrections found by the full suites

Visual package placeholders inherit PopupMenu from the base visual descriptors.
Their property normalizer now accepts its null ObjectReference default. The full
package construction/property/serialization tests exercise this path.

The package test child previously passed argv text directly to RSTRING_CAST.
Command-line strings can be only two-byte aligned, whereas tagged static RSTRING
pointers require four-byte alignment. Some runs therefore leased an empty type
name instead of QuadooPlugin. The child now creates a managed string. This was a
test-helper defect; the production package catalog already uses managed names.
The original ten-second helper crash timeout is retained.

One full Debug Win32 run observed Windows-created SystemUserAdapterWindowClass
and OleMainThreadWndClass windows appearing after the older Choice resource test
froze its baseline. The complete harness passed on rerun with its original strict
resource thresholds. An earlier Release x64 keyboard run also failed transiently;
its focused rerun and the final full Release x64 harness both passed. These original
failure logs are retained; no runtime behavior or leak thresholds were relaxed.

## Reproduction and logs

The workspace validation drivers are `target\Menus-final-validate.ps1`,
`target\Menus-post-validation.ps1` and the focused package invocations. Build and
full-suite logs use `target\Menus-final-<configuration>-<platform>-*` names.
Successful replacement runs are:

- `target\Menus-post-Debug-Win32-QuadooFormHarness.log`
- `target\Menus-post-Debug-x64-QuadooFormDesignerTests.log`
- `target\Menus-package-<configuration>-<platform>-test.log`

All five full suites include the new menu tests. Native/designer/Studio executables
also accept `--menus-only` for focused checks. Test drivers build affected projects
directly and reuse SimbeyCore and the existing VM/parser libraries.

Actual Windows XP VM execution was not performed. The implementation retains the
VS2008 toolset, XP-compatible native menu/GDI APIs and optional dynamically loaded
theme support. See [MenuSupport.md](MenuSupport.md) for usage and API contracts,
and [the sample form](DesignerExamples/Menus.qform.json) for a complete definition.

## Designer reference assignments (2026-10-24)

New MainMenus connect automatically when created through the palette, including
its managed type-name strings. The designer queries the public IQuadooMenu
capability rather than comparing RSTRING pointer addresses. Property editing now
uses an inline, noneditable component drop-down for ObjectReference descriptors,
with menu-kind filtering for MainMenu/PopupMenu and a (none) entry for clearing.

Regression coverage exercises actual palette/list-view notifications, native
combo notifications and keyboard input, assignment/clearing, Escape/cancel,
switching back to a text editor, frame popups, and JSON round trips. A package's
Target property verifies generic ObjectReference editing and preservation of
unresolved values. Studio exercises the same inspector while generating/saving
form source. Focused menu/reference tests pass 103 assertions in every build;
focused package checks pass 343 assertions, including Release x64.

The shared core, stand-alone Designer, Studio and both test executables build
without compiler warnings in VS2008 Win32/x64 Debug/Release. Full-suite results:

| Configuration | Designer | Studio integration |
| --- | ---: | ---: |
| Debug Win32 | 21,425 passed | 399 passed |
| Debug x64 | 21,425 passed | 399 passed |
| Release Win32 | 21,451 passed | 399 passed |
| Release x64 | Preview resource-count check failed | 399 passed |

Assertion counts vary with the resource monitor's bounded warm-up cycles. All
existing thresholds remain unchanged. The first Debug Win32 full designer run
observed GDI=30 against a frozen baseline of 28 during the existing extensions/
Preview resource test; its complete rerun passed.

The Release x64 full designer suite failed that same existing check on three
runs, after its menu and inline-editing tests passed. The final run recorded
GDI=30 versus baseline 28, USER=5 unchanged, at measured cycle 5. This resource
sampling failure has been observed before, but its cause was not isolated in
this change. The full Release x64 designer suite is therefore not green; the
focused menu/reference and package suites and full Studio integration do pass.
No test thresholds or warm-up rules were weakened to obtain those results.

Build/test logs and reproduction drivers use target\References-* names. The
Release x64 full-designer attempts end in -test.log, -rerun-test.log and
-final-test.log. Final focused menu logs use References-final-menus-* names; the
first Debug Win32 full-suite failure is retained in -pretest.log. No published
ABI changes were required. Runtime dependencies were reused without rebuilding;
all four SimbeyCore DLL hashes still match the prior baseline. Actual Windows XP
VM execution was not performed.

## Inline enum and Boolean editors (2026-10-24)

The shared inline dropdown now handles descriptor-defined enum choices and all
Boolean properties, including BorderStyle, WindowState, FormStyle and KeyPreview.
It preserves the numeric enum value or JSON Boolean type rather than saving the
label or selection index. Menu/reference fields retain their existing filtered
choices and GUID storage. Flags retain the checkbox editor. Enter/selection apply,
Escape cancels, Tab advances through editable dropdowns, and read-only descriptors
remain protected. Unknown existing enum values are preserved until changed.

The shared core, stand-alone Designer, Studio and both test executables build
with no compiler warnings in VS2008 Win32/x64 Debug/Release. Each of the four
configurations passes these focused suites:

- Inspector editing: 288 assertions (--editing-only), covering enum/Boolean
  labels, selection, no-change/cancellation, invalid-selection rejection,
  text/dropdown switching, tab navigation, typed JSON round trips, existing
  string/event editors and the full menu/reference tests.
- Package properties: 361 assertions (--packages-only), including custom enum
  values 4/8, Boolean JSON values, read-only Boolean properties and references.
- Studio menu/inspector integration: 45 assertions (--menus-only), including
  all three form enums and KeyPreview through the embedded inspector.

Reproduction: target\InlineChoices-build.ps1 and
 target\InlineChoices-validate.ps1. Logs use target\InlineChoices-* names.
No published ABI changes or runtime dependency rebuilds were required; all four
SimbeyCore DLL hashes remain unchanged. Full suites were not rerun for this
focused UI change; the previous Preview resource-count issue above remains
unresolved. Actual Windows XP VM execution was not performed.
