# Uniform application lifetime implementation

Implemented and validated October 28, 2026. Implements
[ApplicationLifetimePlan.md](ApplicationLifetimePlan.md).

The additive public contract, published before implementation, is:

```cpp
// IID D25A0D6A-34B4-48EA-AB90-3C8AB72A004A
interface IQuadooComponentLifecycle : IUnknown
{
	STDMETHOD(AddDestroyedObserver)(IQuadooComponentDestroyedObserver* observer) = 0;
	STDMETHOD(RemoveDestroyedObserver)(IQuadooComponentDestroyedObserver* observer) = 0;
};

// IID D25A0D6A-34B4-48EA-AB90-3C8AB72A004B
interface IQuadooComponentDestroyedObserver : IUnknown
{
	STDMETHOD_(VOID,OnComponentDestroyed)(IQuadooComponent* component) = 0;
};
```

These optional interfaces add no methods to existing vtables. Registration owns
a reference, duplicate registration returns S_FALSE, removal releases a reference,
and notifications occur once after state Destroyed and native/child/context
teardown. Notification borrows a callback-protected component. Registration during
Destroying/Destroyed fails with EDestroyed; removal remains supported. All calls
are on the component's UI thread. An observer removed before its turn is not
called. Observers are released after notification, including on aborted creation.

AQVMT-managed roots require this capability. Independent factories can implement
it through published interfaces without deriving from private native classes.
The adapter's observers keep a detachable weak session link and queue cleanup.

The revised private host contract is ShowForm(script, nCmdShow), returning Null,
followed by argument-free blocking RunApplication(), returning the pump exit code.
Generated WinMain calls a generated startup helper before RunApplication. Catalog
and template revision 2 use startupClass. Project files[].default selects startup;
application.mainForm is no longer used. No VM/compiler changes are required.

## Runtime behavior

Default selects the generated startup class. There is no stored preferred Form,
replacement primary Form, or special retention path. A session registry owns
native roots through canonical IUnknown identity and public Form/lifecycle
capabilities. Script instance records retain every live Form/Frame's bindings
uniformly. Hidden/minimized/owned Forms count; Frames and retained destroyed
wrappers do not. Explicit Win32 ownership and modal owner restoration are
unchanged; an independent modeless Form must not use its creator as native owner.

Generated startup registers an internal __QuadooApplicationLifetime object, whose
I__QuadooApplicationLifetime.CollectReleasedObjects method returns gc(). The
startup helper then creates/shows the Default instance and returns without a
Form reference. WinMain calls blocking Host.RunApplication() and returns its exit
code. AQVMT invokes script WinMain and performs normal teardown after it returns;
there is no second native pump.

The adapter releases script ownership with QVMReleaseObject. This is necessary
to queue a script destructor when the adapter owns the final reference. Deferred
cleanup invokes the generated interface/ordinal callback to run queued
destructors before committing exit. A destructor or cleanup setter can create a
replacement Form. Retained event delegates keep a detachable binding link and
cached ordinal; the live instance owns the resolved methods interface, so a
retained delegate cannot pin a detached script object.

The session has one message-only dispatch HWND. Construction protection covers
the entire operation, including external interface/owner lookups, rollback and
binding release. Keyboard preprocessing skips uncommitted instances. Callback and
cleanup guards defer checks during re-entry; records are removed before foreign
setters/releases, and bounded cleanup passes re-evaluate activity generations.
The long-lived WinMain/RunApplication frames do not block lifetime decisions.

Normal exit commits only after the last Form's completed destruction, cleanup,
callbacks and construction have finished, with no explicit lifetime lease. It
posts normal WM_QUIT once. Pending ordinary messages are drained first because
they can mask Win32's synthetic low-priority quit notification; an existing quit
code is preserved. Empty startup may briefly drain pending work, then returns
without an idle wait or stray WM_QUIT. Fatal errors stop admissions and unwind
through the pump before forced native cleanup and VM/module unload.

Native RegisterForm supports independent published-interface implementations.
AcquireLifetimeLease supplies the internal seam for future application-owned
services. Its final Release is UI-thread-affine, and retained leases disconnect
safely at shutdown. No tray component, public script lease API or application
component collection was introduced.

## Studio and naming

Removed m_main, SetMain, ShowMain, MainDestroyed and the exceptional Sweep rule.
The builder and catalog use startupClass; template/catalog revision 2 rejects
stale templates. User-chosen class/file names such as MainForm are unchanged.
Runtime test fixtures use StartupForm, with the JSON fixture renamed in SVN.

