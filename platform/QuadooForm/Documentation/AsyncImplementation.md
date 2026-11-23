# Container Async implementation

`CContainerObject::Invoke()` now implements `Async(callback)` for application
container wrappers. The method accepts one non-null IQuadooObject of any concrete
type, validates the target and UI thread, retains the callback and posts it to
the container's actual GetHandle HWND. LPARAM is the IQuadooObject pointer;
WPARAM is an internal request ticket. Success returns Null. The receiver calls
`callback->Invoke(NULL, NULL, &emptyParameters, &result)` once and clears its result.

The receiving path is the existing CWinControl::WindowProc into
**CContainer::Message**, then the construction binder's optional
IQuadooMessageHandler. CFormInstance implements that handler and delegates to
AsyncCalls. There are no added subclasses, WNDPROC replacements, worker threads,
new dispatcher windows, designer commands or JSON properties. The existing
application-lifetime window continues its existing purpose; callbacks are posted
to their target container windows.

## Native contract and ABI

The additive IQuadooContainerMessageTarget marker uses IID
`D25A0D6A-34B4-48EA-AB90-3C8AB72A004D`. All built-in CContainer descendants expose
it through BEGIN_UNK_MAP. Existing published interface IIDs and vtables are
unchanged. QuadooForm.dll remains independent of QuadooScript.

The new published QuadooContainerMessages.h provides the cached, GUID-named
registered message identifier and ForwardContainerMessage helper. A third-party
container can advertise the marker and forward that message plus its primary
HWND's WM_NCDESTROY to its construction binder from its existing window procedure.
The helper leaves WM_NCDESTROY unhandled so normal native teardown continues.
Only the adapter interprets the payload. No inheritance from CControl, CContainer
or any private runtime class is required. A container without the marker returns
E_NOINTERFACE before accepting callback ownership; no subclass fallback exists.

## Ownership and lifecycle

AsyncCalls owns pending records independently of the Windows queue. Each request
holds one callback reference, a target/binding association, and an existing
application lifetime lease. Tickets are allocated without reuse by an interlocked
32-bit process/module counter; exhaustion returns ERROR_ARITHMETIC_OVERFLOW.
Message fields and lookups are pointer-sized. The receiver validates ticket,
HWND and pointer value before dereferencing LPARAM. Unknown, mismatched, stale
and duplicate messages cannot invoke or release an arbitrary pointer.

Records are removed from the pending lookup before user code runs. Callback
guards and local ownership protect nested pumps and self-destruction. Callback
references use QVMReleaseObject; result variants use QVMClearVariant, preserving
script destructor/capture behavior. Posting, allocation and observer-registration
failures roll back ownership without affecting other requests.

WM_NCDESTROY cancels target requests. Optional final-lifecycle observers also
cancel them and retire target records. Closed HWND records block reentrant posts
to a window still being destroyed and are reaped after native teardown. External
containers without lifecycle observers still work through WM_NCDESTROY,
delivery-time state checks, binding detach and session shutdown. HWND replacement
cancels old requests; subsequent calls reacquire the current handle. Normal
delivery retires idle targets and removes their observers.

Detach closes the binding before cancellation or script-property cleanup.
Shutdown disconnects the dispatcher before native/root/VM teardown. Requests and
targets do not retain their runtime owner, avoiding an ownership cycle. Pending
leases keep the message loop alive long enough to dispatch work even with no live
Form, and cancellation releases those leases. The existing normal-exit and
external WM_QUIT policies are preserved.

## Error policy and existing VM limitation

Validation/enqueue errors return synchronously from Async. Deferred HRESULT
failures and recognized VM exception results go through ReportDiagnostic with
an Async/component-name path. They do not call OnError, set modal callback errors,
close dialogs or stop the application. Remaining eligible requests continue.
The existing first-diagnostic retention policy is unchanged.

Ordinary returned objects must not be mistaken for exceptions. An adapter helper
probes the current VM's existing exception identity
`5660ED64-49BB-48B3-908D-5D5B255012CA` as opaque IUnknown. It reads Code/ToString only
after that probe succeeds. This is a documented private identity dependency,
validated with the current VM DLL; no VM implementation headers or class-layout
casts are used and no compiler/VM binaries were modified or rebuilt.

The current VM's native NULL-method invocation of a script class with
`default function` forwarding returns DISP_E_BADVARTYPE (0x80020008), also when
called directly without Async. The class's ordinary script invocation works.
Async deliberately uses the requested IQuadooObject::Invoke API and reports this
existing failure nonfatally. Post a bound method or wrap the call in a lambda:

```text
var callable = new CustomCallable;
Form.Async(new function()
{
	callable();
});
```

The lambda workaround is covered by the real-VM suite. Global function references,
bound methods, lambdas and native objects all pass direct queued invocation.

## Build and validation

```bat
tests\platform\QuadooForm\build-application-lifetime.cmd Debug Win32
tests\platform\QuadooForm\build-application-lifetime.cmd Release Win32
```

```powershell
& .\tests\platform\QuadooForm\test-async.ps1 -Configuration All -Full
```

The build helper builds affected individual projects, including AQVMT, the native
runtime, designer/Studio and their tests. It reuses SimbeyCore.dll, JSON.dll and
the compiler/VM dependency DLLs. Logs are in tests/results/AsyncLogs.

Focused tests use an independent native container implemented with published
interfaces and the real native Form/Frame/container classes through AQVMT. They
cover ticket validation, repeated callbacks, COM identity, unchanged WNDPROC,
wrong arguments/thread, unsupported targets, allocation/post/ticket failures,
partial observer-registration failure and destruction during registration,
optional lifecycle support, HWND replacement/shared HWNDs, reentrant dispatch,
self-destruction and sibling cancellation, retained captures/destructors, ordinary
object results, nonfatal native/script failures, modal owners and external quit
codes, construction rollback, detach/shutdown, pending-work leases without Forms,
last-Form replacement, and the default-forwarding workaround. Full native/private,
application lifetime, script, palette and Studio application regressions are run
alongside them.

Both Win32 builds completed without errors or warnings. Final Debug and Release
results were identical:

| Suite | Assertions | Result |
| --- | ---: | --- |
| Focused Async/native and real VM | 134 | Passed |
| Existing application lifetime | 192 | Passed |
| Existing application script/runtime | 136 | Passed |
| Complete native harness | 9,617 | Passed |
| Complete private storage/metadata | 1,544 | Passed |
| Shared designer palette/resources | 1,435 | Passed |
| Studio application build/run | 160 | Passed |

Validation uses the installed Windows host and VS2008 Win32 Debug/Release builds.
An actual XP VM run remains a manual follow-up. The new Windows APIs are available
on XP; QuadooForm.dll and AQVMT.bin retain PE subsystem version 5.0. No additional
subclassing API or dependency DLL was introduced. Changes
are left uncommitted in Subversion.
