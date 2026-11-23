# Application lifetime without a designated main form

Status: implemented and validated, October 28, 2026. Supersedes the designated-main-form
policy in the delivered stage 4A architecture. The implementation and validation
record are [ApplicationLifetimeImplementation.md](ApplicationLifetimeImplementation.md).
Revised at the user's request: generated script WinMain retains control of both
startup and the message loop through a separate generated startup helper.

## Recommendation

Treat Default as the source selected for generated startup. Track every live
top-level Form in an application session with the same ownership, cleanup and
shutdown rules. Do not promote another form to a special role when one closes.
Normal application exit occurs after the last Form is fully destroyed, pending
creation and callbacks have finished, and any explicit application lifetime
leases have been released. Frames and arbitrary HWNDs do not count as Forms.

Generate a helper that creates/shows the Default instance and returns no Form
reference. WinMain calls that helper, then starts the host's blocking pump:

```text
function __QuadooApplicationStartup (nCmdShow)
{
	Host.SetApplicationLifetime(new __QuadooApplicationLifetime);
	Host.ShowForm(new StartupForm, nCmdShow);
}

function WinMain (strCmdLine, nCmdShow)
{
	__QuadooApplicationStartup(nCmdShow);
	return Host.RunApplication();
}
```

Here StartupForm stands for the class selected by Default; it is not a reserved
class name. Reserve/validate the generated helper name as well as WinMain. Both
functions remain private generated source in memory, with no project file or
editor tab. Host.ShowForm validates/shows the supplied session Form and returns
Null, retaining no selected-instance pointer. Host.RunApplication accepts no
Form argument, runs the shared pump synchronously and returns its exit code.

The helper returns before pumping begins. This releases its temporary argument
and any local references, leaving the startup Form with exactly the same session
ownership as every other Form. Script WinMain remains active throughout the
message loop; native AQVMT invokes it, extracts the result and performs teardown
after it returns. Do not add a second pump to the native entry point.

The generated private __QuadooApplicationLifetime class implements
I__QuadooApplicationLifetime.CollectReleasedObjects with `return gc();`. The
host retains that interface/ordinal to drain queued script destructors after
releasing completed instances and before committing normal exit. This service
captures no Form. The VM queues destructors when native ownership releases its
final script reference through QVMReleaseObject; a later gc()/script return runs
them. A destructor can therefore create a replacement Form before exit commits.
Reserve the generated class and interface names along with the helper and WinMain.

The principal changes belong in ScriptAdapter and AQVMT. A small additive native
lifecycle capability is recommended for reliable final-destruction reporting.
No QuadooScript compiler or VM change is needed.

## Findings in the original implementation

The following findings motivated the change. Line references describe the
pre-change sources; the implementation removes the roles described here.

| Source | Finding and implication |
| --- | --- |
| QuadooStudio/ApplicationTarget.cpp:630 | Already uses the inline `Host.RunApplication(new ..., nCmdShow)` expression. Replace the generated shape with a nonblocking ShowForm call in a helper followed by argument-free RunApplication in WinMain. |
| QuadooForm/ScriptAdapter/ApplicationRuntime.h:32 | m_instances already owns records for Forms and Frames. The list is not a count of live Forms. |
| ScriptAdapter/ApplicationRuntime.cpp:242 | SetMain locates one script instance and saves m_main. ShowMain and MainDestroyed depend on this special pointer. |
| ScriptAdapter/ApplicationRuntime.cpp:293 | RootDestroying sets m_stopping and posts quit when that instance is m_main, even if other Forms exist. Further creation is then rejected. |
| ScriptAdapter/ApplicationRuntime.cpp:309 | Sweep excludes m_main from ordinary cleanup, retaining its binding/script state until shutdown. |
| AQVMT/ApplicationHost.cpp:229 | Host.RunApplication validates/shows the designated instance and blocks inside CQVMMessagePump before returning to the caller. |
| QuadooVM/VMCore.cpp:2091 | INVOKE_METHOD passes arguments from the VM stack to Invoke. It clears them only after Invoke returns, at approximately line 2113. The inline expression still retains its argument throughout a blocking host call. |
| QuadooVM/VMCore.cpp:2240 | RETURN/NRETURN clears function locals and arguments before resuming the caller. A startup helper that returns no Form separates the temporary's lifetime from WinMain's blocking pump call. |
| ScriptAdapter/RuntimeEvents.cpp:290 | The root destroy delegate runs the user's OnDestroy and then RootDestroying. This is a destruction-start notification. |
| QuadooForm/QuadooForm/Controls.cpp:407 | OnDestroy runs with state Destroying, before child/component teardown, native destruction and state Destroyed. This is too early for final cleanup or a reliable zero-form decision. |
| ScriptAdapter/RuntimeEvents.cpp:556 | Detach clears assigned script properties, wrappers, interfaces and script references. Those setters/releases can execute user code and mutate runtime collections. |
| QuadooScript/WQVM/QVMMessagePump.cpp | The pump is already independent of any Form. It preserves a quit code through nested pumps. Reuse it inside the argument-free Host.RunApplication call from script WinMain. |
| QuadooForm/QuadooForm/Windows.cpp:428 | ShowModal has its own nested loop and reposts an incoming WM_QUIT. Lifetime notifications must work in this loop too. |
| ApplicationTarget.cpp / ScriptAdapter/ApplicationResources.cpp | The embedded catalog calls the startup class mainClass. Studio also retains redundant application.mainForm metadata despite files[].default being authoritative. |