files[].default is the only project startup selection. New forms do not write
application.mainForm. Loading ignores a legacy cache, including malformed values;
an explicit ordinary project Save removes it while retaining other settings.
Opening alone does not rewrite the file. The project's schema remains version 1.

## Validation

VS2008 builds of the runtime DLL, extension, native/private/designer harnesses,
Designer core/host, AQVMT, Studio and its application/integration tests pass in all
four configurations without warnings. Builds target individual projects and do
not rebuild SimbeyCore.dll, JSON.dll or compiler/VM dependency DLLs. VS2008's PDB
service requires execution outside the Codex sandbox; the original tool settings
are preserved.

| Suite | Win32 Debug | Win32 Release | x64 Debug | x64 Release |
| --- | ---: | ---: | ---: | ---: |
| New lifetime suite | 192 | 192 | 192 | 192 |
| Existing application runtime | 133 | 133 | 133 | 133 |
| Full native harness | 9,541 | 9,541 | 9,541 | 9,541 |
| Full private native suite | 1,544 | 1,544 | 1,544 | 1,544 |
| Studio application build/run | 160 | 160 | 160 | 160 |
| Studio project creation/migration | 45 | 45 | 45 | 45 |
| Studio source synchronization | 106 | 106 | 106 | 106 |

The new suite includes:

- All six close orders for three independent native Forms, hidden Forms,
  duplicate registration, partial observer-registration failure and rollback.
- Re-entry during observer registration and early script-interface lookup,
  including closing the last existing Form while another creation is pending.
- Native-only and nonvisual completion timing, observer removal during
  notification, canonical COM identity, failed initialization and missing root
  capability diagnostics.
- Startup closing in its constructor, survival of another Form, both close
  orders, creation from OnDestroy, a script destructor and a cleanup setter.
- Nested modal loops consuming earlier wakes, followed by correct final cleanup
  and exactly-once normal quit.
- Startup helper failure, recursive/wrong-phase/wrong-signature primary pump
  calls, fatal destructor errors and external WM_QUIT 123 with live or already
  destroyed Forms.
- Retained event delegates after detachment, destructor-aware reference release,
  leases with zero Forms, late lease release after shutdown, and GDI/USER counts.

Existing application tests now reach their normal shutdown/keyboard/frame checks:
an unconditional callback-failure test block was restricted to its intended
failure scenario. Studio tests inspect the new embedded catalog, reject generated
helper name collisions, run copied executables and verify legacy-cache handling.

Logs are under tests/results/ApplicationLifetimeLogs. The full native resource suite
needs more than the initial 45-second runner allowance; its watchdog is now 180
seconds with one-second waits. Resource assertions were not relaxed. Final adapter
changes were followed by application/Studio reruns in all four configurations;
the native DLL was unchanged after its passing full native/private runs.

Reproduce from the repository root in a normal developer terminal:

```powershell
foreach ($configuration in 'Debug', 'Release')
{
	.\tests\platform\QuadooForm\build-application-lifetime.cmd $configuration Win32
	.\tests\platform\QuadooForm\build-application-lifetime.cmd $configuration x64
}
.\tests\platform\QuadooForm\test-application-lifetime.ps1 -Platform Win32 -Full
.\tests\platform\QuadooForm\test-application-lifetime.ps1 -Platform x64 -Full
```

Use -Integration for lifetime/application/Studio checks without repeating the
native suites, or omit both switches for only the new lifetime suite.

## Windows XP and remaining scope

The Win32 Release AQVMT.bin, QuadooForm.dll and QuadooStudio.exe headers retain
operating-system/subsystem version 5.00. New native calls use XP-compatible
message-only windows, window messages and COM-style public interfaces. No new
OS DLL dependency was added. PE inspection is recorded in Win32-headers.log.

Actual XP VM execution remains to be done on the user's VM. Copy the matching
Win32 Release test executable and runtime DLLs plus LifetimeRuntime.quadoo, then
run:

```bat
QuadooApplicationTests.exe --lifetime-only C:\QS\LifetimeRuntime.quadoo
QuadooFormHarness.exe --trace
```

Also run an Application project in XP Studio that opens an independent modeless
second Form, closes the startup Form first, interacts with the survivor and then
closes it. Rebuild projects to embed the new AQVMT host; existing generated EXEs
retain their old embedded host until rebuilt.

Future notification-area services need application-level ownership and an explicit
lease; ordinary Form-owned Timers/components add no implicit lifetime. Local
Application debugging is the next runtime stage and must account for the private
generated startup/cleanup functions in its generated-source mappings.
