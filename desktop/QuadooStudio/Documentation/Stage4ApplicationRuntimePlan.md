# Stage 4: Application runtime and AQVMT

Status: stage 4A implemented, 2026-10-20. AQVMT, the separate script adapter,
embedded definitions and non-debug Application Run are delivered. Local debugging
and Compile/export remain follow-ups. See [Stage4Validation.md](Stage4Validation.md).
This refines stage 4 of [the integration plan](QuadooFormApplicationIntegrationPlan.md).

Uniform lifetime follow-up, October 28, 2026:
[ApplicationLifetimeImplementation.md](ApplicationLifetimeImplementation.md)
implements the application session policy from
[ApplicationLifetimePlan.md](ApplicationLifetimePlan.md). Generated script WinMain
calls a helper to create/show the startup Form, then calls the argument-free
blocking host pump. Default selects startup only, and final Form destruction
ends normal execution. The host drains queued script destructors before deciding
exit, allowing them to create replacement Forms. Local debugging is next.
Section 10 and Stage4Validation.md retain historical stage 4A delivery notes.

## 1. Outcome and scope

Create a native Windows GUI executable project named **AQVMT** (Application QVM
Template), patterned after WQVMT. Its output is **AQVMT.bin** in the normal
Debug/Release Win32/x64 target directories. The `.bin` is a Windows PE executable
template, not QVM bytecode. Add the new project to
`desktop/QuadooStudio/QuadooStudio.sln`, under its Platform folder, with all four
configuration mappings and the build ordering needed by Application Run. AQVMT
is a separately launched executable template; Studio does not link its executable
code. Targeted validation builds continue reusing the existing SimbeyCore DLLs.

AQVMT's **Host object implements IExternalClassLoader** in addition to its
Windows host capabilities. Register that same Host with AddExternalClassLoader
before VM module initialization. It returns a per-instance script adapter that
knows how to resolve the form/frame class and load its JSON from the copied EXE.
The adapter code remains shared source/static code outside QuadooForm.dll; it
does not require a separately registered loader object or a new DLL.

For Application **Run without debugging**, Studio will:

1. Commit pending property edits and complete the existing Save All transaction.
2. Validate the Default source as a Form unit and collect every project script
   and associated Form/Frame definition, including closed units.
3. Construct private startup source in memory, including absolute script paths
   and a generated `WinMain(strCmdLine, nCmdShow)`.
4. Compile it with `QuadooParseTextToStream()` into an in-memory QBC stream.
5. Copy the matching AQVMT.bin into a staging executable named after the project,
   embed QBC, the unit catalog and every associated native definition, and publish
   the completed `<Project>.exe` only after all resource updates succeed.
6. Launch that EXE in a separate process with the configured arguments and
   working directory. Under the uniform lifetime follow-up, generated WinMain
   calls a generated helper to create/show only the Default Form nonmodally.
   When the helper returns, WinMain calls Host.RunApplication() synchronously
   and returns its exit code. Native AQVMT invokes WinMain and cleans up afterward.

The generated source is never written to disk, added to project JSON, opened in
an editor tab or shown in the project tree. Its compiler file tag is a virtual
diagnostic identity, not a generated file to open. Included user scripts keep
their real file/line diagnostics. The output EXE contains compiled code and
definitions, not the generated source.

Stage **4A is implemented**: adapter plus AQVMT and non-debug Run. The uniform-lifetime follow-up is implemented. Stage **4B** is local application debugging,
using the same catalog/startup builder and adapter. Basic resource packaging
belongs to 4A because Run uses a copied
executable. Stage 5 retains the Compile/export UI, complete deployment and
remote-debug packaging.

## 2. Verified reuse and gaps

Paths below are relative to the repository root.

