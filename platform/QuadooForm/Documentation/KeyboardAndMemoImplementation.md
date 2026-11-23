# Keyboard and Memo implementation decisions

The current native window subclass dispatches OnKeyDown/OnKeyUp with Key:Int
(read-only virtual key) and Handled:Bool (writable). Their ordinals and arguments
remain unchanged. Add OnKeyPress with the same argument shape, where Key is the
Unicode WM_CHAR UTF-16 code unit; surrogate pairs remain two character messages.
Navigation/system keys do not become synthetic character events.

Add optional IQuadooFormKeyboard::PreprocessMessage(MSG*, BOOL*) to Form, and
IQuadooButton::Click() to Button (new IIDs end 0032 and 0033 respectively). Existing
IIDs/vtables/exports remain unchanged; no exports were added. Frame does not expose
the Form dispatcher or KeyPreview. The new Boolean framework property names reject
incompatible types/flags during factory metadata validation.
The host calls its accelerator translator first, then PreprocessMessage, then
TranslateMessage/DispatchMessage only if unhandled. PreprocessMessage owns the
IsDialogMessage stage: do not call it again for this message. ShowModal and native
interactive hosts use this same interface. Direct SendMessage bypasses host
preprocessing; native control events continue to work as before. No thread hooks.

Routing uses a retained snapshot of the public logical control tree, including
runtime Frames across namespace boundaries, and matches native focus subwindows.
Native WM_GETDLGCODE claims, open ComboBox dropdowns and IME composition take
priority over Enter/Escape arbitration. Unmodified dialog Enter/Escape bypass
KeyPreview; modified combinations retain native behavior. Enter prioritizes the
focused push button, even without Default. Otherwise search nearest logical focus
container outward, using stable ascending sibling TabOrder/preorder. Both flags
are permitted, and ModalResult remains independent. Button activation uses Click,
the same logical path as BN_CLICKED, followed by existing modal completion.

Ordinary unhandled preview reaches the native target once. Consumed keydowns do
not get translated; matching already-queued character messages are suppressed for
that key sequence. Keyup remains independently eligible. Callbacks retain objects
and re-check state/focus before using stale targets. Design suppresses routing;
Preview suppresses application delegates but retains native arbitration/editing.

Memo is a separate EDIT factory sharing text synchronization with TextBox.
Text uses the existing Memo property-editor type, independently of its control
class. Defaults: 150x100, ReadOnly=false, WordWrap=true, ScrollBars=Vertical,
WantTabs=false. Enter is claimed, Tab navigates, Ctrl+A selects all. Horizontal
scrolling is suppressed while wrapping even if requested by ScrollBars. Style
replacement stages a new HWND and preserves focus, selection and edit position.
Programmatic logical Text preserves supplied line endings; the native boundary
converts CR/LF to CRLF. Native user edits publish CRLF text. Existing OnChange
policy applies to changed programmatic values and native edits, once after commit.
The shared TextBox/Memo native-text guard covers only the HWND write, not the
OnChange callback. A callback can assign Text again without leaving the logical
value and native editor out of sync; each distinct committed edit raises its own
OnChange. Publishing a native edit does not rewrite the HWND or reset its caret.

The Form Visible default is explicitly false in FormType for initially hidden
runtime forms. Designer Load uses ShowWindow for hosting without editing it.

References checked: Microsoft WM_GETDLGCODE, IsDialogMessageW and EDIT styles
documentation. The latter requires multiline EDIT plus ES_WANTRETURN for Enter
input and identifies ES_AUTOHSCROLL as disabling wrapping. The proposal's
VCL-inspired dialog-key exception is adopted; scope arbitration is explicitly
QuadooForm policy rather than a claim of exact VCL precedence.

Build/test runs use the existing SimbeyCore DLLs; no SimbeyCore rebuild is required.

## Native host integration

Query a Form for IQuadooFormKeyboard once and retain it while the Form is alive.
For messages in the host's normal loop:

```cpp
if(TranslateAcceleratorW(host, accelerators, &message)) continue;
BOOL handled;
Check(keyboard->PreprocessMessage(&message, &handled));
if(!handled)
{
    TranslateMessage(&message);
    DispatchMessageW(&message);
}
```

Use the host's normal HRESULT/WM_QUIT handling around this fragment. Do not call
IsDialogMessage again, and call the dispatcher only once per original message.
Messages for another Form return unhandled; a destroyed Form returns EDestroyed.
The native harness modeless sample and Form ShowModal have adopted this order.
Preview uses ShowModal and therefore requires no separate keyboard implementation.
No global hook or Frame-local keyboard dispatcher is installed.

