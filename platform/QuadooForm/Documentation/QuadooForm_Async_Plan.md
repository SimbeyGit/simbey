# Container Async() design plan

Status: implemented, November 1, 2026. See
[AsyncImplementation.md](AsyncImplementation.md) for the implementation,
validation record and existing VM default-forwarding limitation/workaround.
Revised to receive callbacks through CContainer::Message(), as requested by the
user. No additional window subclassing will be installed.

The implemented one-shot native-handle wait extension is described in
[QuadooForm_AsyncWait_Plan.md](QuadooForm_AsyncWait_Plan.md).

## Script contract

Add `Async(callback)` to `AQVM::CContainerObject::Invoke()`. Forms, Frames,
Panels, GroupBoxes, TabPages, ScrollingPanels and externally implemented
containers receive the method through their existing container wrappers.

```text
Form.Async(callback);
Panel1.Async(callback);
```

The method accepts exactly one non-null `QuadooVM::Object` value. Accept every
kind of IQuadooObject, including native objects, lambdas, bound methods, global
function references and objects with a default callable member. Do not require
a concrete VM class or a special callback interface. Whether the object supports
a zero-argument default invocation is determined when the callback runs.

On success, Async returns Null after posting the request. The eventual invocation
is exactly `callback->Invoke(NULL, NULL, &emptyParameters, &result)`, with a
zero-initialized QVPARAMS and a result initialized to QuadooVM::Null. There are no
callback arguments; captured state belongs to the supplied object. Discard and
clear ordinary return values, including strings and objects.

This is deferred execution on the container's UI thread. It does not create a
worker thread or make the script VM or component wrappers thread-safe. Posting
requires the session's UI thread and a live application binding. Calls through
unbound/detached wrappers fail rather than retaining a disconnected VM.

The callback never runs inline from Async. Normal queue dispatch determines when
it runs. A modal loop or explicit message pumping can execute it while the outer
script handler is still active. Callbacks can post more callbacks, open modal
dialogs, create Forms, or destroy their own container. Nested loops can reenter
the dispatcher; do not promise that callbacks finish in posting order.

## Existing architecture and placement

| Existing source | Consequence for this feature |
| --- | --- |
| ScriptAdapter/RuntimeObjects.cpp | CFormObject delegates unrecognized methods to CContainerObject; CFrameObject inherits it. NewComponentObject also selects CContainerObject for other IQuadooControlContainer implementations. |
| Published/QuadooForm.h | IQuadooControlContainer already inherits IQuadooWinControl. GetHandle provides the target HWND without using private native classes. |
| QuadooForm/Windows.cpp | CContainer::Message() already receives container HWND messages through the existing CWinControl window procedure. Extend this path rather than adding a subclass. |
| ScriptAdapter/RuntimeEvents.cpp | CFormInstance owns a detachable binding link. Async requests must be cancelled before Detach releases script members, interfaces and the script object. |
| ScriptAdapter/ApplicationLifetime.cpp | EnterCallback/LeaveCallback and AcquireLifetimeLease already prevent premature cleanup and application exit. Reuse these services. |
| QuadooScript/QuadooVM/DllMain.cpp | QVMReleaseObject and QVMClearVariant perform the VM's final-object/destructor protocol. A plain final COM Release is insufficient for queued script ownership. |
| QuadooScript/QuadooVM/VMMethodDelegate.cpp, VMGlobalFunctionRef.cpp, VMLambda.cpp | Existing callable objects accept a NULL method. A NULL caller VM selects immediate execution when the posted message is delivered, without changing the compiler or VM. |
| QuadooScript/WQVM/QVMMessagePump.cpp | Its existing Async method has a different controller/handler contract. Leave it alone. The new method needs ordinary window-message dispatch, including nested modal pumps. |

Keep script-object ownership and invocation in ScriptAdapter, compiled into AQVMT
and the existing application tests. Add only generic message forwarding to the
native runtime. QuadooForm.dll must not include QuadooVM.h or own script objects.
No new DLL, designer command, component, JSON property, or change to an existing
interface vtable is required. The additive native capability proposed below
advertises support for message forwarding; it does not contain script methods.

## Receiving the message