Removing the script variable is useful, but does not by itself change shutdown
policy or remove the long-lived VM argument. Simply replacing m_main with a
counter would leave argument retention, early OnDestroy, reentrancy and pending
construction problems unresolved.

## Session ownership and the definition of an existing Form

Separate the application's native Form registry from its script binding records.
The session owns native Form records through public interfaces and the adapter
owns its CFormInstance records for Forms and Frames. A native lifetime coordinator
can be composed into CApplicationRuntime; it need not know script class names,
the Default unit or a preferred Form pointer.

Each live Form record contains canonical IUnknown identity, a retained
IQuadooForm, a destruction-notification subscription and its lifecycle state.
Use repository TArray/TMap/TStackRef abstractions and UI-thread affinity. Do not
identify Forms by class names, HWND enumeration, creation index, visibility,
window ownership or internal CForm pointers.

The script association established by the loader keeps each live script object,
its interface delegates and named members alive. Every successful Form has this
same ownership rule, including the startup instance. Dropping an ordinary script
variable does not close a live native Form. Closing/destroying a Form releases
the session's binding ownership at a safe point; an independently retained
script object can outlive its destroyed native Form under the existing wrapper
error rules.

Count Forms that are hidden, minimized, owned or currently modal. Hide is not
destruction. OnCloseQuery cancellation does not change the count. Retained COM
or script references to an already destroyed Form do not keep the application
alive. Frames, controls, menus and Form-owned nonvisual components do not add
independent application lifetime.

Track in-progress creation separately. A Form is published to the live registry
only after successful construction/activation. A creation scope prevents exit
while a nested constructor, OnCreate or rollback is running. Failed construction
does not leave a live count or stop unrelated existing Forms. Nested successful
creation must be accounted for independently of an outer construction failure.

All Forms belonging to this application session must use its creation/registration
path. A native extension creating an additional Form can join through an explicit
host RegisterForm service using public interfaces and the same lifecycle contract.
Do not count unrelated host/Preview windows or pretend to discover Forms created
outside the session by scanning the thread's HWNDs. Frame ownership remains
unchanged and explicit.

## Script-owned startup and message loop

Recommended flow:

1. AQVMT creates the session before running VM module initialization. Bootstrap
   protection suppresses automatic exit during initialization and generated
   startup, until RunApplication deliberately transitions to normal execution.
2. Generated WinMain calls __QuadooApplicationStartup(nCmdShow). Inside it, the
   private lifetime callback is registered first, then the inline constructor
   uses Form.Create and registers the instance under ordinary rules; it does not
   mark a persistent role.
3. Host.ShowForm validates its argument as a Form created by this session, shows
   it using nCmdShow if still alive, and returns Null. Retain a temporary reference
   only across that call. This is a generic operation usable with other session
   Forms, not an assignment of a startup role.
4. The VM releases ShowForm's argument when that native call returns. The helper
   then returns without a Form value, clearing its frame and any locals. Generated
   WinMain holds no Form local, return value or closure capture. The session's
   uniform binding ownership keeps every surviving Form alive.
