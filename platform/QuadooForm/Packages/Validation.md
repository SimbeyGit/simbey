# Package feature validation — 2026-10-18

VS2008 builds completed with zero errors and warnings in every configuration.
SimbeyCore was not rebuilt; SHA-256 checks of all four binaries match the
unchanged baseline.

| Configuration | Shared package tests | Studio package tests | Full native harness | Private native tests |
| --- | ---: | ---: | ---: | ---: |
| Debug Win32 | 318 passed | 18 passed | 8,167 passed | 674 passed |
| Release Win32 | 318 passed | 18 passed | 8,167 passed | 674 passed |
| Debug x64 | 318 passed | 18 passed | 8,167 passed | 674 passed |
| Release x64 | 318 passed | 18 passed | 8,167 passed | 674 passed |

Package coverage includes strict schema/default validation, duplicate JSON keys
and escaped aliases, reserved framework property names, all supported custom
value codecs, immutable defaults, Release-safe indexes, event-name persistence,
COM identity, owned palette icons, source-sidecar embedding, self-contained
installed JSON, constructor/destructor and retained-factory lifetimes, rollback
at six install and three removal boundaries, unreadable definitions, unopened
and inactive/unsaved Studio forms, actual cross-process usage leases, native
factory identity preservation, visual leaf/container capabilities, nested
layout, windowless child painting and normal/print-client rendering.

The package GUI tests open the shared resource-backed Win32 dialog, verify its
icon list and close it. They exercise the always-enabled Studio command without
a project and in Code view. Installing another package from Code view and then
activating Design view verifies that the hosted tools list remains visible.
The same shared sources/resource are built directly into the standalone host.

Resource coverage uses the existing strict GDI/USER monitor, its unchanged
bounded warm-up and 20 measured cycles, including nested controls, DPI changes,
fonts, icons and destruction. No resource limit was relaxed.

An independent real QuadooPlugin component fixture, implemented through the
published interfaces, is registered in a fresh Runtime registry before loading
the saved form. It verifies successful construction, Module values/metadata
compatibility, event-name resolution, destruction and factory lifetime without
consulting the installed package catalog. Missing real factories and attempts
to use design-only factories in Preview fail and preserve the designer document.

The complete designer and Studio suites were attempted in all four
configurations. They consistently stop at the existing clipboard snapshot
helper, ClipboardTests.h line 51: GetClipboardData fails with 0x8007054F while
backing up an existing clipboard format. Designer runs reach 1,610 assertions;
Studio runs reach 42. The clipboard helper and its requirements were not changed
or bypassed. These full-suite runs are therefore not reported as passing.

The remaining independently runnable designer groups pass in all configurations:

| Designer group | Assertions |
| --- | ---: |
| Tab page selection/placement | 74 |
| Keyboard/Memo | 109 |
| PNG import and failures | 106 |
| Components and lossless fallback | 877 |
| Image control/editor | 31 |
| Color picker dialogs | 1,803 |
| Colors and parent color copying | 138 |
| ColorSwatch inspector/persistence | 18 |

Logs are under target:

- Packages-<configuration>-<platform>-build.log
- Packages-<configuration>-<platform>-packages-final.log
- Packages-<configuration>-<platform>-studio-packages-final.log
- Packages-<configuration>-<platform>-native.log and -private.log
- Packages-<configuration>-<platform>-designer.log and -studio.log
- Packages-<configuration>-<platform>-<designer-group>.log
- Packages-validation-summary.log, Packages-final-summary.log and
  Packages-focused-summary.log

No published QuadooForm IID, vtable, export or definition-schema change was
needed. PackageServices.h contains private native designer-host services only.
No runtime controls, QuadooScript integration, new DLL or QuadooForm.dll dialog
resource was introduced. Windows XP execution and physical monitor/DPI changes
were not performed; VC9 and XP-compatible APIs are retained.