Add a private `AsyncCalls.h/.cpp` with a session dispatcher and reference-counted
per-container/HWND endpoints using CBaseUnknown and repository containers.

Use the container's current IQuadooWinControl::GetHandle HWND, including the
outer handle of scrolling containers. Do not substitute the runtime's lifetime
window or the child-host viewport. Do not call SetWindowSubclass or install a
new WNDPROC.

The delivery path is:

```text
existing CWinControl::WindowProc
    -> CContainer::Message
        -> construction context's GetBinder()
            -> QueryInterface(IQuadooMessageHandler)
                -> CFormInstance::HandleMessage
                    -> adapter dispatcher validates the pending request
                        -> callback->Invoke(NULL, NULL, &emptyParameters, &result)
```

CFormInstance will implement the existing published IQuadooMessageHandler
interface. CContainer::Message forwards only the dedicated callback message and
the primary HWND's WM_NCDESTROY notification to that optional handler. The native
forwarder treats LPARAM as opaque; only the adapter validates/dereferences the
IQuadooObject pointer. Unhandled messages continue through the existing native
message path. Forwarding WM_NCDESTROY must not mark native destruction handled
or prevent the original window procedure and NativeDestroyed from running.

Deferred callback errors are diagnosed inside the adapter and consumed there.
Do not feed their HRESULT into CControl::ReportCallback, which would change the
confirmed nonfatal policy by setting native modal callback errors.

Proposed additive capability: an empty IQuadooContainerMessageTarget : IUnknown
marker in the published header, implemented by CContainer and exposed through
BEGIN_UNK_MAP. It advertises this message-forwarding contract without changing
any existing interface's IID or vtable. Allocate its IID during implementation
after checking the current published header. Async checks for it before taking
queue ownership or posting, so an unsupported external container fails
synchronously instead of silently losing a callback and retaining its lease.

Independent third-party containers can implement that capability and forward
the same two messages from their existing window procedure to their construction
binder's IQuadooMessageHandler. Share the message identifier and forwarding
contract in a small published header; no inheritance from internal native
classes is required. There is no subclass fallback for unsupported containers.

Use one qualified, GUID-named RegisterWindowMessageW identifier for the adapter
protocol. A registered identifier avoids assuming that independently authored
container classes leave a particular WM_USER/WM_APP offset unused. Keep this a
same-process protocol; registering an identifier does not marshal object
pointers between processes.
[RegisterWindowMessageW documentation](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerwindowmessagew)

Post each accepted request as:

```text
PostMessageW(containerHwnd, asyncMessage, requestTicket,
             reinterpret_cast<LPARAM>(callbackObject));
```

LPARAM contains the actual IQuadooObject pointer, as requested. WPARAM contains
an internal nonzero ticket. Maintain a pending-record lookup; the message is a
wake-up/identification mechanism, not the owner of the pointer. Each record owns
one callback reference, the target/binding association, and a temporary session
lifetime lease. Repeated posts of the same object create distinct records and
references. Use pointer-sized message fields on Win32 and x64.

Allocate tickets without reuse for the adapter's lifetime in the process, even
after individual requests/sessions finish. Detect exhaustion instead of wrapping
into an old ticket. On delivery, match the ticket, endpoint and pointer value before dereferencing any
LPARAM. Unknown, cancelled, mismatched or duplicate deliveries must not invoke
or release a pointer supplied by the message. Multiple logical containers sharing
one HWND within a session must remain distinguishable by their tickets and
binding records. The session dispatcher does not infer ownership from the HWND
alone. External containers advertising support must route their HWND messages
to the appropriate session handler. Unknown messages resume normal processing.

PostMessage returns before delivery and can fail. Retain the object before the
post and roll back the pending record/reference/lease if posting fails. Return
the Win32 failure as an HRESULT through the existing Invoke error path.
[PostMessageW documentation](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-postmessagew)

## Queue and dispatch flow

1. Validate parameters/result pointers, argument count, variant type, callback
   pointer, UI thread, runtime acceptance state, component state and current HWND.
   Check the forwarding capability and HWND ownership on the same process/UI
   thread; no window-procedure replacement or subclass installation occurs.
2. Create or locate the endpoint through published component identity and HWND.
   Recheck liveness after calls into third-party interfaces or observer setup.