5. WinMain calls Host.RunApplication() with zero arguments. This validates the
   execution phase, ends bootstrap protection, reaps completed instances and
   evaluates lifetime. If live Forms or leases remain, it runs CQVMMessagePump
   synchronously. With no survivors, return zero or briefly drain pending work
   before exiting; never wait idly for a nonexistent Form. Draining pending posts
   also exposes an existing low-priority WM_QUIT so its original code is preserved.
6. On final destruction the coordinator requests quit; RunApplication returns
   the pump's exit code, and WinMain returns that code to native AQVMT. Existing
   native cleanup runs after the script stack has unwound, before VM/module unload.

This revises the private script-host contract: add nonblocking ShowForm(script,
nCmdShow), and change RunApplication from two arguments to zero while keeping it
blocking. Update generated source and direct host tests together; do not keep a
two-argument compatibility path that pins a preferred Form during pumping. The
helper must not return a Form or delegate that captures it. Never clear borrowed
QVPARAMS to shorten an argument's lifetime.

Bootstrap protection must end at RunApplication entry, not at WinMain return.
Likewise, the long-lived WinMain frame and primary RunApplication invocation are
not active event-callback guards: otherwise last-Form exit would be permanently
blocked by the pump itself. Guard actual user callbacks, construction and cleanup
while preserving safe VM re-entry from the pump, as in the current host. Reject
RunApplication during module initialization, construction or an event callback,
and reject nested, repeated, cross-thread or post-shutdown primary pump calls.
Generated startup/helper failures propagate normally, never enter the pump, and
release bootstrap protection during native error teardown. Native AQVMT must not
start a fallback pump after script WinMain returns.

If the startup Form closes in its constructor or during Show, surviving Forms
still keep the application alive. Keep destroyed binding records through the
bootstrap helper so a completed-but-destroyed startup instance can still be
recognized and skipped for Show. This temporary bootstrap protection applies to
all records, not just the selected startup class. Reap them equally on entry to
RunApplication, after the helper has returned. A genuine constructor failure
remains a startup error.

Default means the instance requested by generated startup, not m_instances[0].
Arbitrary global initializers or constructors can explicitly create additional
Forms before another native Form has completed. Preserve normal script execution
order; do not infer shutdown priority from chronological order or silently alter
VM global-initializer semantics to enforce a stronger ordering promise.

## Reliable destruction completion

Keep user OnDestroy's existing timing and preserve the internal delegate
composition. Do not move the script event after native teardown: its handlers
currently have access to named controls. Treat RootDestroying only as a request
to reconsider lifetime/cleanup, not proof that destruction has completed.

Two additive generic public capabilities were published before implementation:

- IQuadooComponentLifecycle (IID ending 004A) exposes AddDestroyedObserver and RemoveDestroyedObserver, both returning HRESULT.
- IQuadooComponentDestroyedObserver (IID ending 004B) exposes void OnComponentDestroyed(component).

The observer fires exactly once after the component has state Destroyed and
its children, nonvisual components, native windows and bindings have completed
teardown. The callback receives a borrowed component protected for the duration
of the call. Notification remains UI-thread-affine; a destruction error is
reported separately and cannot undo a completed destruction. Observer registration
holds a reference and removal releases it, consistently with property observers.

The native base can provide this generic service without a QuadooScript dependency.
Use BEGIN_UNK_MAP and test identity/refcounts. The adapter subscribes at PostBind,
before OnCreate can destroy the root. Registration failure aborts creation and
rolls back the subscription, script assignments and registry reservation.

The application observer uses a detachable/weak session link and only marks the
record complete and queues work. It must not clear members, release the VM or
decide quit inline in the native destruction stack. Subscribe for Frames as well
for uniform binding cleanup; only IQuadooForm records contribute to application
lifetime. Independent root factories can implement these interfaces without
deriving from a QuadooForm implementation class.

This is the only native ABI addition: new interfaces/IIDs, no change
to existing vtables, ContextOptions, exports or form-definition JSON. The service
is optional for ordinary components and native-only consumers. AQVMT-managed
definition roots require reliable final-destruction reporting; reject an
unsupported root or stale QuadooForm.dll clearly before OnCreate rather than
silently falling back to polling. Third-party child controls are unaffected.

