# Application script adapter

This shared source is compiled into AQVMT and QuadooApplicationTests. It uses
published QuadooForm, JSON and QuadooScript interfaces. QuadooForm.dll has no
QuadooScript dependency. Existing published vtables and exports are unchanged;
final-destruction observation adds two optional native interfaces (004A/004B).

Uniform lifetime revision, October 28, 2026:
[ApplicationLifetimePlan.md](../../../desktop/QuadooStudio/ApplicationLifetimePlan.md)
defines uniform Form ownership and last-Form shutdown. A generated helper returns
before script WinMain calls the argument-free blocking host pump. See
[ApplicationLifetimeImplementation.md](../../../desktop/QuadooStudio/ApplicationLifetimeImplementation.md)
for implementation, validation and the additive final-destruction contract.

## Construction

AQVMT's CAQVMHost derives from CQVMWinHost and implements IExternalClassLoader.
The loader receives the script class name, looks it up in the embedded unit
catalog, and returns a definition object that only implements Create.

```cpp
class MainForm : extern(Form), IMainForm
{
	MainForm ()
	{
		Form = Form.Create(this);
	}
};

class DetailsFrame : extern(Frame), IDetailsFrame
{
	DetailsFrame (Owner)
	{
		Frame = Frame.Create(this, Owner);
	}
};
```

Create returns a distinct live wrapper. CFormObject and CFrameObject share
CContainerObject, which understands IQuadooControlContainer. Owner can be a live
container wrapper or its associated script object. Frames require an owner.
Forms optionally accept an owner as the second Create argument.

Studio generates an owned __QuadooRoot property whose getter/setter access Form
or Frame. PostBind installs the live root through this setter and fills all named
child and nonvisual property variables before OnCreate, while the constructor's
assignment is still pending. The definition loader is never a provisional live
control. __QuadooUnitId is retained as Studio metadata, not used for resource
lookup. Existing user constructors are preserved and must call Create themselves.

Each Create copies its definition. Two frames can share template component IDs
and names while keeping separate native namespaces and script members. Their
runtime root names are unique. Creating two roots for the same script instance
is rejected.

## Native script handlers supplied by external components

A component may expose the published `IQuadooObject` interface through
`QueryInterface`. `CComponentObject` queries the component itself when the wrapper
is constructed or attached, caches an owned reference, and forwards `Invoke`,
`GetProperty`, `SetProperty`, `GetIndexedProperty`, `SetIndexedProperty`,
`GetMemberVariable`, `GetDefaultValue`, and `DeleteProperty`. Neither the factory
nor the component needs to inherit an internal QuadooForm implementation class.
The independent `ExternalScriptMeter` in QuadooFormTestExtension.dll demonstrates
this using published QuadooForm and QuadooVM interfaces and common library bases.

A native handler owns this script-facing dispatch completely. Member-not-found,
not-implemented, pending, and other HRESULTs return to the caller without falling
back to descriptor conversion. VM pointers (including NULL), property names,
indexes, and argument/value variants are forwarded without JSON conversion.
Default indexed access retains its NULL property name; deletion retains an
optional NULL result. Usual QVARIANT ownership rules apply to returned values;
member-variable pointers remain borrowed from the native handler.

The wrapper still enforces component lifetime, exposes its own `State` and
`Destroy`, and retains its container/form convenience operations. COM identity,
service/interface lookup and wrapper destruction are not delegated. Attaching a
replacement component refreshes both owned references before releasing the old
ones, and each forwarded call holds its handler alive across reentrant calls.
Unattached wrappers return EInvalidState; destroyed components return EDestroyed.
Components without a handler retain their descriptor-based script behavior.

This mechanism adds no published ABI or QuadooScript dependency to QuadooForm.dll.
A script-capable extension includes the published QuadooVM header; it still
provides the ordinary component/factory/property interfaces required for native
construction, metadata, designer editing and JSON persistence. Its script handler
can implement additional runtime properties absent from the design metadata.

