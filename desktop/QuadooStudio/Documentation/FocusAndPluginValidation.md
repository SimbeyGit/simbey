# Focus events, runtime QuadooPlugin and test relocation

Implemented and validated 2026-10-22 using Visual Studio 2008 in Debug/Release,
Win32/x64. No public IID, vtable, export or JSON schema changed. QuadooForm.dll
remains independent of QuadooScript. SimbeyCore was not rebuilt; all four DLL
SHA-256 hashes match the saved baseline.

## Delivered behavior

- Built-in HWND controls inherit OnEnter/OnLeave descriptors at CWinControl.
  Enter follows outer-to-inner native ancestry; leave follows inner-to-outer
  ancestry. Common containers remain entered during sibling changes. Internal
  ComboBox edit HWNDs and native replacement do not duplicate logical events.
  Focus redirection and destruction during callbacks retain safe references and
  stop stale traversals. Labels, other windowless controls and nonvisual
  components are excluded. Existing OnFocus/OnBlur behavior is preserved.
- AQVMT registers an independent real nonvisual QuadooPlugin factory before
  loading definitions. PostBind loads Module, retries with a `Modules\` prefix
  on failure, and puts the returned object in the named form/frame member before
  any OnCreate callback. Both failures roll back construction. Teardown releases
  module references and preserves a user's replacement of the member. Factory
  registration failure preserves an existing factory; the host service is
  disconnected before host teardown.
- Studio's Form menu now contains Packages. Its existing always-enabled command
  routing and shared package manager are reused, including with no designer.
- ApplicationTests moved from platform/QuadooScript to
  `tests/desktop/QuadooStudio/ApplicationTests`. Solution, project, includes, build and
  validation paths were updated. All 98 project file entries exist; the old
  directory is absent and active sources/projects/docs have no old references.

Runtime module behavior and script ownership are described in the
[adapter README](../../platform/QuadooForm/ScriptAdapter/README.md). Modules use
the existing host loader's path/working-directory rules. Automatic deployment of
module files and general external-factory bootstrap remain separate work.

## Validation results

Every affected project builds successfully in all four configurations.

| Suite | Debug Win32 | Debug x64 | Release Win32 | Release x64 |
| --- | --- | --- | --- | --- |
| Full native harness | 8,270 passed | 8,270 passed | 8,270 passed | 8,270 passed |
| Native storage/metadata/failure injection | 674 passed | 674 passed | 674 passed | 674 passed |
| Focus entry/leave, ancestry, replacement, reentrancy, serialization | 51 passed | 51 passed | 51 passed | 51 passed |
| Application runtime and plugin failure/lifetime tests | 103 passed | 103 passed | 103 passed | 103 passed |
| Generated executable build/run, direct and Modules fallback | 58 passed | 58 passed | 58 passed | 58 passed |
| Studio Packages/menu | 19 passed | 19 passed | 19 passed | 19 passed |
| Studio source synchronization | 106 passed | 106 passed | 106 passed | 106 passed |
| Designer keyboard | 109 passed | 109 passed | 109 passed | 109 passed |
| Designer nonvisual components and placeholders | 877 passed | 877 passed | 877 passed | 877 passed |
| Designer packages | 318 passed | 318 passed | 318 passed | 318 passed |

Plugin tests use the real parser/VM, direct script and fallback bytecode modules,
missing modules, non-object returns, thrown exceptions, component lifecycle
delegates, failed registration, user member replacement and factory live counts.
The generated executable tests run after original form/source paths are hidden;
their runtime module fixtures are available separately. AQVMT does not link the
design-time package loader. The relocated application test project was rebuilt
in every configuration.

Full Studio and designer suites were also attempted in all four configurations.
They stopped at the existing clipboard-preservation helper: Windows advertises
`application/x-moz-nativeimage`, but GetClipboardData returns NULL for it. Studio
stopped at 42 assertions and designer at 1,610, with HRESULT 0x8007054F. Clipboard
contents were left intact. These full suites are **not** reported as passing;
the focused suites above cover the affected paths and later test groups.

The initial Debug native runs found stale fixed descriptor counts after adding
the two events. Expectations were updated and both full Debug reruns passed.
Existing resource thresholds were preserved. Actual XP VM execution was not
performed; the existing VC9/XP API and compiler settings were retained.

Logs are under target/FocusPlugin-Configuration-Platform-*.log. Final focused
results use `-final-`; full Studio/designer attempts and Release native/private
results use the original prefix. Debug full native reruns use `-final-native`.
The final validation driver is target/FocusPlugin-final-validate.ps1.