An independent button implements IQuadooButton and serializable Boolean Default
and Cancel descriptors to join arbitration. Its Click implements its normal user
activation path. Other native controls claim Enter/Escape through WM_GETDLGCODE;
the independent extension DLL demonstrates this using only IQuadooWinControl and
Win32 subclassing, retaining its module lifetime until its subclass is removed.
Native default-button styling applies only to ordinary push buttons, preserving
owner-drawn/custom styles. It updates on focus, button edits and preprocessing;
the serialized Default flags are never changed by focus.

Tab navigation bypasses Form virtual-key preview. Character Tab input with
WantTabs=true can still be previewed as OnKeyPress. Enter/Escape arbitration is
for unmodified keydown only; modified variants are offered as ordinary input and
then passed directly to native processing, without dialog-button fallback. IME
composition/VK_PROCESSKEY bypass both preview and dialog arbitration. Memo claims
Enter explicitly and releases the multiline EDIT's broad Escape claim when no
IME composition is active, allowing Cancel. A pumped recursive keyboard call
during preprocessing is consumed to prevent reentrant activation. Queued character
suppression uses target HWND and scan code until keyup or the next keydown.

## Sources and coverage limits

[WM_GETDLGCODE](https://learn.microsoft.com/en-us/windows/win32/dlgbox/wm-getdlgcode),
[EDIT styles](https://learn.microsoft.com/en-us/windows/win32/controls/edit-control-styles),
and [IsDialogMessageW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-isdialogmessagew)
describe the native contracts used here. QuadooForm's multiple-button scope/order
rule is its own documented policy; exact VCL arbitration is not claimed.
Live IME composition, physical keyboard layouts/accelerators, physical monitor DPI
changes and Windows XP execution still need manual testing. Automated native
messages, simulated DPI, or a VC9 build do not count as an XP VM run.

## Validation

All seven QuadooForm projects received clean VS2008 Win32 Debug/Release rebuilds
(14 project/configuration pairs) with zero errors/warnings. Final changes were
rebuilt and rechecked in both configurations. SimbeyCore was not rebuilt or
replaced: both existing DLLs match their before-build SHA-256 hashes. Release
validation uses the ordinary target/Release dependencies successfully.

Final Debug and Release public harnesses each pass 7,769 assertions; private native
tests each pass 119. The designer suites pass 1,934 each, including the new 110
keyboard/Memo designer assertions, all legacy groups and strict resource checks.
Focused native tests cover input ordering/handling, UTF-16 and surrogate pairs,
Ctrl+A, modified Enter, thread affinity, native claims, ComboBox dropdown Escape,
two nested runtime Frames, stable tab-order ties, inactive pages, disabled
ancestors, multiple/both flags, recursive input, candidate/target/Frame/Form
destruction, close-query cancellation and module/factory lifetime. Resource tests
retain the existing bounded warm-up and 20 measured cycles, verify captured HWND
destruction and explicitly check Memo factory live counts return to zero.
Reentrant OnChange text replacement is checked on both Memo and TextBox, including
native/logical agreement and exactly one event for each of the two committed edits.

One Release designer run reproduced the existing intermittent late GDI
growth (23 to 25; USER remained 5) in the Preview/resource group. A fresh unchanged
run passed all 1,934 assertions. The failure is retained in
target/KeyboardMemo-Release-designer-resource-failure.log; the unchanged pre-PNG
baseline also reproduced late growth in target/Designer-png-baseline-tests.log.
No resource thresholds, warm-up bounds or measured-cycle counts were relaxed.
This remains a validation limitation, not a claim that the intermittency is fixed.

Build/test logs are target/KeyboardMemo-clean-build.log and
target/KeyboardMemo-{Debug,Release}-{build,native,private,designer}.log. Dependency
and ABI audits are in target/KeyboardMemo-Dependencies-audit.log and
target/KeyboardMemo-ABI-audit.log. All 48 preexisting published interface
declarations match Subversion BASE. Existing exports, ContextOptions layout,
property enum values and key argument shapes are unchanged. New descriptor
properties can shift event-property indexes; clients should resolve by name.
Both published-header/QuadooVM include orders compile with VC9, all seven project
files parse as XML, and the DLL's IME imports are the XP-era IMM32 functions.

DesignerExamples/KeyboardMemo.qform.json is a no-script modal Preview sample.
The runtime/designer --keyboard-only modes run the focused tests. XP, live IME
composition, physical keyboard layout/accelerator and monitor/theme combinations
remain untested beyond the native-message and simulated-DPI cases above.