3. Allocate the record, take its callback reference, acquire the lifetime lease,
   publish the record, and post the message. Failure leaves other requests intact.
   Remove a newly created idle endpoint if its first enqueue fails.
4. When the message arrives, validate it against the pending lookup and remove
   the matching record before entering user code. Hold local strong references
   to the endpoint/runtime and record while handling reentrancy.
5. Check that the binding and component remain live and that the endpoint still
   represents the container's current HWND. Otherwise cancel the request.
6. Enter the existing runtime callback scope, invoke the original object once
   with NULL VM, NULL method and zero arguments, then process/clear its result.
7. Release the owned callback with QVMReleaseObject (or an owned object QVARIANT
   cleared with QVMClearVariant), release the lease, and leave the callback scope.
   Keep cleanup inside the guard: releasing objects can schedule script
   destructors, create replacement Forms, or reenter application code.

Never pass a borrowed VM argument/stack-slot address through the window message.
Do not use TStackRef's ordinary final Release for the owned script callback.

## Cancellation and lifetime rules

| Situation | Required behavior |
| --- | --- |
| Callback completes | Invoke once, clear its result, release its reference and lease once. |
| Unsupported forwarding capability, PostMessage or allocation failure | Return failure synchronously; leave no owned request or callback reference. |
| Container hides or becomes disabled | Keep the request; the container is still alive. |
| Container enters Destroying/Destroyed before delivery | Cancel without invoking. Observe final lifecycle notifications when available and always validate state on delivery. |
| Target HWND receives WM_NCDESTROY | CContainer::Message forwards the notification to the adapter, which closes that endpoint and cancels its pending records. Continue native destruction normally. |
| HWND is replaced while the component survives | Cancel old-HWND requests. A subsequent Async call reacquires GetHandle and installs a fresh endpoint. Do not transfer old requests silently. |
| Callback destroys its own container | Let that running invocation finish under local ownership; cancel its still-pending siblings. |
| Form/Frame binding detaches or construction rolls back | Cancel associated requests before releasing assigned properties, delegates and script ownership. |
| Runtime stops or shuts down | Reject new requests, cancel all remaining work, disconnect handler/dispatcher links, then continue existing native/script teardown before VM/module unload. The window procedure remains unchanged. |

Mark endpoints closed and remove records from shared collections before releasing
objects. A release/destructor must not find a half-removed request. Cancellation
is idempotent. Where an external component lacks the optional final-lifecycle
observer capability, HWND destruction, delivery-time state checks, binding
detach and session shutdown still provide safe cancellation.

Do not depend on Windows delivering or removing messages for a destroyed HWND.
Queued stale messages may contain an obsolete pointer value; only owned records
authorize dereferencing it. Do not blindly purge a container's message range with
PeekMessage: a window filter can include child-window messages belonging to
other endpoints.

The temporary lease accounts for queued work that might create another Form.
Cancellation releases it, so abandoned calls cannot keep the application alive.
Reuse the existing last-Form/lease shutdown policy; Async introduces no preferred
Form or separate pump. Preserve external WM_QUIT codes and modal owner restoration.

## Errors and result handling

Immediate validation and enqueue errors return from Async normally through the
wrapper's HRESULT handling. A successful enqueue does not promise that an
arbitrary object implements a callable default member or accepts zero arguments.
Those failures occur at delivery. E_PENDING is an unsupported continuation result
for this one-shot dispatch; do not retry or retain temporary parameter buffers.

Selected policy (confirmed by the user): report deferred failures through the existing
ReportDiagnostic service with an `Async` path and target component name, then
continue the application. Ordinary return values are ignored; false is not a
cancellation signal. No new synchronous error dialog is opened by the dispatcher.
Other eligible queued callbacks remain scheduled. Preserve the diagnostic
service's existing first-diagnostic behavior; this feature does not add a new log
or diagnostic UI.

Handle the VM's exception-result convention explicitly. Existing event delegates
can treat an object result as an exception because their normal result types are
restricted; Async must not classify every legitimate returned object as failure.
The VM currently exposes CVMException's existing identity through QueryInterface.
Plan a small adapter-only helper that probes that identity as opaque IUnknown,
without including VM implementation headers or casting to CVMException. Validate
this against the existing VM DLL and release the probe normally. Only a recognized
exception result is inspected for Code/ToString; clear every result afterward.
Document this private identity dependency. If the probe cannot be supported by
the current binary, report a proposed public exception-identification capability
before making any VM/API changes; do not substitute property-name heuristics.