| Existing code | Finding and planned use |
| --- | --- |
| `platform/QuadooScript/WQVMT/WinMain.cpp` | Loads QBC/ARGS resources, constructs the Windows VM host, runs module initialization and invokes script WinMain. Use its resource-host pattern for AQVMT. |
| `platform/QuadooScript/WQVMT/WQVMT.vcproj` | Already compiles the shared WQVM host and QVMMessagePump sources. AQVMT can reference those same files instead of copying their implementations. |
| `platform/QuadooScript/WQVM/WQVMRun.cpp` | Supports WinMain with zero or two parameters and translates its result to a process exit code. Preserve the two-argument command-line/show contract for generated startup. |
| `platform/QuadooScript/WQVM/QVMMessagePump.cpp` | Exposes Run, End, message handlers, nested controllers and async waits. Reuse it with an application-owned keyboard/message handler. |
| `platform/QuadooScript/Published/WQVMInterfaces.h` | IQVMMessageHandler lets the application adapter preprocess messages without teaching the generic pump about QuadooForm. |
| `desktop/QuadooStudio/QuadooStudio/ProjectCompilerDlg.cpp` | Demonstrates copying a template and embedding QBC/ARGS/icons. Share resource-writing helpers where appropriate; Application Run needs its own prepared-output transaction and explicit AQVMT selection. |
| `platform/QuadooScript/Published/QuadooParser.h` | QuadooParseTextToStream accepts an in-memory root and compiler-status callbacks; included files can be loaded by the existing preprocessor. |
| `platform/QuadooScript/QuadooParser/PreProcessor.cpp` | Includes are ordinary text inclusion, and include paths are read without backslash escape decoding. Existing `#pragma once` supplies duplicate-inclusion protection. |
| `platform/QuadooScript/Published/QuadooVM.h` | Existing external-class loading, property assignment, interface lookup and ordinal invocation provide the adapter mechanisms. No new VM/compiler API is planned. |
| `platform/QuadooScript/QuadooParser/QuadooParser.cpp`, `QuadooCompiler.cpp` and `QuadooVM/VMCore.cpp` | ParseClass initializes the member named by extern(...) using CExternClassInheritance(pClass->m_rstrName). The compiler emits that script class name for LOAD_EXTERN_CLASS, and the VM passes it with the script instance to Host's loader. Thus extern(Form) in MainForm supplies MainForm to the loader and stores the returned adapter in the Form member. |
| `platform/QuadooForm/Published/QuadooForm.h` | Construction/binding, descriptors, typed arguments, component collections, keyboard preprocessing and form/frame operations are public. Use those contracts, including for third-party controls. |
| `platform/QuadooForm/QuadooForm/Construction.cpp` | PostBind runs after the hierarchy is built/bound, before initialization/OnCreate. Populate members and attach the usable root wrapper there. |
| `platform/QuadooForm/NativeRuntime.md` | Ordinary form close does not post WM_QUIT. The application host owns normal shutdown and waits for final destruction of every session Form and release of explicit application lifetime leases. |
| `desktop/QuadooStudio/QuadooStudio/ApplicationProject.cpp` | Form/Frame scaffolds now include pragma once; source synchronization supplies the Create constructor and root accessor. |

The original blocking GetMessage branch did not distinguish `-1` from a received
message. Stage 4A supplies a focused correction/test and verifies nested WM_QUIT
and handler/controller teardown. This is a narrow shared-host change.

## 3. Studio build and launch pipeline

Keep Application dispatch explicit. Do not infer an AQVMT path by appending `T`
to a generic engine path or fall through to console QVM. Initially the target
architecture matches Studio; cross-architecture selection remains later work.
Locate AQVMT and its dependencies from the matching runtime installation/build
layout. Validate availability before replacing any previous successful output.

Save All remains the precondition, consistent with current Run behavior. This
avoids inventing an unsaved-include provider: the existing compiler callback
resolves paths but does not provide arbitrary in-memory include contents. Read
the saved project/source/definitions as one prepared input set; detect relevant
external changes while preparing it and abort rather than knowingly package
mismatched revisions. Do not auto-repair manually edited source at Run time.

The Default source entry is authoritative. It must be exactly one associated
Form, not an ordinary script or Frame. Under the uniform lifetime follow-up,
stop writing/checking redundant application.mainForm metadata; ignore that legacy
cache on load and remove it on an ordinary explicit save. Do not silently choose
the first source. Check duplicate unit IDs/class identities, missing definitions
and unresolved project metadata before launch; keep runtime checks as well for
independently launched EXEs. Default selects startup only, with no runtime role.

Generate one absolute-path `#include` per distinct project script, in stable
project-file order, including the Default source and all Frame sources. Preserve
the preprocessor's path quoting rules; test spaces, Unicode and UNC paths.
Nested relative includes continue resolving against their containing real file,
with project-relative callback fallback where needed, independent of process CWD.
An absolute include locates a file; it does not itself suppress duplicate text.

Add `#pragma once` to newly generated Form/Frame scaffolds and teach Studio's
conservative source scanner to recognize this exact harmless directive. Existing
sources with overlapping include chains need their normal pragma/include guards;
report duplicate declarations through the compiler and explain the remedy.
Do not silently prepend directives to user files or invent a dependency parser.
Top-level project script code will execute during VM module initialization, so
startup/global side effects in all included scripts are part of this contract.

