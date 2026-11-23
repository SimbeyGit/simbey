# Stage 3 validation — 2026-10-19

All affected Studio/designer projects build with Visual Studio 2008 with zero
errors and warnings in Debug/Release Win32/x64. SimbeyCore was not rebuilt:
SHA-256 hashes of all four DLLs match the unchanged baseline. CodeEdit and the VM
were reused. The user-provided QuadooIsValidIdentifier parser export was built
and exercised in all four configurations; no handwritten parser/VM source was
changed by this implementation.

| Configuration | Full Studio integration | Full standalone designer | Native harness | Private native |
| --- | ---: | ---: | ---: | ---: |
| Debug Win32 | 227 passed | 5,656 passed | 8,167 passed | 674 passed |
| Debug x64 | 227 passed | 5,656 passed | 8,167 passed | 674 passed |
| Release Win32 | 227 passed | 5,656 passed | 8,167 passed | 674 passed |
| Release x64 | 227 passed | 5,656 passed | 8,167 passed | 674 passed |

The full Studio total includes 114 existing integration checks, 18 package
checks and 95 source synchronization checks. Assertions are not additive with
the focused --source-only command. The full native harness uses its existing
--continue-on-failure mode, which retains failures and a failing exit code.
Resource limits and bounded warm-up were not relaxed.

The complete matrix ran first. Final Studio-only hardening was followed by all
four complete Studio runs again. The standalone/native binaries were unchanged
by that final Studio work.

## Identifier API follow-up — 2026-10-19

After QuadooIsValidIdentifier gained the end-of-input check, Studio's duplicate
character and 128-character checks were removed. Identifier decisions now come
entirely from the API; the helper retains only its UTF-16 string storage
precondition and maps a false result to E_INVALIDARG while preserving API errors.
The standalone designer is unchanged.

Focused source synchronization tests passed with 106 assertions in each of
Debug/Release Win32/x64. Eleven added cases compare Studio's result directly
against the API, covering extra tokens, keywords, empty text, Unicode, whitespace,
comments and a 129-character name. Studio and integration-test builds have zero
errors and warnings. SimbeyCore was not rebuilt.

## New coverage

- New Form/Frame scaffolding includes owned declarations and unit identity from
  creation; no Main.quadoo project entry, filename-derived class/interface names,
  and the initial form remains Default.
- Identifier/keyword validation through QuadooIsValidIdentifier. The initial
  matrix included a Studio character guard, superseded by the API follow-up above.
- Generated script compiles using the existing parser. A real VM fixture creates
  its class, populates/reads a generated property through SetProperty/GetProperty,
  reads the unit-identity property, rejects writing that readonly property, gets
  its generated interface, resolves its method ordinal and invokes the handler.
- The compiler prototype established virtual function as the supported syntax.
  Legacy illustrative virtual bool declarations are not reused as compatible
  methods. New stubs return true; runtime return policy remains deferred.
- Strings/comments, malformed scopes, conditional/macro source, constructor
  initializer lists, lambda-backed fields, duplicate declarations, incompatible
  handlers and reuse of compatible user-authored methods/interfaces.
- Named nested visual controls and nonvisual components; GUID/member mappings;
  rename without handler-body rewrite; container/descendant deletion preserving
  user methods; object paste into another Frame retaining event names and
  reporting missing implementations.
- Explicit handler creation/opening and actual inspector/Form-command routing,
  including generating an empty nonvisual event binding and navigating to code.
- Active/inactive unsaved source ownership, view retention, correct source
  breakpoint movement, and independent Form/Frame buffers.
- Four source preparation/application failure boundaries, native factory
  rejection after successful preparation, clean-buffer dirty-state restoration,
  and unchanged source/native definitions after rejection.
- An adversarial published ITextDocument adapter reports failure after a real
  write, can fail Undo, and can also fail fallback restoration. Tests verify
  exact source recovery, retained originals when restoration is unavailable,
  dirty-state protection and successful explicit retry/reconciliation.
- Project-only saves retain saved declarations and breakpoint positions while
  generated source remains pending. Three-file saves/reopen preserve mappings;
  locking the project after source/definition replacement tests rollback,
  retained journals and successful retry. Existing paired-save, external writer
  conflict, malformed definition and legacy project tests also pass.
- Packages continue to install/use/save/remove through Studio with generated
  component properties and their existing project-wide usage protection.

## Validation notes and limits

The earlier clipboard snapshot blocker did not reproduce in this matrix. The
existing clipboard backup helper was not changed, skipped or weakened. Clipboard
formats were inspected by name only during diagnosis; no clipboard data was
cleared to make the tests pass.

One early Debug x64 standalone run failed the palette focused-row assertion
after posted nonvisual input. A subsequent unchanged Win32 baseline palette suite
passed 1,170 assertions, and all final full designer suites passed 5,656. Palette
focus implementation was not changed in this pass; these results do not claim
that intermittent focus/resource behavior has been repaired. The exploratory
failure is recorded in the task history; Stage3-Debug-x64-designer.log now
contains the final passing matrix run. Final full results use the
configuration/platform logs listed below.

The Studio extension fixture now explicitly unregisters the native factories it
registers before unloading its DLL. The host-wide package registry correctly
survives project closure; project close alone is no longer factory ownership.

Standalone parser dependency builds report the existing .def DESCRIPTION warning
and the intentional /D versus /U warning when selecting local tokenizer tables
for direct project builds. They completed without errors. Parser configurations
were built sequentially after regenerating their shared tokenizer outputs once.

No published QuadooForm ABI or definition schema changed. No additional runtime
control, DLL, Application adapter or Run/Debug/Compile support was introduced.
Source-only Undo/Redo is grouped but is not a coordinated designer/source history;
managed-region divergence is detected and can be explicitly reconciled. Unit
file rename/Save As, automatic merge and power-loss atomicity remain deferred.
Actual XP VM execution and physical monitor/IME testing were not performed.

Logs under target:

- Stage3-<configuration>-<platform>-build-final.log
- Stage3-<configuration>-<platform>-studio-final.log
- Stage3-<configuration>-<platform>-native.log
- Stage3-<configuration>-<platform>-private.log
- Stage3-<configuration>-<platform>-designer.log
- Stage3-validation-summary.log and Stage3-final-studio-summary.log
- Stage3-Parser-<configuration>-<platform>.log

Implementation and usage: [SourceSynchronization.md](SourceSynchronization.md).
The next planned stage is the separate script adapter and application hosts.
