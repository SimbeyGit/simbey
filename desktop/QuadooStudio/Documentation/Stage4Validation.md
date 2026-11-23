# Stage 4A validation — non-debug Application Run

For the 2026-10-22 focus events, real QuadooPlugin, Packages menu and test-project
relocation follow-up, see [FocusAndPluginValidation.md](FocusAndPluginValidation.md).
For Application icon/module settings and the current complete Studio results,
see [ApplicationSettings.md](ApplicationSettings.md).
The results below record the original stage 4A delivery.

Implemented and validated 2026-10-20 with Visual Studio 2008, using individual
project builds and the existing JSON, QuadooVM, CodeEdit and SimbeyCore binaries.
All four SimbeyCore.dll SHA-256 hashes match the saved baseline; it was not rebuilt.

## Delivered behavior

Application Run/Ctrl+F5 synchronizes and saves the associated units, compiles all
project scripts through an in-memory startup source, copies AQVMT.bin, embeds
QBC/catalog/form/frame definitions, and launches a project-named EXE under
bin/Win32 or bin/x64. No Main.quadoo is written, added to the project or opened.
Generated WinMain instantiates the Default form and calls Host.RunApplication to
show it nonmodally and pump messages. Arguments and startDir use project settings.

AQVMT is in QuadooStudio.sln and subclasses CQVMWinHost. Its IExternalClassLoader
looks up the script class in embedded resources and returns a Create-only
definition loader. Form.Create(this) returns a distinct live IQuadooForm wrapper.
Frames use extern(Frame) and Frame.Create(this, Owner), returning a live
IQuadooFrame wrapper. Both wrappers share a container base.

Studio generates __QuadooRoot so PostBind can install the live root and named
members before OnCreate. Missing constructors are inserted conservatively;
existing user constructors are preserved. Updated examples include Form/Frame
constructors, the root accessor and #pragma once.

Descriptor properties/events and capability queries support independent native
factories. The script/native contract is documented in
[the adapter README](../../platform/QuadooForm/ScriptAdapter/README.md).
No public QuadooForm or QuadooVM interface, export or JSON schema changed.
QuadooForm.dll remains independent of QuadooScript.

The shared WQVM pump now releases retained handlers and handles GetMessage == -1
without losing controller cleanup. WQVM and WQVMT were rebuilt in all four
configurations. The preprocessor's repeated #pragma once lookup uses HasItem,
avoiding a null output argument to the existing map Find implementation.
No VM implementation change was needed.

## Results

| Suite | Debug Win32 | Debug x64 | Release Win32 | Release x64 |
| --- | --- | --- | --- | --- |
| Application runtime, real parser/VM and independent extension | 81 passed | 81 passed | 81 passed | 81 passed |
| Full Studio integration, packages, source sync and application builder | 267 passed | 267 passed | 267 passed | 267 passed |
| Native QuadooForm harness | 8,167 passed | 8,167 passed | 8,167 passed | 8,167 passed |
| Native metadata/storage/private tests | 674 passed | 674 passed | 674 passed | 674 passed |
| Full designer suite | 5,656 passed | 5,656 passed | stopped at old Preview resource check | 5,656 passed |

The affected projects and both legacy Windows pump consumers build successfully
in Debug/Release Win32/x64. Direct parser builds use the existing SimbeyCore
import library and local tokenizer tables; configurations are built sequentially.
The existing DESCRIPTION linker warning and local-table define/undefine warning
remain unrelated to the new runtime projects.

Runtime coverage includes:

- Actual extern(Form)/extern(Frame) construction; a loader that cannot expose live
  properties before Create; two form types; required frame ownership and two
  independent instances of one frame template.
- Root/member availability before OnCreate, interface/ordinal delegate binding,
  Boolean false callback success, constructor failure cleanup and recovery.
- Raw Quadoo Colors, structured Font round trips, IRStringArray snapshots and
  copying replacements, independent factory properties/layout/mutable events.
- Accepted/cancelled closes, user OnDestroy binding changes without losing
  internal tracking, Timer shutdown, default-button keyboard routing, nested
  WM_QUIT 123 propagation, injected GetMessage failure and controller cleanup.
- Retained destroyed wrappers, readable retained writable event arguments with
  precise rejection of late writes, factory destruction/module unload, and an
  unhandled Timer exception unwinding the real pump with a failure diagnostic.

Builder coverage includes all project files, overlapping pragma-once includes,
generated Frame constructors, class-named embedded definitions, no Default,
missing/invalid AQVMT, user WinMain collision, four failure checkpoints and a
locked output. Previous good EXEs are compared byte-for-byte after failures.
The output is launched from the Windows directory after all original script and
definition paths have been moved out of reach; it closes normally with exit zero.
No designer package loading code is linked into AQVMT.

## Existing regression failures retained

The first Release Win32 designer run and its isolated palette recheck failed
PaletteTests.h line 387: nonvisual tray count/Pointer selection after insertion.
The final full run passed the palette group, then stopped in the existing
Designer/Preview resource test: GDI 29 to 30, USER 5 to 5, at measured cycle 3
after three stable warm-up cycles. It completed 2,996 assertions before stopping.
The earlier palette stop completed 150 assertions; the focused recheck stopped
at 22. No designer, palette or resource-threshold code was changed in this pass.

The Preview GDI growth is already tracked in DesignerValidation.md. A successful
run in another configuration does not establish that either failure is repaired.
Existing assertions and resource thresholds were preserved.

## Limits and follow-ups

- Local debugging, Compile/export UI, remote staging and full redistribution are
  deferred. AQVMT has the actual shared-host dependency on QuadooParser.dll as
  well as QuadooForm, QuadooVM, JSON and SimbeyCore; Run stages these adjacent DLLs.
- General external factory bootstrap and module deployment still need their own
  contract. AQVMT now supplies a real QuadooPlugin component and module-object
  substitution, as covered by the follow-up above. Design-time placeholders are
  never loaded as runtime substitutes.
- Extension-specific native methods need an explicit future script capability
  adapter; custom descriptor properties/events already use the general bridge.
- Output publication rolls back ordinary failures and retains backups if rollback
  itself fails. It has no crash/power-loss journal. Existing source-save recovery
  continues to handle coordinated source/definition/project saves.
- Actual Windows XP VM execution, physical monitor/IME testing, exhaustive UNC/
  Unicode directory cases, and real disk-full/process-launch failure injection
  were not performed. Existing XP-compatible APIs and VC9 settings were retained.

Logs under target use these prefixes:

- Stage4-Configuration-Platform-final-build and -WQVM/-WQVMT-build.
- Stage4-Configuration-Platform-runtime-final-build and -runtime-final (81 checks).
- Stage4-Configuration-Platform-final-QuadooStudioIntegrationTests (267 checks).
- Stage4-Debug-Platform-QuadooFormHarness/-QuadooFormNativeTests/-QuadooFormDesignerTests.
- Stage4-Release-Win32-QuadooFormHarness/-QuadooFormNativeTests, and
  -final-QuadooFormDesignerTests; the initial and isolated palette logs are retained.
- Stage4-Release-x64-final-QuadooFormHarness/-QuadooFormNativeTests/-QuadooFormDesignerTests.

Implementation sequence and remaining scope:
[Stage4ApplicationRuntimePlan.md](Stage4ApplicationRuntimePlan.md).
