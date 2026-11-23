# ImageButton implementation and validation

Implemented October 27, 2026, following QuadooForm_ImageButton_Plan.md. The
scheduled run did not unlock the workstation, drive its GUI, or execute tests
that create HWNDs. Native, Designer, Studio and application-runtime GUI tests
were compiled and deferred at that point. The later authorized GUI validation
and shared button-close correction are recorded below.

## Control and public interfaces

The `ImageButton` visual factory creates `CImageButton : CButton`, using a real
Win32 BUTTON with `BS_OWNERDRAW`. Button input, focus, accessibility text, Click,
Default/Cancel, ModalResult, PopupMenu and lifecycle behavior remain inherited.
The factory composes Button metadata and adds these serializable properties:

| Property | Default | Meaning |
| --- | --- | --- |
| Picture | null | Original embedded PNG/ICO image. |
| DisabledPicture | null | Explicit disabled image; otherwise Picture is grayscaled. |
| ImagePosition | Left | Left or Top relative to Caption. |
| ImageWidth / ImageHeight | 16 / 16 | Bounding cell at 96 DPI, each 1..256. |
| Spacing | 4 | Logical image/caption gap, 0..1024. |
| Interpolation | HighQuality | Existing Nearest/Bilinear/Bicubic/HighQuality choices. |

The image and caption form one centered group. Images retain their aspect ratio.
An image cell is reserved whenever either source is assigned, preventing caption
movement on enable/disable. With neither source, the caption alone is centered.
Requested bounds are unchanged; the inherited default is 100x28 logical pixels.

The only public ABI addition is the optional `IQuadooButtonPresentation` interface,
IID `{D25A0D6A-34B4-48EA-AB90-3C8AB72A0049}`, with
`SetDefaultAppearance(BOOL)`. Existing interfaces, exports and ordinals are
unchanged. Ordinary Button keeps its native default style; ImageButton retains
BS_OWNERDRAW and changes transient presentation state. Neither path changes the
Default property or emits application callbacks.

Default-button policy queries this capability rather than checking native class
names. Parent-directed control WM_DRAWITEM is routed through public container,
WinControl and MessageHandler interfaces, keyed by DRAWITEMSTRUCT.hwndItem.
Menus remain outside this routing. Independently implemented native containers
must provide the same forwarding: call the registered child's message handler
with the child's HWND, the unchanged WM_DRAWITEM parameters, and honor its
handled/result outputs. They do not need an internal runtime base class.

## Painting, images and DPI

The shared renderer dynamically obtains XP-era BUTTON theme functions, and uses
DrawFrameControl/system colors when theming is unavailable. It handles disabled,
pressed, hot, focused and default states, mnemonics/UI cues and WM_PRINTCLIENT.
Painting uses a bounded memory DIB and preserves the caller's DC. Oversized
control bounds use clipped direct painting instead of an unbounded buffer.
Ancestor disabling also selects disabled presentation.

Image-cell dimensions and spacing scale at the painting boundary. Stored JSON
remains in 96-DPI units. ICO entry selection uses the physical cell dimensions;
the original ICO container is preserved. Existing image validation, Base64,
resource budgets and PNG/ICO decoding are reused. Replacement decodes before
committing its resource; failures restore the previous property and usable image.

Automatic grayscale uses `(77*R + 150*G + 29*B + 128) >> 8` on premultiplied
channels and preserves alpha. The derived disabled DIB is cached separately
from the normal raster and never serialized. Cache keys include physical size,
DPI and interpolation. Explicit DisabledPicture is painted unchanged. Legacy
destination-XOR ICO pixels are composed against the current button background;
only covered image contributions are desaturated, preserving colored backgrounds
at translucent edges. This exceptional composition is performed per paint;
decoded/scaled source data remains cached.

Paint operations retain image references across parent-background callbacks and
guard theme/DLL lifetime against reentrant destruction. Resources are released
on replacement, clear, destruction and external native-window destruction.

The allocation-failure sweep exposed an existing error in the bundled lodepng:
fixed Huffman-tree construction ignored allocation failures and could dereference
an incomplete tree. Its two construction results now propagate to the inflate
error path. This source is compiled directly into the affected projects; no
SimbeyCore, JSON, script compiler or VM DLL was rebuilt.

## Designer and script integration

Both hosts discover the new factory and its 20x20 palette icon through the normal
interfaces. ImageButton.ico is a standard QuadooForm.dll resource; ImageButton.png
and the matching code-authored icon source are under Icons. The existing Picture
editor handles both images, and existing inline enum editors handle orientation
and interpolation. Common selection properties, source transactions and script
descriptor access remain generic.

Studio generates one member for the button. No members are generated for its
pictures. The application fixture exercises image properties, Enabled and Click
through the existing runtime adapters. There are no QuadooScript changes.
DesignerExamples/ImageButtons.qform.json embeds complete sources and demonstrates
both layouts, automatic grayscale and an explicit disabled image.

## Permitted validation results

- VS2008 Win32 Debug and Release builds of QuadooForm, private native tests,
  harness, shared Designer core, standalone Designer, Designer tests, AQVMT,
  Studio, Studio integration tests and application tests completed successfully.
- Each configuration passed 711 assertions through the dedicated non-windowed
  ImageButton entry point, including the existing non-windowed image-renderer
  regression. Coverage includes descriptors/ranges, embedded icon dimensions,
  independent public-interface draw routing, default presentation, COM identity,
  JSON round trips, 96/144/192-DPI persistence, image replacement failure sweeps,
  factory lifetime, disabled-cache allocation failures/recovery, alpha/grayscale,
  ICO resolution selection, legacy XOR,
  clipped layouts, memory-DC painting and repeated-paint GDI cleanup.