A posted WM_NULL plus the existing early OnDestroy is insufficient: a nested
modal loop can consume it before native teardown finishes, leaving no wake after
the final state transition. The completion notification avoids that lost-wake
case without timer polling or changes to the shared pump's ABI.

## Deferred cleanup and normal exit

Give the session a private dispatch HWND for coalesced lifetime-check messages.
It is not a Form, has no special application-owner status, and contributes no
keep-alive count. It also works when the thread is inside the native ShowModal
loop, where a thread-only message is not dispatched to an HWND procedure.

Track bootstrap, construction, script/native bridge callback activity, cleanup
reentrancy and a monotonically changing lifetime generation. The primary blocking
RunApplication call and enclosing WinMain frame are excluded from callback guards;
bootstrap ends before this call starts pumping. On completed root
destruction, form creation/commit/rollback, lease changes and outer activity-scope
exit, queue a lifetime check. Use a pending flag to coalesce requests. Do not post
irreversible WM_QUIT merely because an intermediate count reaches zero.

At the deferred check:

1. If construction/bootstrap/application callbacks are active, defer; the outer
   scope's exit must guarantee another check even if a nested loop consumed the
   first message. Guard against recursive cleanup.
2. Remove completed records from mutable registries before detaching/releasing
   them. Retain local references while clearing assigned members and delegates.
   Never retain an array index across a call that can execute script, destructors
   or custom setters. Reap the former startup instance exactly like every other.
   Release the final script reference with QVMReleaseObject, then invoke the
   generated cleanup interface to run queued destructors. Retained delegates use
   a detachable instance link and cache only the ordinal; they cannot retain a
   closed Form's methods interface after detachment.
3. Re-evaluate after releases, because destructors/setters can create or destroy
   another Form. Use the generation/activity guards to recognize this change;
   defer another bounded cleanup pass rather than spinning indefinitely.
4. If startup has finished, no construction/callback/cleanup is outstanding,
   no live Form remains and no keep-alive lease remains, commit normal exit:
   enter Stopping and call PostQuitMessage(0) exactly once on the UI thread.
   Drain ordinary pending messages before posting: Win32 can defer synthesis of
   an external WM_QUIT while those messages remain. Preserve an existing quit
   code rather than replacing it with the normal zero code.
5. After quit is committed, reject new creation/startup/lease acquisition.
   Pending checks before that commitment are revocable if another Form appears.

This handles closing A while B remains; closing B before A; creation of C from
A's OnDestroy; and destruction of A while B is modal. No Form is chosen as A's
replacement or promoted to a main role. A queued timer intending to create a
Form after all Forms have closed is not by itself a keep-alive guarantee; future
background work must explicitly hold application lifetime before the last close.

RunApplication re-entry, cross-thread use and stale queued messages need explicit
guards. Shutdown destroys the private dispatch window only after pending work
and subscriptions are disabled. Do not capture raw instance pointers in posted
messages or try to retract an already-posted WM_QUIT when new work arrives.

## Future notification-area operation

Reserve explicit application keep-alive leases in the lifetime coordinator. With
no leases, normal policy is exactly last-Form destruction. A future application-
owned tray component could hold a lease while no Forms exist, create/show a Form
from a tray command, and release the lease when the tray icon is removed. No
hidden default Form or permanent startup reference is necessary.

A lease is reference-counted, tied to this session, and released exactly once.
UI-thread code acquires/releases it; any later worker-thread support must marshal
to the UI thread. Avoid a lease/session reference cycle; a detached lifetime link
can make retained leases safe after shutdown. Diagnose outstanding leases at
teardown. Ordinary Timers, modules, wrappers and Form-owned components acquire
no implicit lease.

A tray icon intended to outlive every Form must belong to an application-level
component/service owner. A tray component owned by a destroyed Form would be
destroyed with that Form, correctly releasing its lease. Actual tray UI, public
script lease methods and an application component collection remain future work;
the initial implementation can reserve the internal seam without adding them.

## Explicit ownership, cancellation and exceptional termination

