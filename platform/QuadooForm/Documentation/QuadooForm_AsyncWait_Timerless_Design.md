# AsyncWait without a maintenance timer

Status: implemented, November 1, 2026. See
[AsyncWaitImplementation.md](AsyncWaitImplementation.md) for the implementation,
Debug/Release results and remaining XP VM checks. The design below records the
replacement of the original maintenance timer.

## Recommendation

Remove AsyncWait's maintenance timer entirely. Keep
RegisterWaitForSingleObject(...,INFINITE,WT_EXECUTEONLYONCE) and successful
completion through the shared Async PostMessage/CContainer::Message route.

Use a one-time, cross-thread SendNotifyMessageW notification to the existing
runtime lifetime-dispatch HWND only when the native wait callback cannot post
completion. Its UI handler reaps the already-published failure. Cancellation
continues to drain native waits synchronously. Cleanup retries are bounded or
triggered by actual UI activity/shutdown, rather than periodic polling.

An armed, unsignaled AsyncWait therefore causes no AsyncWait timer ticks,
maintenance messages, or scheduled retry work.

## Why the existing timer was introduced

RegisterWaitForSingleObject already provides the native wait. The timer does not
wait on or test the source event. It was added solely to recover from a worker's
failed PostMessage and retry native cleanup failures. The original implementation
started it when the first AsyncWait was prepared and kept it active while native
wait records existed. That unnecessarily makes the exceptional cleanup mechanism
part of every normal wait.

The replacement needs a UI wake mechanism independent of another PostMessage
into an exhausted posted-message queue. It does not need a periodic clock.

## Delivery design

| Situation | Action |
| --- | --- |
| Source remains unsignaled | Let the registered native wait sleep. No maintenance wake is scheduled. |
| Source signals and completion PostMessage succeeds | Deliver the original callback through the existing Async message and CContainer::Message; unregister/drain and release normally. |
| Completion PostMessage fails | Publish the error and finished state, then SendNotifyMessageW a private, pointer-free maintenance wake to the existing lifetime HWND. |
| UI receives maintenance wake | Reap failed requests under the existing UI callback guard; report a nonfatal diagnostic and release source/callback references and leases on the UI thread. Never execute the failed completion callback. |
| Target closes, detaches or shutdown starts | Remove deliverable records, mark canceled, and drain/unregister the native wait as today. |

The worker captures the immutable maintenance HWND/message alongside the existing
successful-delivery values during UI-thread preparation. It makes no runtime or
container interface calls and owns no script reference. The implemented wake has a registered GUID identity and carries zero
WPARAM/LPARAM; no worker-owned context
pointer or borrowed script variant is transported through it.

Publish final failure state before sending the wake. The UI may process the wake
before the worker returns, so the existing native-drain rule remains mandatory
before freeing context. Keep the lifetime HWND alive until all registered wait
callbacks have been drained during shutdown. A late redundant wake just scans
the current runtime's owned records and is harmless.