Focused validation (run after building the TestExtension, AQVMT and
QuadooApplicationTests Win32 projects):

```powershell
.\tests\results\bin\Debug\QuadooApplicationTests.exe --native-handler-only
.\tests\results\bin\Release\QuadooApplicationTests.exe --native-handler-only
```

The fixture registers the independent DLL factory, constructs a control through
the native registry, and runs property/method/indexed access from a real VM. It
also covers object-valued properties, optional deletion results, borrowed member
access, HRESULT preservation, descriptor fallback without a handler, handler
replacement/clearing, null construction, destruction guards, COM identity and
factory/module release. Existing script, lifetime and native regressions run via
`tests\platform\QuadooForm\test-async.ps1 -Configuration All -Full`.

## Script operations and values

| Object/capability | Operations |
| --- | --- |
| Component | Descriptor property get/set, State, Destroy() |
| Visual control | Parent; descriptor bounds/layout/colors/text and other properties |
| Native window | Handle, SetFocus() |
| Container | ControlCount, FindControl(name), GetControl(index), BeginUpdate(), EndUpdate(), UpdateLayout() |
| Form | Show([nCmdShow])/ShowForm([nCmdShow]), Hide()/HideForm(), Close(), ShowModal(); descriptor properties including ModalResult |
| Button | Click() |
| OpenFile/SaveFile/ColorDialog | Execute([owner]) returns an acceptance Boolean |
| TabControl | ActivePage wrapper get/set, SelectNextPage(forward) |
| ComboBox/ListBox | Items uses independent IRStringArray snapshots wrapped through QVMWrapStringArray |

Capabilities are queried from the native object; the adapter does not require
third-party controls to derive from private native classes. Custom descriptor
properties and events work through the same paths. Arbitrary extension-specific
native methods require their own future script capability adapter.

Booleans, strings, integers, structured JSON objects, and collections remain
typed values through the existing VM JSON wrappers. Arrays and IRStringArray
wrappers are copied recursively when assigned. Native JSON validation still
defines the accepted property shape and range. Colors retain their raw 32-bit
Quadoo Color representation; single Color properties accept signed or unsigned
representations of that bit pattern. Opaque unsupported object values fail with
an HRESULT rather than being stringified. No native pointer is exposed through
descriptor property conversion.

Close returns false when native OnCloseQuery cancels closure; accepted closure
returns true. ShowModal returns the native modal result. Named wrappers have
stable identity within an instance. Retained destroyed wrappers permit State
inspection and otherwise return the native destroyed-state error.

## Events and teardown

Create obtains the catalog's named interface through GetInterface. Nonempty
handlers resolve eagerly using ResolveMethod. Delegates retain that interface
and the returned ULONG ordinal and invoke synchronously with one argument.

Event arguments expose EventName, Sender and descriptor-named typed values.
Writable Cancel/Handled/Key/Char/Result fields affect native behavior during the
callback. A retained argument remains readable; all writes after dispatch fail.
Boolean and null handler returns are successful and have no implicit event
effect. Other return types are rejected; VM exception objects are failures.

The adapter composes an internal root OnDestroy delegate on its private runtime
definition. Changing the user OnDestroy binding cannot remove lifecycle tracking.
Child teardown finishes before deferred member/context cleanup. Failed creation
restores previous script properties and destroys partial native state. Successful
teardown clears only script properties still referring to the objects assigned by
that instance, including loaded plugin objects. User replacements are preserved.

Built-in HWND controls expose OnEnter and OnLeave through their inherited event
descriptors. These follow focus within each control's native subtree: enter from
outer container to leaf, leave from leaf to outer container, retaining common
ancestors during sibling changes. Editable ComboBox internal HWND transitions
do not duplicate logical events. Labels, other windowless controls and nonvisual
components do not expose these events. OnFocus/OnBlur keep their existing behavior.

## Runtime QuadooPlugin

