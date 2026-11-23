# AsyncWait implementation

Implemented November 1, 2026 from
[QuadooForm_AsyncWait_Plan.md](QuadooForm_AsyncWait_Plan.md). The timer-free revision
is implemented from
[QuadooForm_AsyncWait_Timerless_Design.md](QuadooForm_AsyncWait_Timerless_Design.md).

## Behavior and placement

`Form.AsyncWait(waitObject, callback)` is implemented at CContainerObject::Invoke,
so Frames and other supported containers expose it through their existing
wrappers. Exactly two non-null Object arguments are required. Native QVPARAMS
stores the callback at index 0 and waitObject at index 1, consistent with the VM's
reverse argument storage. The script order remains waitObject, callback.

The source object's published IQuadooNativeHandle is queried directly on the UI
thread. Its HANDLE-returning GetNativeHandle is called once. NULL and
INVALID_HANDLE_VALUE return ERROR_INVALID_HANDLE as an HRESULT; failed QI,
duplication and registration propagate through Invoke. Malformed
successful QI with a null interface is rejected. A valid handle still must be
eligible for the Windows registered-wait API; the existing interface contains no
object-type information. Mutexes and other unsupported handle types are outside
the provider contract. No undocumented object-type query is used.

The adapter owns a non-inheritable duplicate with SYNCHRONIZE access and retains
the source and callback. This lets the caller close its original handle without
invalidating the registered wait. Every registration has its own duplicate.
Native event/semaphore signaling semantics are preserved.

AsyncWaits.h/.cpp owns the native context, registration and duplicate. The exact
registration is RegisterWaitForSingleObject(...,INFINITE,WT_EXECUTEONLYONCE), with
no additional flags. The worker only reads immutable delivery values, uses
Interlocked state, calls the shared PostAsyncDelivery helper, and records its
result. Failed posts additionally send the private UI failure notification. It never invokes, queries, AddRefs or Releases a supplied object;
there is no worker-side runtime or VM access. Test barriers are compiled only
with QUADOO_ASYNC_TESTS.

CAsyncCalls::Prepare now performs common preparation for Async and AsyncWait.
Both use the same pending-request lookup, unique ticket, target lifecycle
subscription, callback reference and application lease. Async posts immediately;
AsyncWait publishes its record before registering the native wait. Normal Async
retains synchronous posting failure/rollback semantics.

Delivery keeps the original object pointer in LPARAM and the unique ticket in
WPARAM. It continues through the existing container HWND and CContainer::Message,
IQuadooMessageHandler, CFormInstance and CAsyncCalls::HandleMessage. No subclass,
new WNDPROC or callback dispatch window is introduced. The existing lifetime
window is used only for failure maintenance, not completion invocation.

The caller receives Null after successful registration. The callback is invoked
at most once with NULL VM, NULL method and no arguments. Normal results are
cleared; callback failures and recognized script exceptions are diagnosed
nonfatally through the existing service. The existing VM default-function
forwarding limitation and lambda workaround remain unchanged.

## Races and lifecycle

An already-signaled source can start its native callback before registration
returns. Native context and request ownership exist first. A fired flag is
published before posting; the final result is published after posting. Delivery
rejects matching-ticket messages for unfired waits. If a real message arrives
before the worker stores its final status, UI delivery drains the native callback
before testing that status. It does not discard the legitimate message early.

Cancellation removes the request from the deliverable lookup and atomically
marks the native context canceled. UnregisterWaitEx(registration,
INVALID_HANDLE_VALUE) drains that short native callback on the UI thread. It does
not wait for an unsignaled source. The duplicate is closed after confirmed native
quiescence; the registration token is never passed to CloseHandle. A racing stale
post is harmless because its ticket no longer resolves and its pointer is never
dereferenced without an owned matching request.

Source and callback references use QVMReleaseObject on the UI thread under runtime
callback guards. The temporary native-handle interface is released before final
source cleanup. Source release can execute destructors, so target/binding state
is checked again before callback invocation. Callback/lease ownership remains
active throughout user code and result cleanup, including nested modal loops.
Equal source and callback identities retain two separately accounted references.