Generate a private __QuadooApplicationStartup(nCmdShow) helper containing
`Host.ShowForm(new <DefaultClass>, nCmdShow);` and no Form return value. Generated
WinMain calls that helper and, once it returns, executes
`return Host.RunApplication();`. ShowForm validates/shows any session Form and
returns Null; RunApplication accepts no Form argument and owns the blocking shared
pump. The helper's arguments/locals are released before pumping. Native AQVMT
continues invoking script WinMain and processing its return value, with no second
pump. Reserve/validate the generated function, lifetime-interface and service-class
names. The helper registers the private lifetime service with
Host.SetApplicationLifetime before constructing the Form. Its interface callback
runs gc() after native binding releases, before committing final exit. Host retains every live
script-instance/adapter association equally, without a permanent preferred Form
pointer. Include startup/shutdown cleanup on every failure path. No direct
external access to the script's Form member or implicit forwarding is required.
It instantiates no other forms or frames automatically. Reserve generated entry
names and diagnose a user-defined WinMain collision instead of renaming user code.
The exact helper method names belong to the adapter contract; there is no hidden
on-disk Main.quadoo implementation.

Recommended Run output is `bin\<architecture>\<Project>.exe`, where Project is
the `.qsproj` filename stem. Stage to a temporary sibling in that same directory,
check Begin/Update/EndUpdateResource results, then replace the final EXE. Never
launch an old output after a failed compile/build. A locked/running EXE produces
a clear failure without terminating it or overwriting another user-selected
file. Retain the previous valid EXE on preparation failures. This output policy
is implemented. DLL replacements and the EXE are prepared together and roll back
on ordinary publication failures; this is not a power-loss recovery journal.

## 4. Resource catalog and runtime files

Embed these in the Run EXE from the outset:

- QBC, using the existing template convention.
- A versioned application catalog identifying the Default unit and, for each
  Form/Frame unit, its unit GUID, kind, script class, events-interface name and
  native definition resource identity. Index catalog entries by unit identity
  and class name. The unit GUID remains distinct from the definition root's
  component GUID.
- Every Form/Frame definition as existing-format JSON, including nonvisual
  components and embedded pictures/icons. Do not embed Studio sourceSync metadata
  or design-time package installation data. Use the actual unit class name as
  the definition resource name, with proposed resource types QFORM and QFRAME;
  for example, QFORM/MainForm and QFRAME/DetailsFrame. Validate class/resource
  collisions before publishing the executable.
- Compatible argument/icon resources where used by the shared template code.
  Do not carry forward the compiler dialog's original-source-path argument;
  compiled Application startup requires no source filename argument.

Define resource names, IDs, language handling, catalog encoding/version, bounds
and duplicate-entry validation in a small shared Studio/host header. Parse through
the existing JSON and stream utilities. Resource lookup must not rely on the
launcher working directory. Each Create receives an independent definition copy.
Tests must prove the launched EXE does not read the original source/definition
files or the designer package directory.

This is not a dependency-free EXE. Resolve/stage matching QuadooForm, QuadooVM,
JSON, SimbeyCore and any other actual host imports from the existing layout.
Reusing the full WQVM host may retain optional compiler/module facilities and
their dependencies: inventory the built imports rather than assume only the VM
DLL is needed. Do not rebuild SimbeyCore. Keep DLL search independent of startDir
using the established executable-adjacent/runtime deployment policy.

## 5. Script adapter and visible contracts

Build the adapter as shared native source/static library used by AQVMT and later
debugging hosts. Keep it outside QuadooForm.dll; that DLL stays usable without
QuadooScript. Use published native/VM interfaces, CBaseUnknown and repository
containers. No private CControl inheritance, STL, exceptions or VM modifications.

The Host owns the loader entry point and catalog/resource access. For
`class MainForm : extern(Form), IMainForm`, LoadExternalClass receives `MainForm`
and the script instance. `Form` names the member variable that receives the
returned definition loader. For a Frame class the same mechanism supplies that
script class name and assigns the returned definition loader to its Frame member.

Host resolves the supplied script class name directly in the embedded catalog,
selects Form versus Frame from the unit kind and returns a per-instance adapter
bound to that class's named JSON resource. The adapter can load its independent
definition snapshot before or during Create without reading a script identity
property or relying on a source filename at runtime. It retains resource access
to the running copied EXE, not an IDE-side definition path.