CAQVMHost registers a real nonvisual QuadooPlugin factory before any definitions
are loaded. It implements the published component contracts independently of
the built-in native hierarchy. Module is a string, with Name, OnCreate and
OnDestroy supplied by this component's descriptors.

During PostBind, the private IScriptComponent capability supplies the module
object for the named script property instead of an ordinary component wrapper.
It loads Module with the existing Host.LoadQuadoo mechanism (CreateObject receives
null options), then tries `Modules\` followed by the original value if loading or
creation fails. If the project declares that module as a dependency, it also tries
the prepared copy beside the executable. Failure of all applicable attempts aborts
form/frame construction and restores already assigned script properties. The module
is available before any OnCreate callback.
The runtime retains it through teardown; a user replacement of the script property
is not cleared. Module and lifecycle bindings cannot be changed after binding.

The host service is disconnected at shutdown, avoiding a host/component reference
cycle or dangling host if a caller retains a destroyed component/factory. The host
never loads placeholder packages. Module paths otherwise use the existing loader's
lookup and process working-directory rules. Project > Settings now lists explicit
module dependencies to copy beside the executable. One-argument
Host.LoadQuadoo(name) resolves those declared names from that directory without
changing the process working directory.

Caught construction failures permit subsequent creation. Unhandled callbacks
outside construction record a diagnostic and stop the primary pump after callback
unwinding. Every live Form has the same ownership and cleanup rules. Normal exit
waits for final destruction of the last Form, completion of pending activity and
release of explicit application lifetime leases. Hidden/minimized Forms count;
Frames and retained destroyed wrappers do not. Rejected closes and Hide keep the
pump running. Native roots/delegates are torn down before VM/module unload.

Generated WinMain calls __QuadooApplicationStartup(nCmdShow), then returns
Host.RunApplication(). The helper registers the generated cleanup interface and
calls Host.ShowForm(new DefaultClass, nCmdShow). ShowForm is nonblocking and holds
no preferred Form reference. RunApplication accepts no arguments and blocks in
the shared pump, ending startup protection at entry. Native AQVMT invokes WinMain
and processes its result; it does not run another pump afterward.

Final lifecycle observers queue checks to a private message-only HWND. Cleanup
removes records before detaching/releasing them, honors construction/callback
guards, and rechecks activity after user setters or destructors. The adapter uses
QVMReleaseObject for script ownership and the generated interface/ordinal callback
(`return gc();`) to run queued destructors before deciding exit. Retained delegates
keep a detachable binding link and ordinal; the instance owns the interface while
live. A retained delegate cannot keep a detached script target alive. External
WM_QUIT codes survive pending-message drains and nested modal/pump paths.

Native extensions can explicitly join the session with RegisterForm using public
IQuadooForm/lifecycle capabilities. AcquireLifetimeLease is an internal native
seam for future application-owned services, including notification icons. It has
no script UI/API yet; acquisition and final Release are confined to the UI thread,
and shutdown disconnects retained leases. Ordinary components acquire no lease.

The shared WQVM pump routes the application's message handler through
IQuadooFormKeyboard::PreprocessMessage, then dispatches unhandled messages.
It does not call IsDialogMessage a second time. Nested WM_QUIT exit codes and
GetMessage failure cleanup are covered by the runtime harness.

## Resource and deployment contract

ApplicationResources.h is shared by Studio and AQVMT:

- QBC/101: compiled module, with no original-source filename argument.
- QAPPLICATION/104: version 2 catalog with startupClass and unit class/kind/interface/GUID.
- QFORM and QFRAME: definitions named by uppercase script class name.
- RCDATA/105: AQVMT template signature/version marker, validated with PE architecture.

Studio emits neutral-language UTF-16LE JSON resources. The test harness also
exercises UTF-8 resources. Catalogs are limited to 4096 units and resources to
32 MiB. Case-insensitive resource-name collisions and duplicate unit GUIDs are
rejected. Definitions preserve the existing native JSON schema.

Non-debug Run produces bin/Win32/Project.exe or bin/x64/Project.exe and stages
the matching QuadooForm, QuadooVM, QuadooParser, JSON and SimbeyCore DLLs beside
it. Project > Settings also embeds the application icon and stages explicitly
listed module dependencies in that output transaction. The private catalog's
optional modules array contains their file names. See
[ApplicationSettings.md](../../../desktop/QuadooStudio/ApplicationSettings.md).
Arbitrary runtime factory registration, transitive dependency discovery,
redistribution tooling, local debugging and remote staging remain follow-ups.
AQVMT never reads DesignTime
packages or registers their placeholder factories.

## Menus

MenuItems bind OnClick through the normal form/frame interface and become named
script members. Enabled/Checked properties and GetEnabled/SetEnabled,
GetChecked/SetChecked, Toggle and Click methods are available. Form.MainMenu and
Control.PopupMenu accept same-root component wrappers or null; PopupMenu.Popup(x,y)
opens at screen coordinates. See [MenuSupport.md](../MenuSupport.md).

## Deferred container calls

Form, Frame and other container wrappers expose `Async(callback)`. It takes one
non-null IQuadooObject and returns Null after posting to the container's current
HWND. On delivery, the adapter invokes it with NULL caller VM, NULL method and
zero arguments. This defers execution on the UI thread; it is not a worker-thread
API. Ordinary return values are discarded.

Inside a script class, a bound method reference can be posted directly:

```text
function ScheduleRefresh()
{
	Form.Async(Refresh);
}

