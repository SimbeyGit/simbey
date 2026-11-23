#include "..\..\..\..\platform\QuadooForm\QuadooForm\ComponentRuntime.h"
class CChooserBinder : public CBaseUnknown, public IQuadooEventsBinder, public IQuadooDelegate
{
public:
	UINT accepts, cancels, destroys;
	BOOL destroySender;
	HRESULT callbackResult;
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooEventsBinder)
		UNK_INTERFACE(IQuadooDelegate)
	END_UNK_MAP
	CChooserBinder() : accepts(0), cancels(0), destroys(0), destroySender(FALSE), callbackResult(S_OK) {}
	STDMETHOD(BindEvent)(RSTRING, IQuadooDelegate** delegate) { if(!delegate) return E_POINTER; *delegate = static_cast<IQuadooDelegate*>(this); AddRef(); return S_OK; }
	STDMETHOD(PostBind)(IQuadooControl*) { return S_OK; }
	STDMETHOD(Invoke)(IQuadooEventArgs* args)
	{
		HRESULT hr; CRString name; TStackRef<IQuadooComponent> sender;
		Check(args->GetEventName(&name));
		if(name == RSTRING_CAST(L"OnAccept")) { accepts++; if(destroySender) { Check(args->GetSender(&sender)); Check(sender->Destroy()); } }
		if(name == RSTRING_CAST(L"OnCancel")) cancels++;
		if(name == RSTRING_CAST(L"OnDestroy")) destroys++;
		hr = callbackResult;
	Cleanup:
		return hr;
	}
};
class CChooserContext : public CBaseUnknown, public IQuadooConstructionContext, public QF::IFileDialogDriver, public QF::ITimerStartCheck
{
	IQuadooConstructionContext* m_inner;
public:
	UINT calls, filterIndex;
	BOOL isOpen, recursive;
	DWORD options;
	HWND owner;
	CRString title, filename, filter, directory, extension, selected;
	HRESULT result, recursiveResult;
	IQuadooFileDialog* dialog; // Weak test target; never retained by its context.
	HRESULT timerFailure;
	UINT timerStarts;
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooConstructionContext)
		UNK_INTERFACE(QF::IFileDialogDriver)
		UNK_INTERFACE(QF::ITimerStartCheck)
	END_UNK_MAP
	CChooserContext(IQuadooConstructionContext* inner) : calls(0), filterIndex(1), isOpen(FALSE), recursive(FALSE), options(0), owner(NULL), result(S_FALSE), recursiveResult(S_OK), dialog(NULL), timerFailure(S_OK), timerStarts(0) { SetInterface(m_inner,inner); selected = RSTRING_CAST(L"C:\\QuadooForm-test\\chosen.txt"); }
	~CChooserContext() { m_inner->Release(); }
	STDMETHOD(GetMode)(QuadooForm::ContextMode* mode) { return m_inner->GetMode(mode); }
	STDMETHOD(GetRegistry)(IQuadooControlRegistry** registry) { return m_inner->GetRegistry(registry); }
	STDMETHOD(GetDPI)(UINT* dpi) { return m_inner->GetDPI(dpi); }
	STDMETHOD(GetBinder)(IQuadooEventsBinder** binder) { return m_inner->GetBinder(binder); }
	STDMETHOD(GetHostHandle)(HWND* window) { return m_inner->GetHostHandle(window); }
	STDMETHOD(ReportError)(HRESULT hr, RSTRING path, RSTRING detail) { return m_inner->ReportError(hr,path,detail); }
	STDMETHOD(BeforeStart)(HWND, UINT) { timerStarts++; return timerFailure; }
	STDMETHOD(Choose)(HWND window, BOOL open, RSTRING caption, RSTRING initial, RSTRING patterns, RSTRING initialDir, RSTRING defaultExt, UINT index, DWORD flags, RSTRING* chosen, UINT* chosenIndex)
	{
		calls++; owner = window; isOpen = open; title = caption; filename = initial; filter = patterns; directory = initialDir; extension = defaultExt; options = flags;
		if(recursive && dialog) { BOOL accepted = TRUE; recursiveResult = dialog->Execute(window,&accepted); if(accepted) return E_UNEXPECTED; }
		*chosen = NULL; *chosenIndex = filterIndex;
		if(result == S_OK) RStrSet(*chosen,selected);
		return result;
	}
};
static HRESULT ChooserProperty(IQuadooComponent* component, PCWSTR name, IQuadooProperties** properties, UINT* index)
{
	HRESULT hr; TStackRef<IQuadooPropertyDescriptors> descriptors; UINT count;
	Check(component->GetProperties(properties)); Check((*properties)->GetDescriptors(&descriptors)); Check(descriptors->GetCount(&count));
	for(UINT n = 0; n < count; n++) { TStackRef<IQuadooPropertyDescriptor> descriptor; CRString current; Check(descriptors->GetAt(n,&descriptor)); Check(descriptor->GetName(&current)); if(current == RSTRING_CAST(name)) { *index = n; hr = S_OK; goto Cleanup; } }
	hr = DISP_E_MEMBERNOTFOUND;
Cleanup:
	return hr;
}
struct ChooserThreadCall { IQuadooFileDialog* dialog; HRESULT result; BOOL accepted; };
static DWORD WINAPI ChooserThread(PVOID param) { ChooserThreadCall* call = static_cast<ChooserThreadCall*>(param); call->result = call->dialog->Execute(NULL,&call->accepted); return 0; }
static HRESULT FileDialogTests()
{
	HRESULT hr;
	TStackRef<IQuadooConstructionContext> inner; TStackRef<CChooserContext> context; TStackRef<CChooserBinder> binder;
	TStackRef<IJSONObject> definition; TStackRef<IQuadooControl> root; TStackRef<IQuadooComponentHost> host;
	TStackRef<IQuadooComponentCollection> collection; TStackRef<IQuadooComponent> open, save;
	TStackRef<IQuadooFileDialog> openDialog, saveDialog; TStackRef<IQuadooProperties> properties;
	QuadooForm::ContextOptions options; BOOL accepted; UINT index; CRString filename; HWND window; HANDLE thread = NULL; ChooserThreadCall call;
	binder.Attach(__new CChooserBinder); CheckAlloc(binder);
	ZeroMemory(&options,sizeof(options)); options.cbSize = sizeof(options); options.pBinder = binder;
	Check(QuadooCreateConstructionContext(&options,&inner)); context.Attach(__new CChooserContext(inner)); CheckAlloc(context);
	Check(NativeDefinition(L"{\"type\":\"Form\",\"name\":\"Dialogs\",\"components\":[{\"type\":\"OpenFile\",\"name\":\"Open\",\"properties\":{\"Title\":\"Pick data\",\"Filter\":\"Text|*.txt|All|*.*\",\"InitialDir\":\"C:\\\\QuadooForm-test\",\"DefaultExt\":\"txt\",\"FileName\":\"previous.txt\"},\"events\":{\"OnAccept\":\"Accept\",\"OnCancel\":\"Cancel\"}},{\"type\":\"SaveFile\",\"name\":\"Save\",\"events\":{\"OnAccept\":\"Accept\",\"OnCancel\":\"Cancel\"}}]}",&definition));
	Check(QuadooCreateControl(context,NULL,definition,&root)); VERIFY(context->calls == 0);
	Check(root->QueryInterface(__uuidof(IQuadooComponentHost),reinterpret_cast<PVOID*>(&host))); Check(host->GetComponents(&collection));
	Check(collection->FindComponent(RSTRING_CAST(L"Open"),&open)); Check(collection->FindComponent(RSTRING_CAST(L"Save"),&save));
	Check(open->QueryInterface(__uuidof(IQuadooFileDialog),reinterpret_cast<PVOID*>(&openDialog))); Check(save->QueryInterface(__uuidof(IQuadooFileDialog),reinterpret_cast<PVOID*>(&saveDialog)));
	{ TStackRef<IQuadooWinControl> native; Check(root->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&native))); Check(native->GetHandle(&window)); }
	accepted = TRUE; VERIFY(openDialog->Execute(NULL,&accepted) == S_FALSE && !accepted && context->calls == 1 && binder->cancels == 1);
	VERIFY(context->owner == window && context->isOpen && context->title == RSTRING_CAST(L"Pick data") && context->filename == RSTRING_CAST(L"previous.txt") && context->extension == RSTRING_CAST(L"txt") && context->options == 11);
	Check(ChooserProperty(open,L"FileName",&properties,&index)); Check(properties->GetValueString(index,&filename)); VERIFY(filename == RSTRING_CAST(L"previous.txt")); filename = static_cast<RSTRING>(NULL); properties.Release();
	context->result = E_OUTOFMEMORY; accepted = TRUE; VERIFY(openDialog->Execute(NULL,&accepted) == E_OUTOFMEMORY && !accepted && binder->accepts == 0);
	context->result = S_OK; context->recursive = TRUE; context->dialog = openDialog;
	Check(openDialog->Execute(NULL,&accepted)); VERIFY(accepted && binder->accepts == 1 && context->recursiveResult == QuadooForm::EInvalidState);
	context->dialog = NULL; context->recursive = FALSE;
	Check(ChooserProperty(open,L"FileName",&properties,&index)); Check(properties->GetValueString(index,&filename)); VERIFY(filename == static_cast<RSTRING>(context->selected)); properties.Release(); filename = static_cast<RSTRING>(NULL);
	context->selected = RSTRING_CAST(L"C:\\QuadooForm-test\\failed.txt"); binder->callbackResult = E_ABORT;
	VERIFY(openDialog->Execute(NULL,&accepted) == E_ABORT && !accepted);
	Check(ChooserProperty(open,L"FileName",&properties,&index)); Check(properties->GetValueString(index,&filename)); VERIFY(filename == RSTRING_CAST(L"C:\\QuadooForm-test\\chosen.txt")); filename = static_cast<RSTRING>(NULL); properties.Release();
	binder->callbackResult = S_OK; context->filterIndex = 3; VERIFY(openDialog->Execute(NULL,&accepted) == E_UNEXPECTED && !accepted); context->filterIndex = 1;
	call.dialog = openDialog; call.accepted = TRUE; call.result = S_OK;
	thread = CreateThread(NULL,0,ChooserThread,&call,0,NULL); CheckIf(!thread,HrEnsureLastError());
	VERIFY(WaitForSingleObject(thread,5000) == WAIT_OBJECT_0 && call.result == QuadooForm::EWrongThread && !call.accepted); CloseHandle(thread); thread = NULL;
	context->selected = RSTRING_CAST(L"C:\\QuadooForm-test\\save.txt"); binder->destroySender = TRUE;
	Check(saveDialog->Execute(NULL,&accepted)); VERIFY(accepted && !context->isOpen && context->options == 14);
	QuadooForm::ComponentState state; Check(save->GetState(&state)); VERIFY(state == QuadooForm::Destroyed && saveDialog->Execute(NULL,&accepted) == QuadooForm::EDestroyed);
	Check(root->Destroy()); VERIFY(openDialog->Execute(NULL,&accepted) == QuadooForm::EDestroyed && !IsWindow(window));