SendNotifyMessageW returns without waiting for the receiving window procedure
when called from another thread. Nonqueued messages take a different route from
the posted FIFO queue. These properties support an asynchronous cleanup wake
when normal posting fails. The API is available on Windows XP.
[SendNotifyMessageW documentation](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendnotifymessagew),
[queued and nonqueued message routing](https://learn.microsoft.com/en-us/windows/win32/winmsg/about-messages-and-message-queues)

Only the maintenance wake uses SendNotifyMessageW. Application completion
callbacks continue to use PostMessage and the container message handler, preserving
deferred execution and its existing ticket/pointer/lifetime validation. This adds
no window subclass, new HWND, VM integration, public ABI, or message-pump change.
Existing main and native modal pumps already retrieve Windows messages.

## Reentrancy, retries and failure boundaries

Call SendNotifyMessageW from the native worker only. Calling it on the UI thread
can invoke the window procedure synchronously, so UI cleanup must not use it to
recursively reschedule itself. Maintain a dirty/pending flag if wakes are
coalesced. A wake observed while reaping is active must leave work marked for the
outer reap/safe-point path; do not silently clear an unprocessed notification.

The notification handler must use the current UI callback/lifetime guards and
retain the dispatcher/runtime while it runs. Source/callback destructors can
reenter code even though the completion callback itself is not invoked. Remove
records before releasing objects, as in the current implementation.

For native unregister failures, perform at most a bounded retry of a known
rejected transient operation. Retain unconfirmed native cleanup state for later
existing UI sweeps, lifecycle checks, or shutdown. Repeated native failure must
not create a self-sustaining stream of maintenance messages or worker jobs.
Persistent failures keep the existing diagnosed native-resource quarantine
behavior instead of freeing context while callbacks may still access it. This
exceptional path does not justify a timer during ordinary waits.

Check SendNotifyMessageW's return value. It is an independent notification route,
not a promise that Windows can never reject notification. If it also fails,
retain the published failure for the next ordinary UI sweep or teardown and
preserve both native error codes for diagnosis. Never run script or perform final
script releases on the worker as a fallback. Target destruction still cancels
the request, so failure to notify does not prevent last-Form teardown from
releasing its callback and application lease. Immediate diagnosis/cleanup cannot
be guaranteed if Windows refuses both notification mechanisms and the UI has no
subsequent activity; no polling loop is proposed to disguise that boundary.

## Verification performed during this investigation

A temporary, isolated VS2008 Win32 probe created its own message-only window and
filled its UI posted-message queue. The UI then blocked without pumping while a
worker called SendNotifyMessageW. After the sender returned, a PM_NOREMOVE peek
dispatched the nonqueued notification without removing any posted filler message.

Observed on this Windows host:

```text
filled=10000
initial_post_error=1816 (ERROR_NOT_ENOUGH_QUOTA)
send_succeeded=1
send_error=0
notification_callbacks=1
correct_ui_thread=1
post_error_after_notification=1816
result=0
```

This verifies both the asynchronous sender behavior and delivery while the
posted queue remains full on the current host. The probe was compiled with the
VS2008 Win32 toolset, /MT and subsystem 5.0. It does not establish XP VM execution
results. Its temporary source, build command, executable and log are under target
as AsyncWaitWakeProbe.* and build-asyncwait-wake-probe.cmd. No production runtime
was changed and no dependency DLL was rebuilt for this investigation.

## Implementation changes

1. Remove m_asyncWaitTimer, AsyncWaitMaintenanceTimer, Start/StopAsyncWaitMaintenance,
   StopIdleWaitTimer, timer creation in preparation, the WM_TIMER branch, and
   timer-specific tests/counters.
2. Add the private maintenance notification and its immutable native delivery
   values. Snapshot the existing lifetime HWND on the UI thread; publish a
   worker failure before sending its pointer-free wake.
3. Route that wake to guarded CAsyncCalls::ReapWaits. Preserve ordinary UI sweep
   cleanup and shutdown draining. Bound cleanup retries and make any coalescing
   state reentrancy-safe.
4. Preserve the current Async/AsyncWait public script contract, native-handle
   ownership, successful delivery path, diagnostics and deferred callback rules.
5. Update the implementation record and README to describe the timer-free
   runtime, failure fallback and validation results.

## Required validation for the revision

- Assert that AsyncWait creates no maintenance timer, including while an
  unsignaled registration remains armed. Track this feature's API calls, not
  unrelated timers used by controls or Windows itself.
- Convert the quota-exhaustion probe into a focused harness test: preserve all
  posted filler messages, send the failure wake, and prove UI cleanup occurs
  while another PostMessage still fails with ERROR_NOT_ENOUGH_QUOTA.
- Test notification arrival before the worker returns and native drain during
  that interval; prove cancellation/shutdown cannot deadlock with the worker.
- Test failed posts during ordinary and nested native modal pumps, plus multiple
  failed waits, reentrant source destruction and a wake during an active reap.
- Inject failure of both posting and maintenance notification. Verify retention
  until the next safe UI sweep/destruction/shutdown, correct UI-thread releases,
  nonfatal diagnostics and no stuck last-Form lease.
- Test a transient unregister rejection followed by successful bounded/event-
  driven cleanup, and persistent rejection without an infinite retry loop.
- Run all existing focused AsyncWait, Async, lifetime and script-adapter tests in
  Win32 Debug and Release. Run the revised failure-notification cases on XP.

This revision requires no change to the two script arguments, INFINITE timeout,
WT_EXECUTEONLYONCE flag, callback invocation, or published interfaces.

## Implementation results

The runtime now uses this design. The notification reaper latches nested wake
requests; cleanup retry is bounded and tied to actual activity. Native lifetime
PostMessage quota failures are deferred to a safe UI sweep instead of becoming
fatal during failure cleanup. No AsyncWait SetTimer/KillTimer/WM_TIMER path remains.

Win32 Debug and Release builds completed without errors or warnings. The focused
suite passes 191 assertions in each configuration, with a separate 5-assertion
non-windowed entry point. Existing Async, lifetime, script, independent handler,
native/private, palette and Studio regressions passed. The permanent notification
coverage is in ApplicationTests/AsyncWaitNotificationTests.h, including the real
full-queue probe and native modal/reentrancy cases. Actual XP VM execution remains
an external follow-up; subsystem 5.0 and XP-compatible imports were verified.