The existing generated __QuadooUnitId property can remain as unit identity
metadata, but it is not required for runtime class/resource selection. No VM
class-name reflection or extra identity getter call is needed. Scripts access
native form operations and values through the member, for example Form.Close()
and Form.ModalResult. The agreed Form = Form.Create(this) constructor call stays.

The initial contract prototype must prove external-base construction/reentrancy:

1. Expose IExternalClassLoader through AQVMT Host's BEGIN_UNK_MAP and register
   Host before module initialization. Host::LoadExternalClass resolves the
   supplied script class name and its catalog kind/resource, then returns a
   definition loader assigned to the Form/Frame member. It implements Create
   without creating a native subtree or retaining the script instance. Canonical
   IUnknown identity and balanced loader/Host references are covered by tests.
2. At the agreed `Form = Form.Create(this)` call, create the native hierarchy
   from the independent snapshot of the class-bound JSON resource. Get the
   generated events interface with GetInterface using the catalog's interface
   name. Return a distinct live object wrapping IQuadooForm or IQuadooFrame.
   Both live wrappers share a container base; the definition loader is separate.
3. Resolve all nonempty handler names eagerly through ResolveMethod. Delegates
   retain the returned IQuadooInterface and ULONG ordinal and invoke synchronously
   through the existing interface API. Missing methods fail creation cleanly.
4. In PostBind, attach the root to its live wrapper and install it through the
   generated __QuadooRoot property, whose setter updates Form or Frame. Populate
   generated named members through SetProperty before any OnCreate callbacks. Include
   descendants and nonvisual components, using the unit's component identities
   and names. Runtime-created Frames retain independent namespaces/instances.
5. Allow the external Form/Frame methods to work inside OnCreate even though
   the constructor assignment has not returned yet. Respect the native component
   lifecycle for operations that require Ready. On failure, undo all partial
   member population/registrations and destroy the incomplete native subtree.

New Form scaffolds should include a parameterless constructor containing the
agreed Form.Create(this) assignment. Recommended Frame shape is a constructor
accepting a live container, with `Frame = Frame.Create(this, Owner)`; Form.Create
may accept an optional owner for secondary/modal forms. Default Form construction
must require no arguments. For existing units, insert a missing constructor only
when conservative source analysis can do so safely; never replace a user's
constructor or add a second Create call. Explain any manual migration required
before Run. Preserve managed source ownership and breakpoint/dirty transactions.

Expose descriptor-backed native properties without built-in-type switches, with
capability-based methods such as Show/Hide/Close/ShowModal, Destroy, SetFocus,
Button.Click and dialog Execute. Complex value conversion must be documented:
preserve raw Quadoo Colors, use QVMWrapStringArray/IRStringArray snapshot semantics
for item collections, and retain typed structured/collection/object values rather
than stringify them. Support all existing built-in properties through deliberate
conversions and report unsupported extension capabilities precisely. Publish the
script-facing method/value contract before implementation changes public headers.

Event argument objects expose EventName, Sender and descriptor-named typed
arguments, enforcing writable flags for Cancel, Handled, Key/Char and Result.
Retaining an argument must not expose a raw stack pointer after dispatch. Define
which late writes can affect native behavior (synchronous dispatch only).

Implemented return policy: Boolean and null returns do not implicitly cancel or
mark an event handled; native effects come from writable arguments. The generated
`return true;` remains valid, and false is also a successful callback. Other
return types are rejected; VM exception objects are reported as failures.
Construction errors can be caught by the caller without poisoning the runtime.
Unhandled callbacks outside construction request orderly application shutdown.

## 6. Lifetime, message pumping and errors

Use CQVMWinHost/CQVMMessagePump by shared source reference, with an application
host extension or composed Application object providing the forms services.
Register an IQVMMessageHandler for the matching live form's keyboard interface.
Order is host accelerators (if any), IQuadooFormKeyboard::PreprocessMessage, then
TranslateMessage/DispatchMessage only when unhandled. That preprocessing already
includes dialog translation; do not call IsDialogMessage again. Cover owned forms,
runtime Frames, Tab navigation, default/cancel buttons, shortcuts and KeyPreview.

The uniform lifetime policy ends the primary loop only after the last session
Form is fully destroyed, pending startup/construction/callback/cleanup activity
has finished, and no explicit application lifetime lease remains. The Default
instance has exactly the same retention and cleanup policy as every other Form.
Rejected OnCloseQuery and Hide do not remove a Form; Frames do not independently
keep the process alive. Handle startup destruction without entering an idle pump
when no Forms survive. Preserve incoming WM_QUIT codes across nested pumps and
return a defined process exit code, normally zero for normal final destruction.