Do not make the startup Form the implicit Win32 owner of other Forms. Ordinary
`Form.Create(this)` already creates an independent Form. Explicit owner arguments
are separate: Windows can destroy owned windows when their owner is destroyed.
Keep existing explicit owner/modal semantics in this pass and test them. To keep
an independent modeless Form alive after its creator closes, do not pass the
creator as native owner. Automatic owner detachment/reassignment would be a
separate behavior change, not an incidental consequence of counting Forms.

OnCloseQuery rejection, Hide and nonzero reference counts do not mean a Form has
been destroyed. Preserve the existing Button/ImageButton ModalResult completion
behavior and modal owner restoration. Only a successfully completed native
destruction removes a live Form.

The last-Form rule governs normal automatic shutdown. Preserve a distinct fatal
error/forced-stop path: record the first error, stop admitting new activity,
unwind callbacks, destroy remaining roots and release services at a safe point,
then terminate with the error code. Forced teardown cannot depend on a user's
OnCloseQuery accepting it. Do not unload the VM inside a callback. Caught
construction errors must not poison the entire session.

An externally supplied WM_QUIT is already a thread termination request. Preserve
its exact code and nested-loop propagation, then force cleanup of remaining
session Forms; never discard it to enforce a normal-lifetime rule. A future
user-facing Exit/CloseAll command can honor close-query vetoes separately.
The coordinator owns new normal quit requests; nested pumps still mechanically
repost an existing quit request as required.

## Naming and metadata changes for implementation

| Current reference | Proposed treatment |
| --- | --- |
| CApplicationRuntime::m_main | Remove. Keep only uniform registries, startup-completed state and session exit state. |
| SetMain / ShowMain | Replace with transient ShowForm(script, nCmdShow), usable for any session Form. Retain no selected-instance pointer afterward. |
| Host.RunApplication(script, nCmdShow) | Generate a helper that calls ShowForm(new DefaultClass, nCmdShow), returns, then call blocking RunApplication() from WinMain. Native AQVMT does not acquire a second pump. |
| MainDestroyed | Replace with session-level ShouldRunMessageLoop / CanExitNormally, evaluated after cleanup. |
| RootDestroying | Keep the early hook only if useful; name it RootDestructionStarted. Add completed-root notification/queued cleanup separately. |
| Sweep's `instance != m_main` exception | Remove; every destroyed root follows the same cleanup. |
| m_stopping used for main closure and fatal errors | Split lifecycle phases and explicit stop reasons. Last-Form closure must not latch Stopping before deferred evaluation. |
| Builder mainClass, mainID, configuredID, local generated `main` text | Use startupClass, startupUnitID and startupSourceText; remove redundant configured-role checks. |
| Embedded catalog mainClass / validator mainUnit | Rename to startupClass / startupUnit; used only to validate startup/resource metadata. |
| Project application.mainForm | Stop writing/depending on this redundant field. files[].default plus designer metadata remains authoritative. Ignore the legacy cache on load and remove it on an explicit normal project save. |
| Test role names/logs such as `Main form created`, runtime fixture MainForm | Use Startup form, StartupForm/IStartupForm and OtherForm for fixtures; update JSON/RC resources and expectations together. |
| Documentation claiming primary-loop exit on main-form close | Replace future guarantees with the session policy and retain old validation reports as historical results. |

Keep WinMain, the generated startup program, the executable entry point and
MainMenu terminology: they do not identify a privileged Form. Do not rename a
user's chosen class/file/property just because it is literally named MainForm.
Default remains the existing Studio UI concept and is not a runtime status flag.

Bump the embedded catalog/template version to 2 when changing mainClass to
startupClass. Update CatalogVersion, AQVMT.rc's resource 105 marker (currently
hard-coded to 1), builder validation and all test catalogs in the same change.
Reject mismatched templates using the existing revision-mismatch path. Existing
EXEs embed their old host and retain their old behavior until rebuilt.

The project metadata can remain schema version 1 because removing a redundant
derived key does not change files[].default or the settings schema. Preserve all
other application fields. Save must not rewrite a project merely because it was
opened; the first ordinary save removes the legacy cache. No compatibility shim
should reinstate a runtime main-form role.

## Implementation sequence and acceptance

1. Prove the generated helper boundary with the real VM: release the ShowForm
   argument and helper frame before entering RunApplication; exercise script
   destruction and a surviving second Form. Prototype final-state notifications
   and nested destruction before changing shutdown.