Pending requests count as outstanding target work. WM_NCDESTROY, component
lifecycle notification, binding detach and runtime shutdown cancel waits and
posted work. Closed endpoints for replaced HWNDs retain the existing Async
retirement/sweep behavior. Hiding/disabling a target does not cancel. Releasing
last-Form waits releases their leases, allowing ordinary last-Form application
shutdown. Callback self-destruction safely cancels its pending siblings.

## Failed delivery and native cleanup errors

A failed worker-side post is an asynchronous diagnostic, not a retroactive error
from the completed registration call. There is no maintenance timer or periodic
wake, including while an infinite wait is armed.

The worker publishes the post error and finished state before calling
SendNotifyMessageW with zero parameters on the existing runtime lifetime window.
The message has its own registered GUID identity. This notification is sent only
on failure; successful completion still follows the existing Async container
message. Cross-thread notification does not wait for the window procedure, so UI
cleanup may drain the worker safely even if the notification arrives before the
worker has returned. All native context remains owned until confirmed drain.

The UI notification handler retains the runtime/dispatcher and runs the existing
reaper under a callback guard. A nested notification sets m_reapPending, ensuring
the outer reap scans again without recursively releasing the same records.
Source destruction can pump a second notification; that path is covered by an
actual reentrancy test. Failed callbacks are not invoked by this handler.

If SendNotifyMessageW also fails, its Win32 error is retained with the posting
error. An ordinary UI sweep, target destruction or shutdown performs cleanup and
reports both codes. No worker-side script call or final script release is used
as a fallback. Immediate cleanup cannot be guaranteed when Windows refuses both
notifications and the UI has no subsequent activity; target cancellation still
releases the request and its application lease.

One immediate retry is allowed for a rejected ERROR_BUSY unregister operation.
Remaining native context/duplicate ownership is retained in the allocation-free
cleanup list. Existing cleanup records get at most one retry per actual reap;
nested notifications do not restart that retry loop. Ordinary UI sweeps and
shutdown provide additional opportunities. No retry timer or self-sustaining
message/work-item chain is created. Script ownership can be released on the UI
thread after cancellation because workers use only independent native handles
and opaque message values.

A real saturated-queue test exposed a second quota path: source/callback lease
cleanup schedules the runtime's ordinary lifetime check. If this PostMessage
also fails with ERROR_NOT_ENOUGH_QUOTA, the runtime now latches m_checkRetry and
retries after an ordinary UI sweep frees queue capacity. Quota exhaustion no
longer sets the application's fatal error. Other genuine dispatch-post failures
retain their existing fatal handling. Tests preserve the full posted queue while
failure cleanup completes, then validate the deferred lifetime check both with a
live registered Form and with the callback lease as the only application lifetime.

If a real unregister failure persists through final shutdown, an explicit
incomplete-cleanup diagnostic is recorded and the canceled native-only record is
retained until process exit. It holds no source/callback/VM reference. The worker
code is compiled into AQVMT/test executables, so no unloaded VM DLL code is used.
This exceptional path deliberately retains native resources rather than allowing
a use-after-free; it is not reported as successful cleanup. Normal completion,
cancellation, rollback and the tested transient-failure path reclaim all records,
registrations, duplicates, targets and leases.

## ABI and dependencies

No published QuadooForm/QuadooScript vtable or export changed. QuadooForm.dll has
no new QuadooScript dependency; it needs no native implementation change for this
feature. The existing message-forwarding contract also works for independent
containers. There are no new built-in controls or designer/JSON changes.

AQVMT and QuadooApplicationTests compile the shared adapter sources directly.
VS2008, Windows XP, repository containers, CBaseUnknown and VM-aware ownership
remain in use. No STL, C++ exceptions or new DLL is introduced. SimbeyCore.dll,
JSON.dll and compiler/VM dependency DLLs were reused without rebuilding.

## Validation commands

```bat
tests\platform\QuadooForm\build-application-lifetime.cmd Debug Win32
tests\platform\QuadooForm\build-application-lifetime.cmd Release Win32
```