function Refresh()
{
	Label1.Caption = "Updated";
}
```

Lambdas and global function references also work. Captures remain alive until
delivery or cancellation. Nested message loops can run pending callbacks while
an outer handler is still active. Destroying the target, replacing its HWND,
detaching its binding, or shutting down the session cancels pending work and
releases its references. Hide/Enabled do not cancel work. Deferred failures use
ReportDiagnostic and leave the application and modal dialogs running.

The current VM DLL has an existing limitation with native NULL-method invocation
of script objects that use `default function` forwarding. The same call fails
outside Async with DISP_E_BADVARTYPE. Async accepts the object and diagnoses the
failure normally. A lambda keeps that default call inside the VM and works with
the current binary:

```text
var work = new CustomCallable;
Form.Async(new function()
{
	work();
});
```

Native delivery reuses CContainer::Message and the binder's IQuadooMessageHandler;
no additional window subclass is installed. Independent native containers can
implement IQuadooContainerMessageTarget and the forwarding contract in
Published/QuadooContainerMessages.h. Unsupported containers fail synchronously.
See [AsyncImplementation.md](../AsyncImplementation.md) for ownership, ABI and
validation details.

## Native-handle completion with AsyncWait

```text
var completed = event(null,true);
Form.AsyncWait(completed,new function()
{
	Label1.Caption = "Completed";
});