Bootstrap protection ends when the helper has returned and RunApplication begins.
The blocking host pump call and enclosing WinMain frame must not themselves count
as outstanding callbacks that prevent normal exit. Guard actual user callbacks,
construction and cleanup; reject recursive/invalid primary pump calls. The pump
exit code returns through RunApplication and WinMain before native VM teardown.

Lifecycle tracking cannot depend on a user supplying an OnDestroy binding.
Prototype adapter-owned root lifecycle delegate composition on an independent
runtime definition copy, preserving the user's callback and never saving internal
bindings. Use the existing native event/binder contracts if sufficient. OnDestroy
currently precedes child destruction. The additive final-destruction
observer reports state Destroyed, then a private dispatch HWND schedules cleanup
after active callbacks unwind. Remove records before releases that can re-enter
script; recheck lifetime before committing quit. Details, the public interface
proposal, tray lifetime leases and explicit Win32 ownership are in
[ApplicationLifetimePlan.md](ApplicationLifetimePlan.md). Stop requests must not
unload the VM from inside an active script callback.

Keep named component wrappers stable for one runtime instance. Destroyed objects
retain safe identity but report native destroyed-state errors for invalid
operations. Explicitly break the script/interface/delegate/root/host reference
cycles on accepted destroy, failed creation and process shutdown, including when
script code retains a child wrapper or event argument. Unload extension modules
only after their objects, descriptors, factories and delegates are released.

Startup exceptions report a meaningful error and leave no partial application.
Recommended unhandled callback-error policy is to report and request orderly
application shutdown after unwinding, preserving native close-query semantics
for ordinary user closes. Never propagate a C++ exception across Win32 callbacks.
Test host teardown as well as every Form close order and deferred normal exit.

## 7. Real external controls and bootstrap