2. Implement the generic lifecycle capability, uniform native/session records,
   activity scopes, deferred cleanup/check HWND and normal-exit decision. Remove
   the special record and preserve recoverable creation/error behavior.
3. Generate the startup helper and script WinMain sequence, split ShowForm from
   argument-free blocking RunApplication, and transition bootstrap at pump entry.
   Keep native AQVMT's script invocation/result/cleanup path. Migrate role names,
   catalog/project metadata, host/runtime fixtures and integration assertions.
   Keep source synchronization unchanged.
4. Build affected individual projects with VC9 in Win32 Debug/Release, followed
   by x64 configurations when available; run the real parser/VM and generated-EXE
   tests, native lifecycle/modal/keyboard suites, and focused Studio project/run
   coverage. Do not rebuild SimbeyCore, JSON or compiler/VM dependency DLLs. Test
   the final Win32 executable on XP separately.

Required tests include:

| Scenario | Expected outcome |
| --- | --- |
| A creates independent B; A closes first | B receives events/input and keeps the process alive; A's bindings are reaped. |
| B closes before A; three Forms close in each order | Only final completed destruction requests normal quit, once. |
| Startup Form closes during constructor/Show and B survives | No startup-pointer failure or hang; B continues. With no survivors/leases, startup returns cleanly. |
| A creates B/C from OnClose/OnDestroy or a script destructor | Creation scopes/generation changes prevent premature quit and stale array access. |
| Last close rejected, hidden or minimized | No form removal or normal quit. |
| Frames/child controls are destroyed while their Form survives | Binding cleanup occurs; no independent application exit effect. |
| Last Form closes inside nested modal/event loops | Final-state observation, callback unwind, owner restoration and exactly-once quit remain correct. |
| Root lifecycle observer setup or construction fails | Full rollback, correct counts, no premature quit; independent Forms stay usable after caught failures. |
| Startup object passed as `new ...` only to ShowForm in the helper | Argument and helper frame are released before RunApplication; no generated Form reference remains while pumping. Native/binding cleanup equals other Forms. |
| Script WinMain owns startup and the blocking pump | Exactly one pump; its exit code returns through RunApplication and WinMain. Bootstrap and pump frames cannot suppress last-Form exit. |
| Helper throws, or RunApplication is called with arguments/re-entered | No pump after startup failure; incorrect phase, signature and recursive use fail clearly without corrupting the session. |
| Retained destroyed script/control/collection/event wrappers | Safe destroyed-state errors, released runtime-owned references, no accidental keep-alive. |
| Mock lease with zero Forms, then new Form, then final lease release | Pump survives zero Forms only while deliberately held; no tray implementation required. |
| Explicit Win32 owned modeless Forms and owner destruction | Counts reflect actual destruction; no implicit ownership is added. |
| Independent native Form implementation/registration | Same capability-based lifetime policy, COM identity and subscription cleanup without private class inheritance. |
| External WM_QUIT 123, callback error, host teardown | Original exit code/error retained, remaining Forms cleaned safely, no VM unload on an active script stack. |
| Default changed, stale legacy metadata, version-mismatched AQVMT | Correct startup class from Default, coherent saved project, clear stale-template error. |

Also assert no m_main/SetMain/ShowMain/MainDestroyed production roles remain;
verify extension/factory/observer lifetimes and GDI/USER counts after repeated
creation, all close orders, cancellation and failed startup. Avoid using a pass
in one configuration as proof of XP behavior.

## Implementation notes

The implemented decisions are: keep startup and pumping under script WinMain;
use a generated helper for inline construction and nonblocking ShowForm; return
from that helper before calling argument-free blocking RunApplication; add a
generic final-destruction capability; preserve explicit Win32 ownership; remove
redundant project mainForm metadata; version the embedded startup catalog; and reserve explicit lifetime
leases for future application-owned services. These decisions support the stated
behavior without an application main Form, a VM change or a hidden surrogate Form.

The additive lifecycle signatures/IIDs and revised host contract were published
before implementation. The generated cleanup interface is the additional private
host detail required by the existing VM's destructor scheduling. See the
implementation report for validated configurations, commands and remaining XP
execution checks. No compiler/VM dependency DLL changes are required.