// Signal from the producer through the event's handle or its Set method.
completed.Set();
```

`AsyncWait(waitObject, callback)` accepts two non-null objects. The first must
expose the existing IQuadooNativeHandle through QueryInterface on that object
itself. GetNativeHandle must return a supported waitable handle; mutexes are not
supported. A component adapter's forwarded IQuadooObject methods do not implicitly
forward its QueryInterface capabilities.

The adapter retains both objects and creates its own non-inheritable duplicate
with SYNCHRONIZE access. It registers exactly INFINITE / WT_EXECUTEONLYONCE. On
signaling, a short native thread-pool callback posts the original callback through
the shared Async message helper. Script invocation still occurs on the UI thread
through CContainer::Message, with NULL VM/method and zero arguments. Registration
returns Null; return values and failure diagnostics follow ordinary Async rules.

Forms, Frames and supported container wrappers share this method. Each call is
independent. Destroying the target/HWND, replacing that HWND, detaching its binding
or shutting down cancels the registration and any pending delivery. Closing the
provider's original handle does not invalidate the adapter's duplicate. Hiding or
disabling the target does not cancel a wait. There is no explicit cancellation
or timeout parameter in this version.

AsyncWait uses no maintenance timer. An unsignaled wait schedules no maintenance
wake. If posting completion fails, the worker publishes its error and sends a
pointer-free SendNotifyMessageW notification to the existing lifetime window.
The UI reaps that failure without executing the completion callback. Normal
completion still uses PostMessage and CContainer::Message.

Cleanup permits one immediate ERROR_BUSY retry, then relies on ordinary UI
sweeps or shutdown. If both posting and notification fail, their error codes are
retained for later UI cleanup or cancellation. Callback failures remain nonfatal,
including during modal dialogs. All final script releases occur on the UI thread.

See [AsyncWaitImplementation.md](../AsyncWaitImplementation.md) for the ownership,
race handling, tests, XP follow-up and exceptional native cleanup behavior.

## Canvas drawing and text measurement

Canvas methods are dispatched through the optional published IQuadooCanvas
capability. The native control remains independent from QuadooScript. Existing
external IQuadooObject forwarding takes precedence over these built-in bindings.

```text
var rgb = Canvas1.ResolveColor(Canvas1.ForeColor);
Canvas1.Clear(Canvas1.ResolveColor(Canvas1.BackColor));
Canvas1.SetFont("Arial", 11, false, false);
var extent = Canvas1.MeasureText("Measured text");
Canvas1.DrawText("Measured text", 12, 12, rgb);
Canvas1.DrawLine(12, 16 + extent.Height, 12 + extent.Width,
    16 + extent.Height, rgb);
Canvas1.DrawImageList(ImageList1, 0, 12, 24 + extent.Height);
var png = Canvas1.ExportPng();
```

Drawing and measured dimensions use physical client pixels; control bounds still
use the framework's 96-DPI logical units. Drawing colors are opaque COLORREF
(0x00BBGGRR). ResolveColor converts symbolic Quadoo Colors at the boundary.
Measurement returns an independent Width/Height object and requires no pixel
surface. ExportPng returns an independently owned image/png binary snapshot.

AutoRedraw defaults to True. With AutoRedraw=False, draw only from OnPaint;
GetPixel and ExportPng are unavailable. Text supports literal ampersands, eight
space tab stops, and CRLF/CR/LF line breaks without automatic wrapping.

See [CanvasImplementation.md](../CanvasImplementation.md) for the complete
method table, lifecycle rules, drawing limits, sample project, and validation.

## Canvas sprites

`Canvas1.CreateSprite()` returns a runtime-only, callable image object. It supports
PNG/BMP/ICO files, PNG binary data, and owned RGB24/RGBA32/BGRA32 pixel copies.
`sprite.Draw(x,y)` and a local variable's `sprite(x,y)` call the same clipped alpha
compositor. Width, Height and IsLoaded are read-only; Clear releases source pixels.

Each Sprite retains IQuadooCanvas and obtains its borrowed GetSurface view afresh
for every draw. It never caches Canvas handles or pixels. No Sprite component,
designer editor, JSON property, SIF binary linkage, or compiler/VM change is needed.
Rebuild consumers for the revised IQuadooCanvas IID ending in 0056.

See [SpriteImplementation.md](../SpriteImplementation.md) for exact binary/stride
contracts, ownership, limits, callable-object syntax, examples and validation.

## AQVMT Host sounds

`Host.PlaySound(source)` plays synchronously; `Host.PlaySoundAsync(source)` uses
SND_ASYNC. A string selects SND_FILENAME. An existing binary object implementing
ILockableStream supplies a complete RIFF/WAVE image through SND_MEMORY. Both
methods return WinMM's immediate Boolean result and use SND_NODEFAULT.

Binary input is validated and copied before playback. Async copies are retained
until another sound replaces them or the application shuts down; no provider
lock, script object, timer or completion callback is retained. Sound does not
keep the application alive after its normal lifetime ends.

See [SoundImplementation.md](../../QuadooScript/AQVMT/SoundImplementation.md) for
ownership, error behavior, platform details and non-audible tests.
