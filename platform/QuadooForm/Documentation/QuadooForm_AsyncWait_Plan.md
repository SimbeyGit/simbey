# Container AsyncWait() design plan

Status: implemented, November 1, 2026. See
[AsyncWaitImplementation.md](AsyncWaitImplementation.md) for implementation,
validation and remaining XP VM checks. The sections below retain the design
contract and implementation sequence.

The implemented revision removing the maintenance timer is documented in
[QuadooForm_AsyncWait_Timerless_Design.md](QuadooForm_AsyncWait_Timerless_Design.md).

Build upon the implemented [Async plan](QuadooForm_Async_Plan.md) and
[implementation record](AsyncImplementation.md). Preserve its CContainer::Message
routing, ticket validation, UI-thread invocation, cancellation, VM-aware object
release and nonfatal diagnostic policy.

## 1. Script contract

Add the method beside Async in AQVM::CContainerObject::Invoke():

```text
Form.AsyncWait(waitObject, callback);
```

Both arguments must be non-null IQuadooObject values. Query the first object
itself for the existing published IQuadooNativeHandle interface and obtain its
native handle. The second object is the completion callback. Return Null when
the wait has been registered successfully. Each successful call represents one
independent registration and at most one completion invocation.

Use precisely these registration parameters:

```cpp
RegisterWaitForSingleObject(
	&registration,
	ownedHandle,
	WaitCallback,
	context,
	INFINITE,
	WT_EXECUTEONLYONCE);
```

There is no script timeout, periodic callback, wait result argument, registration
object, or explicit cancellation method in this first version. Target lifetime
supplies automatic cancellation. A later API can add cancellation tokens without
changing this call's two-argument contract.

When signaled, post the original completion object through the same native
posting helper and message protocol used by Async. Invocation remains exactly:

```cpp
callback->Invoke(NULL,NULL,&emptyParameters,&result);
```

Use zero arguments and a Null-initialized result. Ignore ordinary returned values
and report callback failures without stopping the application, as Async already
does. Registration does not invoke script inline. Nested message loops retain
the existing Async reentrancy behavior.

Implementing this at CContainerObject also gives Frames and other supported
containers the same behavior as Forms, using their own current HWNDs. Independent
containers use the existing IQuadooContainerMessageTarget forwarding capability.

Example, where completionEvent is an existing native event object:

```text
Form.AsyncWait(completionEvent, new function()
{
	Label1.Caption = "Completed";
});
```

Keep the existing VM default-function-forwarding limitation documented in
AsyncImplementation.md. Accept any completion IQuadooObject; determine whether
its default invocation works when dispatched. The existing lambda workaround
also applies here.

## 2. Existing interfaces and implementation constraints

| Existing code | Consequence |
| --- | --- |
| QuadooScript/Published/QuadooVM.h | IQuadooNativeHandle already exists. GetNativeHandle returns HANDLE directly, not HRESULT. |
| ScriptAdapter/RuntimeObjects.cpp | CContainerObject owns the Async entry point and validates the script binding. |
| ScriptAdapter/AsyncCalls.h/.cpp | CAsyncCalls owns UI-thread request/target collections, tickets, leases, posting and delivery. |
| ScriptAdapter/ApplicationLifetime.cpp | The runtime already owns a message-only dispatch HWND, callback scopes and lifetime leases. |
| ScriptAdapter/RuntimeEvents.cpp | Binding detach cancels Async work before releasing script members. |
| Published/QuadooContainerMessages.h and QuadooForm/Windows.cpp | The existing registered message reaches the binder through CContainer::Message. |

Do not call QueueAsync or CAsyncCalls::Enqueue from the thread-pool callback.
Both currently perform UI-thread validation, interface calls, collection edits
and script-reference operations. They are not worker-thread APIs.

Keep QuadooForm.dll independent of QuadooScript. No new published native/VM
interface, export, component, JSON property, DLL, window, subclass, or WNDPROC
replacement is needed. Native control rendering and the WQVM message pump remain
outside this change. Preserve VS2008, Windows XP, no-STL/no-exceptions and the
repository's formatting and ownership conventions.