- Both configurations compiled the full application script fixture without
  executing it: 27,583 bytes, HRESULT S_OK. This validates script syntax, not
  runtime property/event dispatch.
- Seven modified project files passed XML/path/duplicate checks. Both JSON
  fixtures and their eight embedded pictures passed format/Base64 checks. The
  palette PNG/ICO are 20x20; the ICO uses an uncompressed 32-bit DIB and AND mask.
- The executed headless entry returns before CoInitialize; compile-only returns
  before DPI/common-control/OLE initialization. Neither launches a form or test
  desktop. No broad GUI suite was executed.
- Both runtime DLLs are Win32 PE images with subsystem version 5.0. Import
  inspection found no static UxTheme, new DPI, or buffered-paint imports. Existing
  SimbeyCore/JSON hashes were unchanged; compiler/VM timestamps predate this run.

From `D:\dev\projects\trunk`, reproduce the allowed build/test run with:

```powershell
& .\tests\platform\QuadooForm\test-imagebutton.ps1 -Configuration All -Build
```

The dedicated build script builds individual projects, excluding dependency DLL
projects. Logs are written under `tests\results\ImageButtonLogs` for each configuration.
Without `-Build`, the same runner tests existing binaries only.

## GUI validation follow-up

The user authorized GUI checks later on October 27. Both Win32 configurations
built successfully without warnings and passed these checks:

| Suite | Debug assertions | Release assertions |
| --- | ---: | ---: |
| Focused ImageButton native GUI and shared button completion | 119 | 119 |
| Focused ImageButton shared Designer | 7 | 7 |
| Focused ImageButton Studio/source persistence | 4 | 4 |
| Full application script runtime | 116 | 116 |
| Full native harness | 9,541 | 9,541 |
| Full private native suite | 1,544 | 1,544 |
| Shared palette/icons/scroll/resource suite | 1,402 | 1,402 |
| Designer keyboard properties and modal Preview | 109 | 109 |
| Designer image rendering/editor and Preview | 46 | 46 |

The broader tests exposed stale palette row/image counts after the new factory
was added. Those expectations now include ImageButton and explicitly check its
palette entry and image. The runtime factory-catalog count is updated as well.

The user's Cancel/ModalResult issue was reproduced by a failing regression:
`CForm::CompleteModal` rejected every completion when the form was modeless.
It now applies a nonzero result and runs the normal Close/OnCloseQuery path for
both modal and modeless forms. A veto or failed close that leaves the form open
restores the previous result. Zero remains an ordinary click; Cancel selects the
Escape target and
Default selects the Enter target. The existing public interface and vtable are
unchanged; its comment now documents completion for modeless forms.

New native cases exercise Button and ImageButton inside a Panel through Escape,
Enter, native BM_CLICK and IQuadooButton::Click. Every case first verifies a
zero-result click, then a vetoed close with the previous result restored, and
finally successful destruction with exactly one OnClose and the requested result.
The existing modal-owner, Preview, cancellation and resource tests also pass in
the full native suite.

The complete Designer/Studio suites encounter the pre-existing clipboard fixture
failure documented in ListViewStage5.md: GetClipboardData fails with 0x8007054F
while CTestClipboard::Save attempts to preserve the current clipboard. No
clipboard formats were discarded and no assertion was weakened to bypass it.
The full Designer run reaches 2,248 assertions before that fixture failure; full
Studio reaches 174. Focused ImageButton and palette suites pass independently.
These ambient-fixture failures are not recorded as full-suite passes.

Visual inspection using the computer-use skill confirmed Left/Top arrangements,
automatic grayscale, the unchanged explicit disabled image, native-style chrome
and the palette icon in the Release Designer and its live Preview. Clicking
Preview's Close button returned result 2. The example was closed without changes.
Escape/Enter completion is covered by the native regression above.

Logs are under tests/results/ImageButtonLogs, including the per-suite full validation
logs and gui-full-results.json. Use the following focused command to repeat the
GUI checks:

```powershell
& .\tests\platform\QuadooForm\test-imagebutton.ps1 -Configuration All -Gui
```

That opt-in runs the focused native, shared Designer and Studio ImageButton tests,
then the full application runtime fixture. The native tests cover BS_OWNERDRAW,
Enter/Escape/Space/default routing, Click counts, printing, ancestor disabling,
physical bounds at 144 DPI, and modal completion. Designer tests cover properties,
reload, copy/paste payloads, new identities and mixed selection. Studio tests
cover generated members and save/reopen. The runner does not run all existing
native/Designer suites automatically.

Run the broader related/full suites separately on an interactive desktop:

```powershell
foreach ($imageConfiguration in 'Debug','Release')
{
    & ".\tests\results\bin\$imageConfiguration\QuadooFormHarness.exe"
    & ".\tests\results\bin\$imageConfiguration\QuadooFormNativeTests.exe"
    & ".\tests\results\bin\$imageConfiguration\QuadooFormDesignerTests.exe"
    & ".\tests\results\bin\$imageConfiguration\QuadooStudioIntegrationTests.exe"
}
```

## Remaining manual and XP validation

Check both orientations in Designer, Preview and a generated Application;
mouse capture/release, mnemonic and Tab behavior; nested containers/frames;
PopupMenu; all theme/default/focus/UI-cue states; parent disable/re-enable; image
replace/clear; tiny/long-caption buttons; DPI transitions; and repeated creation,
theme changes and destruction with stable USER/GDI resource counts. Verify both
the explicit disabled source and the automatic fallback, including translucent
PNG and legacy ICO edges.

On the actual XP VM, test Release binaries with the existing dependency DLLs,
themed common controls and classic appearance. Confirm palette/icon loading,
painting and input, modal behavior, scaling and cleanup. Local builds, import
inspection and headless GDI tests do not establish an actual XP pass.