AQVMT must never load `%APPDATA%\QuadooForms\DesignTime` or design placeholders.
It starts with built-in runtime factories and accepts real factories through the
published visual/nonvisual registries. A design package does not provide a module
load entry point. As of 2026-10-22, the host also registers its real nonvisual
QuadooPlugin factory, with the placeholder's Module:String contract. PostBind
loads that module through Host.LoadQuadoo, retries `Modules\` plus the value on
failure, and assigns the loaded object to the named form/frame script property
before OnCreate. Both failures roll back construction. The module is retained
through teardown and user replacements of the property are preserved.

Recommend an optional application initialization function before any application
form can be created, with explicit runtime module registration through a small
versioned adapter/host extension contract. Agree its function name, module
registration export/interface and dependency source before implementing external
module deployment. Existing Host.LoadQuadoo facilities are useful module
primitives, not an existing QuadooForm factory-registration protocol.

The native independent factory and runtime QuadooPlugin are covered by the test
host and generated-executable integration tests. Arbitrary real-control factory
registration and full dependency deployment remain separate work. Project > Settings
now lists explicit module files to copy beside the Run executable and an embedded
application icon. The private catalog records deployed file names; AQVMT can load
those copies without changing the configured working directory. Transitive
dependency discovery and export remain follow-ups. See
[ApplicationSettings.md](ApplicationSettings.md). No runtime code reads design-package JSON.

## 8. Source organization and implementation order

| Area | Planned work |
| --- | --- |
| `platform/QuadooScript/AQVMT` | New GUI executable/template project in QuadooStudio.sln, WinMain, resources and four build configurations. Host implements IExternalClassLoader and returns per-instance adapters with embedded class-to-JSON lookup. Reference WQVM shared sources. |
| `platform/QuadooForm/ScriptAdapter` | Native/VM bridge, runtime catalog access, external bases, component/argument wrappers, binders and lifecycle coordination. Shared build code, no new DLL required. |
| `desktop/QuadooStudio/QuadooStudio/ApplicationTarget.*` | Saved project input collection, in-memory startup, explicit AQVMT selection, catalog/resources, output transaction and launch. |
| Studio scaffold/source synchronization | Runtime constructors and harmless pragma-once support; preserve existing ownership, rollback and source navigation. |
| Native/Studio integration tests and a runtime host harness | Real compiler/VM, independent factories, generated EXEs, pump/lifecycle and failure injection. |

Implementation order:

1. Lock the small script-facing contracts and decisions below; publish any
   necessary additive extension interface proposal before changing an ABI.
2. Prove external-base timing, unit lookup, PostBind members, synchronous handler
   reentrancy and cycle-free failure/destruction with the real existing VM.
3. Implement catalog/conversion/capability wrappers and exercise built-ins plus
   the independent third-party test control without internal native dependencies.
4. Add AQVMT and shared pump integration; validate native/scaffold startup and
   accepted/cancelled Form closure independently of Studio's Run command. The
   uniform lifetime follow-up adds uniform ownership and last-Form shutdown
   before local debugging.
5. Add memory-only startup compilation and transactional EXE/resource assembly;
   integrate the Application Run command, parameters and user diagnostics.
6. Enable Ctrl+F5 only after end-to-end acceptance. Native Preview is unchanged;
   F5 debugging/Compile stay disabled until their own implementation is ready.

## 9. Acceptance and validation

Run Debug/Release Win32/x64 with the existing VC9 toolset and repository style;
do not rebuild SimbeyCore. Build individual affected projects and retain native
ABI/JSON compatibility. Actual XP VM execution is a separate validation step.

- Compile every project script from the memory-only root; check relative/nested
  includes, pragma once, duplicate diagnostics, source errors, Unicode/spaces/UNC
  paths and virtual-root error reporting without creating a source file/tab.
- Validate no Default, wrong Default kind, duplicate/inconsistent startup IDs,
  missing/malformed JSON, duplicate identities, missing AQVMT and bad catalog
  versions/resources without launching stale output.
- Create/show the Default form nonmodally. Click a scripted button, tick Timer,
  mutate properties/selection, use existing dialogs, update typed writable event
  arguments, and create two instances of a Frame under live runtime containers.
- Query Host's loader from its other interfaces and verify COM identity/reference
  balance. Compile actual extern(Form)/extern(Frame) classes and assert that the
  loader receives their script class names and the returned adapters reach their
  respective member variables. Exercise two different Form classes, class-named
  JSON resources, missing resources, wrong kinds and repeated instances. Verify
  Form.Close()/Form.ModalResult and class lookup without __QuadooUnitId access.
- Verify all generated members and the distinct live root wrapper are usable at
  the appropriate OnCreate stage. Cover unresolved methods, wrong signatures,
  reentrant callbacks/destroy and failures at each constructor/binding step.
- Use an independent published-interface factory with custom properties/events
  and a container child; no design placeholder registration or private classes.
- Verify close cancellation, Hide, all Form close orders, startup destruction
  before pump entry, keyboard/modal behavior, WM_QUIT/exit codes, GetMessage error,
  timers, retained wrappers/arguments and module unload/cycle/resource counts.
- Run the built EXE after removing access to original script/definition files,
  from another working directory and without installed designer packages. Stage
  the known runtime dependencies; this is not a full redistribution claim.
- Inject resource update/finalization, copy, disk/full/access and launch failures;
  a locked running output must survive. Preserve previous good output and clean
  only task-owned staging files. Preserve save conflict/recovery behavior.
- Run the existing Studio/native/private/designer suites and WQVM/WQVMT pump
  regressions where shared code changes. Keep existing resource thresholds.

## 10. Accepted scope and decisions

Historical stage 4A delivery notes follow. The October 28 lifetime implementation
supersedes their designated-main-form shutdown policy.

| Decision | Implemented choice / follow-up |
| --- | --- |
| Delivery scope | Implement non-debug Run as stage 4A; local debugging follows in 4B. Keep Compile/export and remote staging in stage 5. |
| Run output | `bin\<architecture>\<Project>.exe`; keep it after exit and report a locked prior instance rather than terminate it. |
| Real external modules | AQVMT registers real QuadooPlugin and installs loaded module objects before OnCreate. General factory registration and module staging remain follow-ups. |
| Handler return value | Ignore successful return values for event effects; use writable Cancel/Handled/Result fields. Invocation errors remain failures. |
| Frame construction | New frames use `extern(Frame)` and `Frame = Frame.Create(this, Owner)` with a required live container. Forms use `extern(Form)` and `Form = Form.Create(this)`; an optional owner is accepted for secondary forms. |

Run saves synchronized sources, uses the Studio architecture, and returns zero
after ordinary accepted main-form closure. No public native/VM interface changed.
One narrow parser fix is required for overlapping #pragma once includes: the
preprocessor uses HasItem for membership rather than passing a null output to Find.
There are no QuadooScript VM changes and SimbeyCore is reused without rebuilding.

Production external-module registration/deployment, local debugging, export UI,
actual XP execution and power-loss validation remain separate follow-ups.