The first object's QueryInterface is authoritative. Forwarding IQuadooObject
methods through CComponentObject::m_srNativeHandler does not automatically expose
IQuadooNativeHandle on that wrapper. A component wrapper that lacks this public
capability is rejected normally; do not silently unwrap it or infer a handle
from a Handle property. A provider can return a native IQuadooObject exposing the
interface directly.

## 3. Validate and own the native handle

Perform all provider calls on the application's UI thread:

1. Validate parameter/result pointers, exactly two Object arguments and both
   object pointers. Validate the runtime, binding and live container, including
   the existing message-forwarding capability and HWND ownership.
2. Query waitObject for IQuadooNativeHandle, propagating its HRESULT. Treat a
   successful query with a null interface as an invalid provider response.
3. Call GetNativeHandle once. Reject NULL and INVALID_HANDLE_VALUE with
   HRESULT_FROM_WIN32(ERROR_INVALID_HANDLE). A provider's private error convention
   cannot be inferred because this interface returns no HRESULT.
4. Duplicate the handle into this process with SYNCHRONIZE access, inheritance
   disabled and no DUPLICATE_CLOSE_SOURCE. Return the Win32 error if duplication
   fails; do not fall back to a borrowed handle.
5. Retain owned references to the original waitObject and callback. Release the
   temporary IQuadooNativeHandle interface on the UI thread before the final
   VM-aware release of its owning object. Do not retain an unnecessary secondary
   interface reference through final script-object cleanup.
6. Revalidate the target after provider/observer calls, which can reenter code or
   destroy the container. Publish no half-initialized request.