Cleanup:
	if(thread) { WaitForSingleObject(thread,5000); CloseHandle(thread); }
	if(context) context->dialog = NULL; if(root) root->Destroy();
	return hr;
}
static HRESULT TimerRollbackTests()
{
	HRESULT hr;
	TStackRef<CChooserBinder> binder; TStackRef<CChooserContext> context; TStackRef<IQuadooConstructionContext> inner;
	TStackRef<IQuadooControlRegistry> registry; TStackRef<IQuadooComponentRegistry> components; TStackRef<IQuadooComponentFactory> factory; TStackRef<IQuadooFactoryLifetime> lifetime;
	TStackRef<IJSONObject> definition; TStackRef<IQuadooControl> root, failed; TStackRef<IQuadooComponentHost> host; TStackRef<IQuadooComponentCollection> collection;
	TStackRef<IQuadooComponent> component; TStackRef<IQuadooTimer> timer; TStackRef<IQuadooWinControl> native;
	QuadooForm::ContextOptions options; BOOL enabled; UINT interval, live, starts; HWND window, designHost = NULL;
	binder.Attach(__new CChooserBinder); CheckAlloc(binder); Check(QuadooGetControlRegistry(&registry));
	Check(registry->QueryInterface(__uuidof(IQuadooComponentRegistry),reinterpret_cast<PVOID*>(&components))); Check(components->FindComponentFactory(RSTRING_CAST(L"Timer"),&factory)); Check(factory->QueryInterface(__uuidof(IQuadooFactoryLifetime),reinterpret_cast<PVOID*>(&lifetime)));
	ZeroMemory(&options,sizeof(options)); options.cbSize = sizeof(options); options.pRegistry = registry; options.pBinder = binder;
	Check(QuadooCreateConstructionContext(&options,&inner)); context.Attach(__new CChooserContext(inner)); CheckAlloc(context); context->timerFailure = E_OUTOFMEMORY;
	Check(NativeDefinition(L"{\"type\":\"Form\",\"name\":\"FailedActivation\",\"components\":[{\"type\":\"Timer\",\"name\":\"Timer1\",\"events\":{\"OnDestroy\":\"Destroyed\"}}]}",&definition));
	VERIFY(QuadooCreateControl(context,NULL,definition,&failed) == E_OUTOFMEMORY && !failed && context->timerStarts == 1 && binder->destroys == 0);
	Check(lifetime->GetLiveCount(&live)); VERIFY(live == 0); definition.Release();
	Check(NativeDefinition(L"{\"type\":\"Form\",\"name\":\"TimerRollback\",\"components\":[{\"type\":\"Timer\",\"name\":\"Timer1\",\"properties\":{\"Enabled\":false,\"Interval\":10},\"events\":{\"OnDestroy\":\"Destroyed\"}}]}",&definition));
	Check(QuadooCreateControl(context,NULL,definition,&root)); Check(root->QueryInterface(__uuidof(IQuadooComponentHost),reinterpret_cast<PVOID*>(&host))); Check(host->GetComponents(&collection)); Check(collection->FindComponent(RSTRING_CAST(L"Timer1"),&component)); Check(component->QueryInterface(__uuidof(IQuadooTimer),reinterpret_cast<PVOID*>(&timer)));
	VERIFY(timer->SetEnabled(TRUE) == E_OUTOFMEMORY); Check(timer->GetEnabled(&enabled)); VERIFY(!enabled);
	context->timerFailure = S_OK; Check(timer->SetEnabled(TRUE)); context->timerFailure = E_OUTOFMEMORY;
	VERIFY(timer->SetInterval(50) == E_OUTOFMEMORY); Check(timer->GetEnabled(&enabled)); Check(timer->GetInterval(&interval)); VERIFY(enabled && interval == 10);
	Check(root->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&native))); Check(native->GetHandle(&window));
	{ MSG message; DWORD started = GetTickCount(); BOOL received = FALSE; while(GetTickCount() - started < 1000) { if(PeekMessageW(&message,window,WM_TIMER,WM_TIMER,PM_REMOVE)) { received = TRUE; DispatchMessageW(&message); break; } MsgWaitForMultipleObjects(0,NULL,FALSE,2,QS_TIMER); } VERIFY(received); }
	starts = context->timerStarts; Check(timer->SetEnabled(FALSE)); VERIFY(starts == context->timerStarts);
	Check(root->Destroy()); timer.Release(); component.Release(); native.Release(); collection.Release(); host.Release(); root.Release();
	VERIFY(binder->destroys == 1); Check(lifetime->GetLiveCount(&live)); VERIFY(live == 0);
	// Neither non-runtime mode asks the OS to start a timer, even if start would fail.
	for(INT mode = QuadooForm::Design; mode <= QuadooForm::Preview; mode++)
	{
		inner.Release(); context.Release(); definition.Release(); options.Mode = static_cast<QuadooForm::ContextMode>(mode);
		if(mode == QuadooForm::Design)
		{ designHost = CreateWindowExW(0,L"STATIC",L"",WS_OVERLAPPED,0,0,640,480,NULL,NULL,GetModuleHandle(NULL),NULL); CheckIf(!designHost,HrEnsureLastError()); options.hwndHost = designHost; }
		else options.hwndHost = NULL;
		Check(QuadooCreateConstructionContext(&options,&inner)); context.Attach(__new CChooserContext(inner)); CheckAlloc(context); context->timerFailure = E_OUTOFMEMORY;
		Check(NativeDefinition(L"{\"type\":\"Form\",\"name\":\"SuppressedTimer\",\"components\":[{\"type\":\"Timer\",\"name\":\"Timer1\",\"properties\":{\"Interval\":1},\"events\":{\"OnTimer\":\"Tick\",\"OnDestroy\":\"Destroyed\"}}]}",&definition));
		Check(QuadooCreateControl(context,NULL,definition,&root)); VERIFY(context->timerStarts == 0); Check(root->Destroy()); root.Release(); VERIFY(binder->destroys == 1);
		if(designHost) { DestroyWindow(designHost); designHost = NULL; }
	}
Cleanup:
	if(root) root->Destroy(); if(failed) failed->Destroy(); if(designHost) DestroyWindow(designHost);
	return hr;
}