Deferred callback failures must not call CApplicationRuntime::OnError, set the
application's fatal error/stopping state, record a native modal callback error,
close a dialog, or post WM_QUIT. Release the failed request's result, callback
reference and lease normally, and continue dispatch. A nested modal loop remains
active under its existing rules. No fatal-error/modal-unwinding capability is
needed for this feature.

## Implementation sequence

1. **Message bridge and ownership.** Add AsyncCalls, ticket/endpoint bookkeeping,
   enqueue rollback, VM-aware object release and test seams. Add the published
   forwarding marker/contract, CContainer::Message forwarding, and the binder's
   IQuadooMessageHandler implementation. Do not add any window subclass.
2. **Wrapper and dispatch.** Add the Async branch to CContainerObject::Invoke;
   validate all input pointers before touching result or parameter storage.
   Implement default invocation, result cleanup and the chosen diagnostic policy.
3. **Session integration.** Connect lifetime leases, callback guards, binding
   rollback/Detach, optional component lifecycle observation and Shutdown cleanup.
   Keep runtime ownership acyclic and disconnected endpoints harmless.
4. **Validation and documentation.** Extend application/adapter tests, add callable
   examples using verified script syntax, and update ScriptAdapter/README.md.

Expected edits are RuntimeObjects.h/.cpp, new AsyncCalls.h/.cpp,
ApplicationRuntime.h/.cpp, ApplicationLifetime.cpp and RuntimeEvents.cpp, plus
AQVMT/ApplicationTests project file entries and application test fixtures. The
native edits are the published forwarding capability/contract, CContainer's
interface map in Runtime.h, and message forwarding in Windows.cpp. The WQVM
message pump needs no change under the selected diagnostic policy.

## Validation plan

Use non-windowed tests for ownership accounting and failure injection, plus real
HWND/message-loop application tests for delivery and teardown:

- Native IQuadooObject, lambda, bound method, global function reference and
  default-callable object; verify NULL method, NULL caller VM and zero arguments.
- No inline execution; exactly-once delivery; multiple requests using the same
  object; normal posting order; nested loops and callbacks that queue more work.
- Ordinary object/string/scalar return values are released correctly; real script
  throws and native HRESULT failures produce the selected diagnostic behavior.
- Wrong arity, Null/non-object/null object pointer, wrong thread, detached binding,
  stopped runtime, invalid HWND, unsupported forwarding capability, injected
  PostMessage/allocation failures and third-party forwarding errors.
- Destroy before delivery, self-destruction, pending sibling cancellation, hidden
  and disabled targets, HWND replacement, shared HWNDs, stale/duplicate tickets,
  pointer mismatch, native object address reuse and ticket exhaustion.
- Form, Frame, nested built-in containers and a third-party container implemented
  solely with the published native interfaces and forwarding contract; no
  IControlInternal assumptions. Verify a container lacking that contract rejects
  Async synchronously, and verify no subclass/new WNDPROC is installed.
- Drop the script's callback variable and run gc() before delivery; verify retained
  captures survive and destructors run after completion/cancellation.
- Queue during construction, then fail/roll back construction. Test detach and
  shutdown while callbacks/results have references to script objects or Forms.
- Callback creates another Form; closes the last Form; closes the target; enters
  ShowModal; receives external WM_QUIT. Verify leases, safe cleanup, owner state,
  quit-code preservation and no premature exit or stuck application lifetime.
- Repeated enqueue/deliver/cancel cycles return callback, endpoint, factory and
  USER/GDI resource counts to baseline.

Build affected Win32 Debug/Release projects individually, using VS2008 and
reusing SimbeyCore.dll, JSON.dll and the compiler/VM dependency DLLs. Run focused
Async tests, existing application lifetime/script integration tests, and the
native regressions appropriate to any eventual native changes. Validate XP API
compatibility and schedule an actual XP VM run separately. No builds or tests
are required to deliver this planning document.