The duplicate refers to the same synchronization object while keeping its handle
lifetime independent from closure of the provider's original handle. This is the
chosen ownership policy. [DuplicateHandle documentation](https://learn.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-duplicatehandle)

The provider must supply a supported waitable handle with synchronization access.
Events are the primary use case; process, thread, semaphore and waitable-timer
handles can follow the native wait contract. A valid Win32 handle is not
necessarily eligible: mutexes are explicitly unsupported. An auto-reset event
can be consumed by a wait, and a semaphore wait can consume a count. Do not probe
eligibility using WaitForSingleObject(handle,0), which could consume the signal.
[RegisterWaitForSingleObject documentation](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-registerwaitforsingleobject)

The existing interface cannot reliably classify every arbitrary handle using
documented XP APIs. Duplication is not proof of wait eligibility. Unsupported
handle types remain a provider contract violation; do not promise graceful
rejection of every such type, introduce undocumented object-type queries, or add
an exception-catching mechanism. A future requirement for complete type
validation would need an explicit additional capability/contract. No interface
change is proposed for the specified event-wait behavior.

Give every registration its own duplicate, including repeated registrations on
the same source object. Preserve native signaling semantics; one auto-reset
signal is not a broadcast to every registration. Do not reset, pulse, close or
otherwise modify the caller's original handle.

## 4. Share Async preparation, posting and delivery

Refactor only the parts of Async needed to support delayed posting:

- **Prepare on the UI thread:** validate/retain the target and callback, reserve
  the existing unique ticket, acquire a lifetime lease, and insert the request
  into the normal pending-request lookup.
- **Post an already prepared request:** a narrow helper uses only immutable HWND,
  message number, ticket and opaque callback-pointer values, plus Win32 posting
  and atomics. It performs no interface calls, allocation or map operations.
- **Dispatch on the UI thread:** use the existing lookup, liveness checks,
  callback scope, default invocation, diagnostics and VM-aware cleanup.

Ordinary Async performs prepare followed by post immediately and retains its
existing synchronous rollback behavior. AsyncWait prepares an unposted request
and lets the registered native callback perform the shared posting step later.

Add an optional private wait record to an AsyncRequest, or an equivalent private
association. The UI-owned record contains the registration handle, owned duplicate,
owned waitObject, and a native-only callback context. The normal request retains
the callback, target and application lease. Waiting requests must count as
outstanding work in RetireIdle, cancellation and endpoint bookkeeping.

The callback context contains only immutable delivery values and Interlocked-
managed cancellation/completion/status fields. It has no owning script references,
container interfaces, VM pointers, dispatcher-map pointers or worker-side
reference-count operations. Its storage remains valid until the registered wait
has been definitively drained. CBaseUnknown-based ownership stays on the UI
thread; a POD context may simply be owned by that record.

Use the unchanged protocol:

```text
WPARAM = existing unique request ticket
LPARAM = original callback IQuadooObject pointer, opaque outside the adapter
```

The delivery route remains:

```text
Win32 wait callback
  -> shared Async PostMessage helper
  -> target container HWND
  -> existing CContainer::Message()
  -> IQuadooMessageHandler / CFormInstance
  -> CApplicationRuntime::HandleContainerMessage()
  -> CAsyncCalls::HandleMessage()
  -> original callback Invoke(NULL,NULL,...), on the UI thread
```

Do not introduce a second script invocation path or invoke callbacks from the
runtime's lifetime-dispatch window. No additional window subclassing is used.

## 5. Registration and worker-thread races

Initialize the native context and publish the prepared request before calling
RegisterWaitForSingleObject. Hold its ownership through registration, including
rollback. An already-signaled event can start its worker callback before
RegisterWaitForSingleObject returns. Store the registration handle and finalize
registration state before making any further reentrant interface calls.

The worker callback performs only these actions:

1. Atomically claim the one-shot notification and observe cancellation.
2. Record that the native wait has fired before attempting the post.
3. If not canceled, post the existing Async message using the prepared values.
4. Capture any PostMessage error immediately and publish the final native status
   with Interlocked operations. Return without invoking or releasing anything.

It never calls script code, AddRef/Release on supplied objects, QVMReleaseObject,
GetState/GetHandle, runtime diagnostics, SendMessage, or dispatcher collections.
Test-only allocation/post failure hooks must also be thread-safe or captured as
immutable per-request settings before registration.

Extend message validation to distinguish an armed-but-unsignaled wait from an
eligible posted request. A matching ticket/pointer alone must not trigger an
unfired wait. Ignore such a message without canceling the real registration.

A successful post can reach the UI before the worker records its final status.
For a fired wait, drain/unregister its native callback on the UI thread before
relying on that status or releasing its context. Do not discard a legitimate
message merely because the final Posted state has not yet been stored. After
draining, accept delivery only when posting actually succeeded and the target
is still eligible.

Remove eligible requests from the lookup before any script invocation or object
release. Duplicate/stale messages then remain harmless. Recheck binding/HWND
liveness after cleanup that can reenter script code.

## 6. Completion, cancellation and shutdown

Use UnregisterWaitEx(registration,INVALID_HANDLE_VALUE) on the UI thread to drain
the short native callback. Call it after normal firing as well as during
cancellation: WT_EXECUTEONLYONCE does not eliminate unregistering. Never unregister
synchronously from that registration's worker callback. Do not hold a lock that
the worker needs, and do not pump messages while draining.
[UnregisterWaitEx documentation](https://learn.microsoft.com/en-us/windows/win32/sync/unregisterwaitex)

This waits for the native posting callback to return, not for the source event
to become signaled. The worker must never wait for the UI, invoke external code,
or perform blocking unregister calls itself. That invariant makes this simple
join suitable for target destruction and shutdown.

Keep the two handle kinds separate: the registration token is unregistered; the
owned duplicated synchronization handle is closed only after native callback
quiescence. Never use CloseHandle on the registration token. The caller's source
handle remains untouched.

For successful delivery, drain and remove wait-specific native resources before
entering the user's completion callback. Release the owned waitObject on the UI
thread with QVMReleaseObject, inside the existing runtime callback guard. This
can run destructors, so perform the usual target/binding revalidation afterward.
The callback reference and lifetime lease remain held through invocation/result
cleanup, as in Async. If both arguments identify the same object, account for
both owned references and final destruction exactly once.

Cancellation order is: mark canceled/closed; remove the request from deliverable
collections while retaining local ownership; drain the native wait; close the
duplicate; release source and callback with VM-aware cleanup on the UI thread;
release the lease. A racing final post is allowed to leave an obsolete message;
its ticket no longer resolves and its pointer is never dereferenced.

| Situation | Behavior |
| --- | --- |
| Registration/duplication/allocation fails | Return the HRESULT synchronously and roll back this request completely. |
| Source already signaled | Queue once through Async; no direct script invocation from registration. |
| Source never signals | Remain armed while the target remains valid. |
| Target hides or becomes disabled | Keep waiting. |
| Target destroys its HWND, binding detaches, or construction rolls back | Cancel waiting and already-posted requests without completion invocation. |
| Target HWND changes | Cancel work addressed to the previous HWND; do not transfer it. |
| Callback destroys target or closes last Form | Finish the running callback under its guard and cancel pending siblings. |
| Runtime stops/shuts down | Reject new registrations; cancel/drain waits before severing runtime links, destroying m_dispatch, or unloading VM/module code. |
| External WM_QUIT | Preserve the existing quit code and shutdown rules; do not wait for unsignaled sources. |

Keep an application lease while the request is armed or posted, then release it
on every terminal path. Target destruction cancels these leases, so an infinite
wait attached to the last closed Form does not keep the application alive.

Check unregister failures explicitly. Without confirmed native quiescence, do
not free callback-context storage or close the duplicated handle. Keep native
cleanup state retained, diagnose the failure and retry through UI maintenance;
never treat a failed drain as permission to free memory. Persistent native cleanup
failure must be reported as an incomplete-cleanup condition, not hidden behind
successful resource counts or an unsafe code unload.

## 7. Failed-post diagnostics and independent cleanup

A worker-side PostMessage failure occurs after AsyncWait has returned, so it
cannot become that earlier call's synchronous HRESULT. The worker records a
native error only. UI maintenance reports it through ReportDiagnostic with an
AsyncWait(target-name) path, cancels the request and releases its resources.
Do not call OnError, terminate a modal loop, set the fatal stopping state, show a
new error dialog, or post WM_QUIT for an ordinary deferred failure.

Do not rely on a second successful PostMessage to clean up a failed first post.
A saturated or unavailable target queue could otherwise strand the callback,
source object and lease indefinitely.

The implemented timer-free revision snapshots the existing lifetime-dispatch
HWND and a unique registered failure-message ID on the UI thread. The worker
publishes its final post-error state, then sends a pointer-free SendNotifyMessageW
wake only if posting failed and the request is not canceled. The existing UI
window invokes the guarded reaper; successful callbacks continue exclusively
through the Async container-message path.

No maintenance timer, periodic wake, new HWND or subclass is used. Nested failure
notifications are latched for another scan by the current reaper. A rejected
ERROR_BUSY cleanup gets one immediate retry; additional native cleanup depends
on ordinary UI sweeps or shutdown and cannot create a self-sustaining retry loop.

If the maintenance notification also fails, preserve both Win32 error codes and
retain work for the next ordinary UI sweep or teardown. Immediate cleanup cannot
be guaranteed if the OS refuses both mechanisms and the UI has no further
activity. Destruction still cancels the target and releases its application lease.

The lifetime check generated by cleanup must also tolerate a full posted queue:
ERROR_NOT_ENOUGH_QUOTA latches a deferred retry at the next UI sweep instead of
becoming fatal. The implementation tests both Form and lease-only application
lifetime under actual queue exhaustion.


## 8. Implementation stages and expected edits

1. **Shared Async request preparation and posting.** Extract the UI preparation
   and native-only posting operations inside AsyncCalls.h/.cpp. Add explicit
   delivery eligibility without changing ordinary Async behavior. Preserve all
   current tests before enabling waits.
2. **Native wait ownership and lifecycle.** Add private AsyncWaits.h/.cpp with
   registration/context ownership, cancellation, native draining and completion
   status. Connect it to CAsyncCalls target/request bookkeeping and the existing
   runtime dispatch-window failure notification. Keep request maps UI-only.
3. **Script entry point and shutdown.** Add AsyncWait to RuntimeObjects.cpp and
   QueueAsyncWait to ApplicationRuntime.h plus its implementation. Integrate
   CancelBinding, CloseTarget, registration rollback and Shutdown ordering. Share
   the existing nonfatal diagnostic/default-invocation code.
4. **Validation and documentation.** Add focused native/real-VM tests and a runner,
   project entries, README usage and an implementation/validation record.

Expected production changes are confined to ScriptAdapter/AsyncCalls.h/.cpp,
new ScriptAdapter/AsyncWaits.h/.cpp, RuntimeObjects.cpp, ApplicationRuntime.h and
ApplicationLifetime.cpp (plus the relevant runtime implementation file). Update
AQVMT and QuadooApplicationTests project source lists. Add tests under
QuadooStudio/ApplicationTests and a focused platform/QuadooForm test script.

Native QuadooForm.dll, designer data formats, factory registration and existing
published headers do not require modification. This plan proposes no public ABI
addition and no compiler/VM changes.

## 9. Validation and acceptance criteria

Use deterministic native barriers and failure injection instead of sleep-based
race assertions. Keep pure worker-state/ownership tests non-windowed and identify
actual-HWND/message-loop tests separately.

- Wrong argument count/types, null object pointers, missing native-handle
  interface, malformed successful QI, NULL/INVALID_HANDLE_VALUE, duplication
  failure, registration failure, failure-notification rejection, allocation and ticket
  failures. No retained request, observer, source, callback, lease or owned handle
  remains after successful rollback.
- An independent IQuadooObject/IQuadooNativeHandle provider, a real VM event
  object, and native/lambda/bound-method/global-function callbacks. Verify exact
  NULL VM/method and zero arguments, callback execution on the UI thread, and no
  supplied-object interface/refcount calls on the wait worker.
- Manual-reset and auto-reset events; never-signaled and already-signaled events;
  repeated Set calls; multiple registrations on one source; repeated callback
  objects; source/callback identity equality. Verify native signaling semantics
  and exactly one eligible dispatch per registration.
- A deterministic worker run before RegisterWaitForSingleObject returns; UI
  delivery before worker final-status publication; cancellation before firing,
  during posting and after posting. No use-after-free, deadlock or lost dispatch.
- Drop script variables and run gc() while waiting. Verify retained captures and
  source survive. Close the original event handle after registration and signal
  its separately held duplicate to verify the adapter's handle remains valid.
- Matching-ticket premature messages while unsignaled, stale/duplicate messages,
  pointer mismatch, HWND reuse/replacement and independent third-party containers.
  Confirm the existing WNDPROC/subclass state is unchanged.
- Inject worker-side post failure and verify diagnostics/resource cleanup via
  the nonqueued maintenance notification without a successful fallback post. Verify no completion
  invocation, fatal error, stuck lease or modal-dialog termination.
- Callback failures/throws and ordinary returned object/string values; reentrant
  Async/AsyncWait; source destructors that reenter or destroy the target; callbacks
  that destroy themselves, open modal forms or create a replacement Form.
- Unsignaled source remains untriggered during Form/Frame/container destruction,
  binding detach, failed construction, last-Form exit, external WM_QUIT and runtime
  shutdown. Cleanup must not await the source signal. Verify quit-code preservation.
- Transient unregister failure and eventual confirmed drain; no context or handle
  is freed while native callbacks can still access it. Treat a persistent failure
  as a failed cleanup test with an explicit diagnostic.
- Repeated arm/fire/cancel cycles return kernel HANDLE counts, target/request
  counts, source/callback/factory ownership and leases to baseline.

Build affected Win32 Debug and Release projects individually with VS2008. Reuse
SimbeyCore.dll, JSON.dll and compiler/VM dependency DLLs without rebuilding them.
Run the new focused wait tests, all existing Async tests, application-lifetime
and script-adapter regressions, including the native-handler forwarding suite.
Use existing native/designer/Studio regressions if implementation changes their
shared code. Validate XP imports/subsystem compatibility and run the focused
wait/shutdown scenarios on the XP VM separately.

Planned commands, once the new runner/fixtures exist:

```bat
tests\platform\QuadooForm\build-application-lifetime.cmd Debug Win32
tests\platform\QuadooForm\build-application-lifetime.cmd Release Win32
```

```powershell
& .\tests\platform\QuadooForm\test-asyncwait.ps1 -Configuration All
& .\tests\platform\QuadooForm\test-async.ps1 -Configuration All -Full
.\tests\results\bin\Debug\QuadooApplicationTests.exe --native-handler-only
.\tests\results\bin\Release\QuadooApplicationTests.exe --native-handler-only
```

No builds or tests are required for delivery of this planning document. Remaining
constraints are the provider's supported-handle obligation and the already
recorded VM default-forwarding limitation; neither requires a public interface
change for the requested event-based feature.