```powershell
& .\tests\platform\QuadooForm\test-asyncwait.ps1 -Configuration All
& .\tests\platform\QuadooForm\test-asyncwait.ps1 -Configuration All -StateOnly
& .\tests\platform\QuadooForm\test-async.ps1 -Configuration All -Full
.\tests\results\bin\Debug\QuadooApplicationTests.exe --native-handler-only
.\tests\results\bin\Release\QuadooApplicationTests.exe --native-handler-only
```

`-StateOnly` runs the native state fixture before common-control, DPI and OLE
initialization. It creates no HWND, pumps no messages and performs no GUI action.
The default runner additionally runs actual HWND/modal and real-VM tests. Both
modes launch hidden with redirected logs under tests/results/AsyncWaitLogs. The tests use
native barriers to force posting/registration and delivery/final-status races.

## Results and XP follow-up

Both final Win32 builds completed with zero errors and zero warnings. Results
were identical in Debug and Release:

| Suite | Assertions | Result |
| --- | ---: | --- |
| Non-windowed wait state entry point | 5 | Passed |
| Focused native/real-VM AsyncWait, including state fixture | 191 | Passed |
| Existing Async/native and real VM | 134 | Passed |
| Application lifetime | 192 | Passed |
| Existing application script/runtime | 136 | Passed |
| Independent native-handler forwarding | 56 | Passed |
| Complete native harness | 9,617 | Passed |
| Complete private storage/metadata | 1,544 | Passed |
| Shared designer palette/resources | 1,435 | Passed |
| Studio application integration | 160 | Passed |

The full native/private/palette/Studio suites passed during this implementation.
After final notification-identity and lease-only quota checks, focused AsyncWait,
existing Async, lifetime, script/runtime and native-handler suites were rerun on
both final builds. Logs are in tests/results/AsyncWaitLogs, tests/results/AsyncLogs and the
target/AsyncWait-Timerless-*-handler.log files. Build and combined summaries use
the target/AsyncWait-Timerless-* prefix.

Coverage includes exact callback arguments/thread; independent providers and
containers; equal and distinct source/callback identities; manual/auto-reset,
already-signaled and unsignaled events; original-handle closure; multiple waits;
early registration notification and UI delivery before worker completion;
premature, stale and duplicate messages; every request-allocation boundary;
QI/null/invalid-handle/duplication/registration failures; thread affinity;
source-interface reentrancy; failed-post cleanup without another successful post;
unregister retry; detach/shutdown and HWND replacement; repeated resource release;
native modal owner restoration and nonfatal errors; callback self-destruction,
sibling cancellation and last-Form exit; real VM events, captures, result cleanup,
throws and source/callback garbage collection. The timer-free revision adds
actual posted-queue quota exhaustion with PM_NOREMOVE, sender completion before
UI notification, notification before worker return, both notifications failing,
cleanup during a native modal loop, a source destructor pumping another failure,
no maintenance wake while unsignaled or on success, bounded cleanup retry, and
last-Form/lease-only exit after queue capacity becomes available.

PE inspection confirmed subsystem version 5.0 for AQVMT.bin and
QuadooApplicationTests.exe in Debug and Release. The new wait/handle imports are
KERNEL32.dll RegisterWaitForSingleObject, UnregisterWaitEx and DuplicateHandle,
and USER32.dll SendNotifyMessageW. No Vista thread-pool API was introduced.

An actual XP VM run has not been performed here. Copy the Release test executable,
its normal dependency DLLs/VS2008 runtime deployment and the ApplicationTests
fixture folder to XP. Run these commands, substituting the fixture location:

```bat
QuadooApplicationTests.exe --asyncwait-state-only
QuadooApplicationTests.exe --asyncwait-only "C:\QuadooTests\ApplicationTests\AsyncWaitRuntime.quadoo"
```

The second command includes the native HWND/modal and real-VM cases. In an
Application project, also smoke-test an event-triggered update and closing its
last Form before the event signals. The expected result is one UI completion
when signaled, or clean cancellation and application exit when the Form closes.

Changes remain uncommitted in Subversion.
