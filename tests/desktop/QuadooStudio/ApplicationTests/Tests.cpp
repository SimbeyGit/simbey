#include <windows.h>
#include <commctrl.h>
#include <crtdbg.h>
#include <rtcapi.h>
#include <dbghelp.h>
#pragma comment(lib,"dbghelp.lib")
#include "Library\Core\CoreDefs.h"
#include "Library\DPI.h"
#include "..\..\..\..\platform\QuadooScript\Published\TestSupport\AQVMT\ApplicationHost.h"
#include "..\..\..\..\platform\QuadooScript\Published\TestSupport\WQVM\QVMMessagePump.h"
#include "..\..\..\..\platform\QuadooForm\ScriptAdapter\RuntimeObjects.h"
#include "..\..\..\platform\QuadooForm\TestExtension\ExternalControl.h"
#include "Library\Util\TextHelpers.h"
#include "Library\Util\Formatting.h"

#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

static UINT s_checks;

static LONG WINAPI TraceCrash(EXCEPTION_POINTERS* exception)
{
	HANDLE process = GetCurrentProcess();
	CONTEXT context = *exception->ContextRecord;
	STACKFRAME64 frame = {0};
#ifdef _WIN64
	DWORD machine = IMAGE_FILE_MACHINE_AMD64;
	frame.AddrPC.Offset = context.Rip;
	frame.AddrFrame.Offset = context.Rbp;
	frame.AddrStack.Offset = context.Rsp;
#else
	DWORD machine = IMAGE_FILE_MACHINE_I386;
	frame.AddrPC.Offset = context.Eip;
	frame.AddrFrame.Offset = context.Ebp;
	frame.AddrStack.Offset = context.Esp;
#endif
	frame.AddrPC.Mode = AddrModeFlat;
	frame.AddrFrame.Mode = AddrModeFlat;
	frame.AddrStack.Mode = AddrModeFlat;
	SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
	SymInitialize(process,NULL,TRUE);
	for(UINT n = 0; n < 24; n++)
	{
		BYTE buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME] = {0};
		SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer);
		IMAGEHLP_LINE64 line = {sizeof(line)};
		DWORD64 displacement;
		DWORD offset;
		CHAR text[2000];
		DWORD written;
		symbol->SizeOfStruct = sizeof(*symbol);
		symbol->MaxNameLen = MAX_SYM_NAME;
		SymFromAddr(process,frame.AddrPC.Offset,&displacement,symbol);
		SymGetLineFromAddr64(process,frame.AddrPC.Offset,&offset,&line);
		INT length = wsprintfA(text,"crash %p %s at %s:%u\r\n",reinterpret_cast<PVOID>(static_cast<UINT_PTR>(frame.AddrPC.Offset)),symbol->Name,line.FileName ? line.FileName : "?",line.LineNumber);
		WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),text,length,&written,NULL);
		if(!StackWalk64(machine,process,GetCurrentThread(),&frame,&context,NULL,SymFunctionTableAccess64,SymGetModuleBase64,NULL))
		{
			break;
		}
	}
	return EXCEPTION_EXECUTE_HANDLER;
}

static VOID Stage(PCSTR text)
{
	DWORD written;
	WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),text,lstrlenA(text),&written,NULL);
}

#ifdef _DEBUG
static INT __cdecl TraceRuntime(INT, PCSTR file, INT lineNumber, PCSTR, PCSTR format, ...)
{
	CHAR text[4096];
	va_list arguments;
	va_start(arguments,format);
	wvsprintfA(text,format,arguments);
	va_end(arguments);
	Stage(text);
	Stage("\r\n");
	wsprintfA(text,"%s:%d\r\n",file ? file : "?",lineNumber);
	Stage(text);
	PVOID frames[24];
	USHORT count = CaptureStackBackTrace(0,ARRAYSIZE(frames),frames,NULL);
	HANDLE process = GetCurrentProcess();
	SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
	SymInitialize(process,NULL,TRUE);
	for(USHORT n = 0; n < count; n++)
	{
		BYTE buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME] = {0};
		SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer);
		IMAGEHLP_LINE64 line = {sizeof(line)};
		DWORD offset;
		DWORD64 displacement;
		symbol->SizeOfStruct = sizeof(*symbol);
		symbol->MaxNameLen = MAX_SYM_NAME;
		SymFromAddr(process,reinterpret_cast<UINT_PTR>(frames[n]),&displacement,symbol);
		SymGetLineFromAddr64(process,reinterpret_cast<UINT_PTR>(frames[n]),&offset,&line);
		wsprintfA(text,"%s %s:%u\r\n",symbol->Name,line.FileName ? line.FileName : "?",line.LineNumber);
		Stage(text);
	}
	ExitProcess(2);
	return 0;
}

static VOID WINAPI TraceCheck(HRESULT hr, PCSTR file, UINT line)
{
	CHAR text[500];
	DWORD written;
	INT length = wsprintfA(text,"check 0x%.8x at %s:%u\r\n",hr,file,line);
	WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),text,length,&written,NULL);
}
#endif

static BOOL Expect(BOOL condition, UINT line)
{
	s_checks++;
	if(!condition)
	{
		WCHAR text[100];
		Formatting::TPrintF(text,ARRAYSIZE(text),NULL,L"FAILED line %u\r\n",line);
		DWORD written;
		CHAR narrow[100];
		INT length = WideCharToMultiByte(CP_UTF8,0,text,-1,narrow,ARRAYSIZE(narrow),NULL,NULL);
		WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),narrow,length - 1,&written,NULL);
	}
	return condition;
}

#define REQUIRE(expression) CheckIf(!Expect((expression),__LINE__),E_FAIL)

class CCompilerStatus : public IQuadooCompilerStatus
{
public:
	VOID STDMETHODCALLTYPE OnCompilerAddFile(PCWSTR, INT)
	{
	}
	VOID STDMETHODCALLTYPE OnCompilerStatus(PCWSTR)
	{
	}
	VOID STDMETHODCALLTYPE OnCompilerError(HRESULT hr, INT line, PCWSTR file, PCWSTR error)
	{
		WCHAR text[1200];
		CHAR narrow[2400];
		DWORD written;
		Formatting::TPrintF(text,ARRAYSIZE(text),NULL,L"%ls:%d: %ls (0x%.8x)\r\n",file,line,error,hr);
		INT length = WideCharToMultiByte(CP_UTF8,0,text,-1,narrow,ARRAYSIZE(narrow),NULL,NULL);
		WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),narrow,length - 1,&written,NULL);
	}
	STDMETHOD(OnCompilerResolvePath)(PCWSTR, PWSTR, INT)
	{
		return E_NOTIMPL;
	}
};

static HRESULT Call(IQuadooVM* vm, PCWSTR function, QuadooVM::QVARIANT* result)
{
	HRESULT hr;
	ULONG ordinal;
	DWORD parameters;
	Check(vm->FindFunction(function,&ordinal,&parameters));
	CheckIf(parameters,DISP_E_BADPARAMCOUNT);
	Check(vm->RunFunction(ordinal,result));
Cleanup:
	return hr;
}

static HRESULT PumpContracts(CAQVMHost* host)
{
	HRESULT hr;
	TStackRef<CQVMMessagePump> outer, inner;
	TStackRef<CQVMRunController> controller;
	QuadooVM::QVARIANT result = {QuadooVM::Null}, exitCode = {QuadooVM::Null};
	QuadooVM::QVPARAMS parameters = {0};
	MSG message;
	outer.Attach(__new CQVMMessagePump(host));
	inner.Attach(__new CQVMMessagePump(host));
	controller.Attach(__new CQVMRunController(&exitCode));
	CheckAlloc(outer);
	CheckAlloc(inner);
	CheckAlloc(controller);
	Check(outer->UseController(controller));
	PostQuitMessage(123);
	Check(inner->Invoke(NULL,RSTRING_CAST(L"Run"),&parameters,&result));
	REQUIRE(PeekMessageW(&message,NULL,WM_QUIT,WM_QUIT,PM_REMOVE) && message.wParam == 123);
	Check(outer->UseController(NULL));
	REQUIRE(inner->UseController(NULL) == S_FALSE);
Cleanup:
	if(outer)
	{
		outer->UseController(NULL);
	}
	QVMClearVariant(&result);
	QVMClearVariant(&exitCode);
	return hr;
}

#include "ListAdapterTests.h"
#include "CanvasScriptTests.h"
#include "SpriteTests.h"
#include "SoundTests.h"
#include "DateTimePickerTests.h"

static HRESULT TestRuntime(HINSTANCE module, PCWSTR scriptPath, UINT scenario = 0, BOOL treeOnly = FALSE, BOOL canvasOnly = FALSE)
{
	HRESULT hr, registered;
	LONG spriteObjects = AQVM::SpriteLiveObjects();
	LONG spriteBytes = AQVM::SpriteAllocatedBytes();
	TStackRef<CAQVMHost> host;
	TStackRef<IQuadooInstanceLoader> loader;
	TStackRef<IQuadooVM> vm;
	TStackRef<IQuadooObject> exception;
	TStackRef<IJSONObject> catalog;
	TStackRef<IQuadooControlRegistry> registry;
	TStackRef<IQuadooControlFactory> factory;
	TStackRef<IQuadooFactoryLifetime> lifetime;
	TStackRef<IQuadooComponent> component, frameComponent;
	TStackRef<IQuadooForm> form;
	TStackRef<IQuadooWinControl> window;
	TStackRef<IQuadooControlContainer> panel;
	TStackRef<IExternalClassLoader> external;
	TStackRef<IQVMMessageHandler> handler;
	TStackRef<IUnknown> identityA, identityB;
	QuadooVM::QVARIANT script = {QuadooVM::Null}, root = {QuadooVM::Null}, button = {QuadooVM::Null};
	QuadooVM::QVARIANT value = {QuadooVM::Null}, owner = {QuadooVM::Null}, frame = {QuadooVM::Null}, deferred = {QuadooVM::Null};
	QuadooVM::QVPARAMS parameters = {0};
	CMemoryStream byteCode;
	CMemoryStream pluginCode;
	CRString scriptDirectory;
	CRString pluginPath;
	CCompilerStatus compiler;
	CHostedModules modules;
	HWND handle = NULL;
	PCWSTR catalogText = L"{\"schemaVersion\":2,\"startupClass\":\"StartupForm\",\"units\":["
		L"{\"class\":\"StartupForm\",\"kind\":\"form\",\"eventsInterface\":\"IStartupForm\",\"unitId\":\"{E6BF4AEC-5904-42E1-AAA7-000000000001}\"},"
		L"{\"class\":\"DetailsFrame\",\"kind\":\"frame\",\"eventsInterface\":\"IDetailsFrame\",\"unitId\":\"{E6BF4AEC-5904-42E1-AAA7-000000000010}\"},"
		L"{\"class\":\"DeferredForm\",\"kind\":\"form\",\"eventsInterface\":\"IDeferredForm\",\"unitId\":\"{E6BF4AEC-5904-42E1-AAA7-000000000020}\"},"
		L"{\"class\":\"ThrowForm\",\"kind\":\"form\",\"eventsInterface\":\"IThrowForm\",\"unitId\":\"{E6BF4AEC-5904-42E1-AAA7-000000000030}\"},"
		L"{\"class\":\"TreeForm\",\"kind\":\"form\",\"eventsInterface\":\"ITreeForm\",\"unitId\":\"{F3000000-0000-0000-0000-000000000001}\"},"
		L"{\"class\":\"ListForm\",\"kind\":\"form\",\"eventsInterface\":\"IListForm\",\"unitId\":\"{F4000000-0000-0000-0000-000000000001}\"},"
		L"{\"class\":\"VirtualListForm\",\"kind\":\"form\",\"eventsInterface\":\"IVirtualListForm\",\"unitId\":\"{F4000000-0000-0000-0000-000000000002}\"},"
		L"{\"class\":\"SpriteForm\",\"kind\":\"form\",\"eventsInterface\":\"ISpriteForm\",\"unitId\":\"{F9000000-0000-0000-0000-000000000001}\"},"
		L"{\"class\":\"CanvasForm\",\"kind\":\"form\",\"eventsInterface\":\"ICanvasForm\",\"unitId\":\"{F8000000-0000-0000-0000-000000000001}\"},"
		L"{\"class\":\"StatusBarForm\",\"kind\":\"form\",\"eventsInterface\":\"IStatusBarForm\",\"unitId\":\"{F7000000-0000-0000-0000-000000000001}\"},"
		L"{\"class\":\"ProgressBarForm\",\"kind\":\"form\",\"eventsInterface\":\"IProgressBarForm\",\"unitId\":\"{F6000000-0000-0000-0000-000000000001}\"},"
		L"{\"class\":\"ImageButtonForm\",\"kind\":\"form\",\"eventsInterface\":\"IImageButtonForm\",\"unitId\":\"{F5000000-0000-0000-0000-000000000001}\"},"
		L"{\"class\":\"MenuForm\",\"kind\":\"form\",\"eventsInterface\":\"IMenuForm\",\"unitId\":\"{F2000000-0000-0000-0000-000000000001}\"},"
		L"{\"class\":\"MissingPluginForm\",\"kind\":\"form\",\"eventsInterface\":\"IMissingPluginForm\",\"unitId\":\"{E6BF4AEC-5904-42E1-AAA7-000000000040}\"}]}";
	Stage("Compile application and plugin fixtures.\r\n");
	Check(AQVM::ParseObject(catalogText,TStrLenAssert(catalogText),&catalog));
	Check(QuadooParseToStream(scriptPath,0,&byteCode,NULL,&compiler));
	{
		PCWSTR leaf = TStrRChr(scriptPath,L'\\');
		CheckIf(!leaf,E_INVALIDARG);
		Check(RStrCreateW(static_cast<INT>(leaf - scriptPath),scriptPath,&scriptDirectory));
		Check(RStrFormatW(&pluginPath,L"%r\\PluginModule.quadoo",static_cast<RSTRING>(scriptDirectory)));
		Check(QuadooParseToStream(pluginPath.c_wstr(),0,&pluginCode,NULL,&compiler));
	}
	host.Attach(__new CAQVMHost);
	Stage("Initialize application host and factories.\r\n");
	CheckAlloc(host);
	Check(host->Initialize(&modules,RSTRING_CAST(scriptPath),0,NULL));
	Check(QuadooGetControlRegistry(&registry));
	Check(CreateExternalFactory(&factory));
	Check(registry->RegisterFactory(factory));
	Check(factory->QueryInterface(&lifetime));
	Check(host->InitializeApplication(module,catalog,registry));
	{
		TStackRef<CAQVMHost> rejected;
		TStackRef<IQuadooComponentRegistry> components;
		TStackRef<IQuadooComponentFactory> before;
		TStackRef<IQuadooComponentFactory> after;
		Check(registry->QueryInterface(&components));
		Check(components->FindComponentFactory(RSTRING_CAST(L"QuadooPlugin"),&before));
		rejected.Attach(__new CAQVMHost);
		CheckAlloc(rejected);
		REQUIRE(FAILED(rejected->InitializeApplication(module,catalog,registry)));
		rejected->ShutdownApplication();
		Check(components->FindComponentFactory(RSTRING_CAST(L"QuadooPlugin"),&after));
		REQUIRE(AQVM::SameIdentity(before,after));
	}
	Stage("Application host initialized.\r\n");
	Check(PumpContracts(host));
	Check(host->QueryInterface(__uuidof(IExternalClassLoader),reinterpret_cast<PVOID*>(&external)));
	Check(host->QueryInterface(__uuidof(IQVMMessageHandler),reinterpret_cast<PVOID*>(&handler)));
	Check(external->QueryInterface(IID_IUnknown,reinterpret_cast<PVOID*>(&identityA)));
	Check(handler->QueryInterface(IID_IUnknown,reinterpret_cast<PVOID*>(&identityB)));
	REQUIRE(static_cast<IUnknown*>(identityA) == static_cast<IUnknown*>(identityB));
	Check(QVMCreateLoader(NULL,&loader));
	host->AttachLoader(loader);
	Check(loader->AddInstance(RSTRING_CAST(L"DirectPlugin.qbc"),NULL,&pluginCode,pluginCode.DataRemaining(),NULL));
	{
		ULONG position;
		Check(pluginCode.Seek(0,FILE_BEGIN,&position));
	}
	Check(loader->AddInstance(RSTRING_CAST(L"Modules\\FallbackPlugin.qbc"),NULL,&pluginCode,pluginCode.DataRemaining(),NULL));
	Check(loader->AddInstance(RSTRING_CAST(L"ApplicationTests"),NULL,&byteCode,byteCode.DataRemaining(),NULL));
	Check(loader->LoadVM(RSTRING_CAST(L"ApplicationTests"),&registered,&vm));
	Check(vm->AddGlobal(RSTRING_CAST(L"Host"),host));
	Check(vm->AddExternalClassLoader(host));
	Check(vm->RunConstructor(&exception));
	REQUIRE(!exception);
	Check(host->Runtime()->BeginScriptEntry());
	{
		PCWSTR const names[] = {L"NonobjectPlugin.qbc",L"ThrowingPlugin.qbc"};
		PCWSTR const sources[] = {L"function CreateObject(options) { return 5; }",L"function CreateObject(options) { throw \"Plugin test failure\"; }"};
		for(UINT n = 0; n < ARRAYSIZE(names); n++)
		{
			CMemoryStream bad;
			CRString fallback;
			TStackRef<IQuadooObject> loaded;
			ULONG position;
			Check(QuadooParseTextToStream(names[n],TStrLenAssert(names[n]),sources[n],TStrLenAssert(sources[n]),0,&bad,NULL,&compiler));
			Check(loader->AddInstance(RSTRING_CAST(names[n]),NULL,&bad,bad.DataRemaining(),NULL));
			Check(RStrFormatW(&fallback,L"Modules\\%ls",names[n]));
			Check(pluginCode.Seek(0,FILE_BEGIN,&position));
			Check(loader->AddInstance(fallback,NULL,&pluginCode,pluginCode.DataRemaining(),NULL));
			Check(host->LoadPluginModule(RSTRING_CAST(names[n]),&loaded));
			Check(loaded->Invoke(NULL,RSTRING_CAST(L"Ping"),&parameters,&value));
			REQUIRE(value.eType == QuadooVM::I4 && value.lVal == 73);
			QVMClearVariant(&value);
		}
	}
	if(!treeOnly)
	{
		Check(CanvasScriptChecks(vm));
		Check(SpriteNativeTests());
		Check(SpriteScriptChecks(vm));
	}
	if(canvasOnly)
	{
		hr = S_OK;
		goto Cleanup;
	}
	Check(Call(vm,L"Trees",&value));
	if(value.eType != QuadooVM::I4 || value.lVal != 73)
	{
		CRString detail;
		CHAR message[2048];
		if(SUCCEEDED(QVMVariantToString(&value,&detail)))
		{
			WideCharToMultiByte(CP_UTF8,0,detail.c_wstr(),-1,message,ARRAYSIZE(message),NULL,NULL);
			Stage(message);
			Stage("\r\n");
		}
	}
	REQUIRE(value.eType == QuadooVM::I4 && value.lVal == 73);
	QVMClearVariant(&value);
	if(treeOnly)
	{
		hr = S_OK;
		goto Cleanup;
	}
	Check(Call(vm,L"Lists",&value));
	if(value.eType != QuadooVM::I4 || value.lVal != 83)
	{
		QuadooVM::QVARIANT progress = {QuadooVM::Null};
		if(SUCCEEDED(Call(vm,L"ListDiagnostics",&progress)) && progress.eType == QuadooVM::String)
		{
			CHAR diagnostic[256];
			WideCharToMultiByte(CP_UTF8,0,RStrToWide(progress.rstrVal),-1,diagnostic,ARRAYSIZE(diagnostic),NULL,NULL);
			Stage(diagnostic);
			Stage("\r\n");
		}
		QVMClearVariant(&progress);
		CRString detail;
		CHAR message[2048];
		if(SUCCEEDED(QVMVariantToString(&value,&detail)))
		{
			WideCharToMultiByte(CP_UTF8,0,detail.c_wstr(),-1,message,ARRAYSIZE(message),NULL,NULL);
			Stage(message);
			Stage("\r\n");
		}
	}
	REQUIRE(value.eType == QuadooVM::I4 && value.lVal == 83);
	QVMClearVariant(&value);
	Check(Call(vm,L"VirtualLists",&value));
	if(value.eType != QuadooVM::I4)
	{
		CRString detail;
		CHAR diagnostic[2048];
		if(SUCCEEDED(QVMVariantToString(&value,&detail)))
		{
			WideCharToMultiByte(CP_UTF8,0,detail.c_wstr(),-1,diagnostic,ARRAYSIZE(diagnostic),NULL,NULL);
			Stage(diagnostic);
			Stage("\r\n");
		}
	}
	REQUIRE(value.eType == QuadooVM::I4 && value.lVal == 94);
	QVMClearVariant(&value);
	Check(TestListAdapters(vm,host));
	Check(Call(vm,L"StatusBars",&value));
	REQUIRE(value.eType == QuadooVM::Bool && value.fVal);
	QVMClearVariant(&value);
	Check(Call(vm,L"ProgressBars",&value));
	REQUIRE(value.eType == QuadooVM::Bool && value.fVal);
	QVMClearVariant(&value);
	Check(Call(vm,L"ImageButtons",&value));
	REQUIRE(value.eType == QuadooVM::Bool && value.fVal);
	QVMClearVariant(&value);
	Check(Call(vm,L"Menus",&value));
	if(value.eType != QuadooVM::I4 || value.lVal != 42)
	{
		CRString detail;
		CHAR message[2048];
		if(SUCCEEDED(QVMVariantToString(&value,&detail)))
		{
			WideCharToMultiByte(CP_UTF8,0,detail.c_wstr(),-1,message,ARRAYSIZE(message),NULL,NULL);
			Stage(message);
			Stage("\r\n");
		}
		if(RStrLen(host->Runtime()->Diagnostic()))
		{
			WideCharToMultiByte(CP_UTF8,0,RStrToWide(host->Runtime()->Diagnostic()),-1,message,ARRAYSIZE(message),NULL,NULL);
			Stage(message);
			Stage("\r\n");
		}
	}
	REQUIRE(value.eType == QuadooVM::I4 && value.lVal == 42);
	QVMClearVariant(&value);
	Check(Call(vm,L"MissingPlugin",&value));
	REQUIRE(value.eType == QuadooVM::Bool && value.fVal);
	REQUIRE(SUCCEEDED(host->Runtime()->Error()));
	REQUIRE(RStrLen(host->Runtime()->Diagnostic()) > 0);
	QVMClearVariant(&value);
	if(scenario == 1)
	{
		HRESULT thrown = Call(vm,L"MakeThrow",&value);
		REQUIRE(FAILED(thrown) || value.eType == QuadooVM::Object);
		REQUIRE(SUCCEEDED(host->Runtime()->Error()));
		REQUIRE(RStrLen(host->Runtime()->Diagnostic()) > 0);
		REQUIRE(wcsstr(RStrToWide(host->Runtime()->Diagnostic()),L"Deliberate initialization failure") != NULL);
		QVMClearVariant(&value);
		Check(Call(vm,L"Deferred",&deferred));
		Check(deferred.pObject->Invoke(NULL,RSTRING_CAST(L"Start"),&parameters,&value));
		Check(deferred.pObject->GetProperty(NULL,RSTRING_CAST(AQVM::RootProperty),&root));
		REQUIRE(root.eType == QuadooVM::Object);
		hr = S_OK;
		goto Cleanup;
	}
	{
		UINT live;
		Check(lifetime->GetLiveCount(&live));
		REQUIRE(live == 0);
	}
	Check(Call(vm,L"NoOwner",&value));
	REQUIRE(value.eType == QuadooVM::Bool && value.fVal);
	QVMClearVariant(&value);
	Check(Call(vm,L"Deferred",&deferred));
	Stage("Deferred loader created.\r\n");
	Check(deferred.pObject->GetProperty(NULL,RSTRING_CAST(AQVM::RootProperty),&root));
	REQUIRE(root.eType == QuadooVM::Object);
	REQUIRE(FAILED(root.pObject->GetProperty(NULL,RSTRING_CAST(L"Caption"),&value)));
	QVMClearVariant(&root);
	Check(deferred.pObject->Invoke(NULL,RSTRING_CAST(L"Start"),&parameters,&value));
	Stage("Deferred form created.\r\n");
	Check(deferred.pObject->GetProperty(NULL,RSTRING_CAST(AQVM::RootProperty),&root));
	Check(host->Runtime()->Unwrap(root.pObject,&component));
	REQUIRE(SUCCEEDED(component->QueryInterface(&form)));
	Check(component->Destroy());
	QVMClearVariant(&root);
	QVMClearVariant(&deferred);
	component.Release();
	form.Release();
	host->Runtime()->Sweep();
	Check(Call(vm,L"Make",&script));
	Stage("Startup form created.\r\n");
	QVMClearVariant(&value);
	Check(Call(vm,L"PluginCounts",&value));
	REQUIRE(value.eType == QuadooVM::I4 && value.lVal == 10);
	QVMClearVariant(&value);
	{
		UINT live;
		Check(lifetime->GetLiveCount(&live));
		REQUIRE(live == 1);
	}
	Check(script.pObject->GetProperty(NULL,RSTRING_CAST(AQVM::RootProperty),&root));
	{
		TStackRef<IQuadooComponentRegistry> components;
		TStackRef<IQuadooComponentFactory> plugin;
		TStackRef<IQuadooFactoryLifetime> live;
		UINT count;
		Check(registry->QueryInterface(&components));
		Check(components->FindComponentFactory(RSTRING_CAST(L"QuadooPlugin"),&plugin));
		Check(plugin->QueryInterface(&live));
		Check(live->GetLiveCount(&count));
		REQUIRE(count == 2);
	}
	Check(host->Runtime()->Unwrap(root.pObject,&component));
	Check(component->QueryInterface(&form));
	Check(component->QueryInterface(&window));
	Check(window->GetHandle(&handle));
	REQUIRE(IsWindow(handle));
	{
		QuadooVM::QVARIANT color = {QuadooVM::I8};
		LONGLONG raw;
		color.llVal = 0x8000000Fui64;
		Check(root.pObject->SetProperty(NULL,RSTRING_CAST(L"BackColor"),&color));
		Check(root.pObject->GetProperty(NULL,RSTRING_CAST(L"BackColor"),&value));
		Check(AQVM::VariantInteger(&value,&raw));
		REQUIRE(static_cast<DWORD>(raw) == 0x8000000F);
		QVMClearVariant(&value);
	}
	Check(root.pObject->GetProperty(NULL,RSTRING_CAST(L"Font"),&value));
	Check(root.pObject->SetProperty(NULL,RSTRING_CAST(L"Font"),&value));
	QVMClearVariant(&value);
	Check(root.pObject->GetProperty(NULL,RSTRING_CAST(L"OnDestroy"),&value));
	REQUIRE(value.eType == QuadooVM::String && RStrLen(value.rstrVal) == 0);
	QVMClearVariant(&value);
	value.eType = QuadooVM::String;
	value.rstrVal = RSTRING_CAST(L"Destroyed");
	Check(root.pObject->SetProperty(NULL,RSTRING_CAST(L"OnDestroy"),&value));
	value.rstrVal = RSTRING_CAST(L"");
	Check(root.pObject->SetProperty(NULL,RSTRING_CAST(L"OnDestroy"),&value));
	value.rstrVal = RSTRING_CAST(L"Destroyed");
	Check(root.pObject->SetProperty(NULL,RSTRING_CAST(L"OnDestroy"),&value));
	value.eType = QuadooVM::Null;
	Check(root.pObject->GetProperty(NULL,RSTRING_CAST(L"ModalResult"),&value));
	REQUIRE(value.eType == QuadooVM::I8 && value.llVal == 4);
	QVMClearVariant(&value);
	{
		QuadooVM::QVARIANT list = {QuadooVM::Null}, items = {QuadooVM::Null}, snapshot = {QuadooVM::Null};
		QuadooVM::QVARIANT replacement = {QuadooVM::String};
		TStackRef<IQuadooArray> array, snapshotArray;
		replacement.rstrVal = RSTRING_CAST(L"Changed");
		Check(script.pObject->GetProperty(NULL,RSTRING_CAST(L"List1"),&list));
		Check(list.pObject->GetProperty(NULL,RSTRING_CAST(L"Items"),&items));
		REQUIRE(items.eType == QuadooVM::Object);
		Check(items.pObject->QueryInterface(&array));
		REQUIRE(array->Length() == 2);
		Check(array->SetItem(0,&replacement));
		Check(list.pObject->GetProperty(NULL,RSTRING_CAST(L"Items"),&snapshot));
		Check(snapshot.pObject->QueryInterface(&snapshotArray));
		Check(snapshotArray->GetItem(0,&value));
		REQUIRE(value.eType == QuadooVM::String && !lstrcmpW(RStrToWide(value.rstrVal),L"First"));
		QVMClearVariant(&value);
		QVMClearVariant(&snapshot);
		snapshotArray.Release();
		Check(list.pObject->SetProperty(NULL,RSTRING_CAST(L"Items"),&items));
		Check(list.pObject->GetProperty(NULL,RSTRING_CAST(L"Items"),&snapshot));
		Check(snapshot.pObject->QueryInterface(&snapshotArray));
		Check(snapshotArray->GetItem(0,&value));
		REQUIRE(value.eType == QuadooVM::String && !lstrcmpW(RStrToWide(value.rstrVal),L"Changed"));
		QVMClearVariant(&value);
		QVMClearVariant(&snapshot);
		QVMClearVariant(&items);
		QVMClearVariant(&list);
	}
	{
		QuadooVM::QVARIANT meter = {QuadooVM::Null};
		TStackRef<IQuadooComponent> native;
		TStackRef<IExternalMeter> externalMeter;
		UINT arranged, dpi;
		INT accepted;
		Check(script.pObject->GetProperty(NULL,RSTRING_CAST(L"Meter1"),&meter));
		Check(meter.pObject->GetProperty(NULL,RSTRING_CAST(L"Level"),&value));
		REQUIRE(value.eType == QuadooVM::I4 && value.lVal == 23);
		QVMClearVariant(&value);
		value.eType = QuadooVM::I4;
		value.lVal = 31;
		Check(meter.pObject->SetProperty(NULL,RSTRING_CAST(L"Level"),&value));
		Check(host->Runtime()->Unwrap(meter.pObject,&native));
		Check(native->QueryInterface(&externalMeter));
		Check(externalMeter->GetArrangeCount(&arranged,&dpi));
		REQUIRE(arranged > 0 && dpi >= 48);
		Check(externalMeter->Pulse(10,&accepted));
		REQUIRE(accepted == 17);
		QVMClearVariant(&meter);
		QVMClearVariant(&value);
		Check(Call(vm,L"Pulses",&value));
		REQUIRE(value.eType == QuadooVM::I4 && value.lVal == 2);
		QVMClearVariant(&value);
	}
	Check(script.pObject->GetProperty(NULL,RSTRING_CAST(L"Button1"),&button));
	Check(button.pObject->GetProperty(NULL,RSTRING_CAST(L"Caption"),&value));
	REQUIRE(value.eType == QuadooVM::String && !lstrcmpW(RStrToWide(value.rstrVal),L"Bound before OnCreate"));
	QVMClearVariant(&value);
	{
		QuadooVM::QVARIANT replacement = {QuadooVM::I4};
		replacement.lVal = 1234;
		Check(script.pObject->SetProperty(NULL,RSTRING_CAST(L"DirectPlugin"),&replacement));
	}
	if(scenario == 2)
	{
		QuadooVM::QVARIANT failureForm = {QuadooVM::Null};
		QuadooVM::QVARIANT list = {QuadooVM::Null};
		QuadooVM::QVARIANT closed = {QuadooVM::Null};
		TStackRef<IQuadooComponent> component;
		TStackRef<IQuadooWinControl> window;
		HWND native;
		WCHAR buffer[16] = L"unsafe";
		QuadooVM::QVPARAMS empty = {0};
		Check(Call(vm,L"VirtualFailureForm",&failureForm));
		HRESULT failure = failureForm.eType == QuadooVM::Object ? S_OK : E_FAIL;
		if(SUCCEEDED(failure))
		{
			failure = failureForm.pObject->GetProperty(NULL,RSTRING_CAST(L"List1"),&list);
		}
		if(SUCCEEDED(failure))
		{
			failure = host->Runtime()->Unwrap(list.pObject,&component);
		}
		if(SUCCEEDED(failure))
		{
			failure = component->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&window));
		}
		if(SUCCEEDED(failure))
		{
			failure = window->GetHandle(&native);
		}
		if(SUCCEEDED(failure))
		{
			ListView_GetItemText(native,1,0,buffer,ARRAYSIZE(buffer));
			failure = !buffer[0] && SUCCEEDED(host->Runtime()->Error()) && RStrLen(host->Runtime()->Diagnostic()) &&
				wcsstr(RStrToWide(host->Runtime()->Diagnostic()),L"Deliberate virtual provider failure") ? S_OK : E_FAIL;
			failureForm.pObject->Invoke(NULL,RSTRING_CAST(L"Close"),&empty,&closed);
		}
		QVMClearVariant(&closed);
		QVMClearVariant(&list);
		QVMClearVariant(&failureForm);
		Check(failure);
	}
	if(scenario == 2)
	{
		QuadooVM::QVARIANT arguments[2] = {{QuadooVM::I4},{QuadooVM::Object}};
		Check(Call(vm,L"ArmTimerThrow",&value));
		arguments[0].lVal = SW_SHOW;
		arguments[1].pObject = script.pObject;
		parameters.cArgs = ARRAYSIZE(arguments);
		parameters.pqvArgs = arguments;
		Check(host->Invoke(NULL,RSTRING_CAST(L"ShowForm"),&parameters,&value));
		parameters.cArgs = 0;
		parameters.pqvArgs = NULL;
		REQUIRE(FAILED(host->Invoke(NULL,RSTRING_CAST(L"RunApplication"),&parameters,&value)));
		REQUIRE(FAILED(host->Runtime()->Error()));
		REQUIRE(wcsstr(RStrToWide(host->Runtime()->Diagnostic()),L"Deliberate callback failure") != NULL);
		hr = S_OK;
		goto Cleanup;
	}
	Check(button.pObject->Invoke(NULL,RSTRING_CAST(L"Click"),&parameters,&value));
	Check(Call(vm,L"Counts",&value));
	REQUIRE(value.eType == QuadooVM::I4 && value.lVal == 11);
	QVMClearVariant(&value);
	Check(Call(vm,L"SavedEventName",&value));
	REQUIRE(value.eType == QuadooVM::String && !lstrcmpW(RStrToWide(value.rstrVal),L"OnClick"));
	QVMClearVariant(&value);
	{
		MSG key = {0};
		BOOL handled;
		TStackRef<IQuadooComponent> native;
		TStackRef<IQuadooWinControl> buttonWindow;
		Check(form->ShowForm(SW_SHOWNORMAL));
		Check(host->Runtime()->Unwrap(button.pObject,&native));
		Check(native->QueryInterface(&buttonWindow));
		Check(buttonWindow->GetHandle(&key.hwnd));
		SetFocus(key.hwnd);
		key.message = WM_KEYDOWN;
		key.wParam = VK_RETURN;
		Check(host->Runtime()->Preprocess(&key,&handled));
		REQUIRE(handled);
		Check(Call(vm,L"Counts",&value));
		REQUIRE(value.eType == QuadooVM::I4 && value.lVal == 21);
		QVMClearVariant(&value);
	}
	REQUIRE(form->Close() == S_FALSE && IsWindow(handle));
	{
		QuadooVM::QVARIANT saved = {QuadooVM::Null}, cancel = {QuadooVM::Bool};
		Check(Call(vm,L"SavedArguments",&saved));
		Check(saved.pObject->GetProperty(NULL,RSTRING_CAST(L"Cancel"),&value));
		REQUIRE(value.eType == QuadooVM::Bool && value.fVal);
		REQUIRE(saved.pObject->SetProperty(NULL,RSTRING_CAST(L"Cancel"),&cancel) == QuadooForm::EInvalidState);
		QVMClearVariant(&saved);
		QVMClearVariant(&value);
	}
	Check(script.pObject->GetProperty(NULL,RSTRING_CAST(L"Panel1"),&owner));
	{
		ULONG ordinal;
		DWORD count;
		Check(vm->FindFunction(L"MakeFrames",&ordinal,&count));
		Check(vm->PushValue(&owner));
		Check(vm->RunFunction(ordinal,&frame));
		Stage("Frames created.\r\n");
	}
	Check(host->Runtime()->Unwrap(owner.pObject,&frameComponent));
	Check(frameComponent->QueryInterface(&panel));
	{
		UINT children;
		TStackRef<IQuadooControl> first, second, firstLabel, secondLabel;
		TStackRef<IQuadooControlContainer> a, b;
		Check(panel->GetChildCount(&children));
		REQUIRE(children == 3);
		Check(panel->GetChild(1,&first));
		Check(panel->GetChild(2,&second));
		Check(first->QueryInterface(&a));
		Check(second->QueryInterface(&b));
		Check(a->FindControl(RSTRING_CAST(L"Label1"),&firstLabel));
		Check(b->FindControl(RSTRING_CAST(L"Label1"),&secondLabel));
		REQUIRE(!AQVM::SameIdentity(firstLabel,secondLabel));
		Check(first->Destroy());
		Check(panel->GetChildCount(&children));
		REQUIRE(children == 2);
	}
	{
		QuadooVM::QVARIANT arguments[2] = {{QuadooVM::I4},{QuadooVM::Object}};
		arguments[0].lVal = SW_SHOW;
		arguments[1].pObject = script.pObject;
		parameters.cArgs = ARRAYSIZE(arguments);
		parameters.pqvArgs = arguments;
		Check(host->Invoke(NULL,RSTRING_CAST(L"ShowForm"),&parameters,&value));
		parameters.cArgs = 0;
		parameters.pqvArgs = NULL;
		Check(host->Invoke(NULL,RSTRING_CAST(L"RunApplication"),&parameters,&value));
		Stage("Application pump exited.\r\n");
		REQUIRE((value.eType == QuadooVM::I4 && value.lVal == 0) || (value.eType == QuadooVM::I8 && value.llVal == 0));
	}
	REQUIRE(!IsWindow(handle));
	QVMClearVariant(&value);
	Check(Call(vm,L"Counts",&value));
	REQUIRE(value.eType == QuadooVM::I4 && value.lVal >= 211);
	QVMClearVariant(&value);
	REQUIRE(button.pObject->GetProperty(NULL,RSTRING_CAST(L"Caption"),&value) == QuadooForm::EDestroyed);
	REQUIRE(SUCCEEDED(host->Runtime()->Error()));
	Check(Call(vm,L"DestroyedCount",&value));
	REQUIRE(value.eType == QuadooVM::I4 && value.lVal == 1);
	QVMClearVariant(&value);
	host->ShutdownApplication();
	Check(Call(vm,L"PluginCounts",&value));
	REQUIRE(value.eType == QuadooVM::I4 && value.lVal == 11);
	QVMClearVariant(&value);
	Check(script.pObject->GetProperty(NULL,RSTRING_CAST(L"DirectPlugin"),&value));
	REQUIRE(value.eType == QuadooVM::I4 && value.lVal == 1234);
	QVMClearVariant(&value);
	Check(script.pObject->GetProperty(NULL,RSTRING_CAST(L"FallbackPlugin"),&value));
	REQUIRE(value.eType == QuadooVM::Null);
	QVMClearVariant(&value);
	{
		UINT live;
		Check(lifetime->GetLiveCount(&live));
		REQUIRE(live == 0);
	}
Cleanup:
	if(host)
	{
		host->ShutdownApplication();
	}
	QVMClearVariant(&script);
	QVMClearVariant(&root);
	QVMClearVariant(&button);
	QVMClearVariant(&value);
	QVMClearVariant(&owner);
	QVMClearVariant(&frame);
	QVMClearVariant(&deferred);
	exception.Release();
	if(vm)
	{
		vm->Unload();
	}
	if(host)
	{
		host->UnloadModules();
	}
	if(registry)
	{
		registry->UnregisterFactory(RSTRING_CAST(L"ExternalMeter"));
	}
	if(SUCCEEDED(hr))
	{
		hr = SpriteResourcesReleased(spriteObjects,spriteBytes);
	}
	return hr;
}

#include "ListViewContractTests.h"
#include "ApplicationLifetimeTests.h"
#include "AsyncTests.h"
#include "NativeHandlerTests.h"
#include "AsyncWaitTests.h"
#include "ParserEOFTests.h"

INT wmain(INT count, WCHAR** arguments)
{
	HRESULT hr;
	SetUnhandledExceptionFilter(TraceCrash);
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
	if(count == 3 && !lstrcmpW(arguments[1],L"--compile-only"))
	{
		CMemoryStream code;
		CCompilerStatus compiler;
		hr = QuadooParseToStream(arguments[2],0,&code,NULL,&compiler);
		CHAR text[128];
		DWORD written;
		INT length = wsprintfA(text,"Non-windowed script compilation: %u bytes; result 0x%.8x\r\n",code.DataRemaining(),hr);
		WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),text,length,&written,NULL);
		return FAILED(hr) ? 1 : 0;
	}
#ifdef _DEBUG
	_SetAssertCallback(TraceCheck);
	_CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
	_RTC_SetErrorFunc(TraceRuntime);
#endif
	if(count == 2 && !lstrcmpW(arguments[1],L"--parser-eof-only"))
	{
		hr = ParserEOFTests();
		CHAR text[160];
		wsprintfA(text,"Parser EOF: %u assertions; result 0x%.8x\r\n",s_checks,hr);
		Stage(text);
		return FAILED(hr) ? 1 : 0;
	}
	if(count == 2 && !lstrcmpW(arguments[1],L"--asyncwait-state-only"))
	{
		hr = NativeWaitStateTests();
		CHAR text[160];
		wsprintfA(text,"AsyncWait non-windowed: %u assertions; result 0x%.8x\r\n",s_checks,hr);
		Stage(text);
		return FAILED(hr) ? 1 : 0;
	}
	if(count == 2 && !lstrcmpW(arguments[1],L"--sprite-headless"))
	{
		hr = SpritePixelTests();
		CHAR report[128];
		wsprintfA(report,"Sprite offscreen: %u assertions; result 0x%.8x\r\n",s_checks,hr);
		Stage(report);
		return FAILED(hr) ? 1 : 0;
	}
	if(count == 2 && !lstrcmpW(arguments[1],L"--sound-headless"))
	{
		hr = SoundTests();
		CHAR report[128];
		wsprintfA(report,"Host sound non-windowed: %u assertions; result 0x%.8x\r\n",s_checks,hr);
		Stage(report);
		return FAILED(hr) ? 1 : 0;
	}
	INITCOMMONCONTROLSEX controls = {sizeof(controls),ICC_WIN95_CLASSES};
	DPI::SetDPIAware();
	DPI::Initialize();
	InitCommonControlsEx(&controls);
	OleInitialize(NULL);
	if(count == 2 && !lstrcmpW(arguments[1],L"--datetime-picker-only"))
	{
		hr = DateTimePickerScriptTests();
	}
	else if(count == 3 && (!lstrcmpW(arguments[1],L"--canvas-only") || !lstrcmpW(arguments[1],L"--sprite-only")))
	{
		hr = TestRuntime(GetModuleHandleW(NULL),arguments[2],0,FALSE,TRUE);
	}
	else if(count == 2 && !lstrcmpW(arguments[1],L"--native-handler-only"))
	{
		hr = NativeHandlerTests();
	}
	else if(count == 3 && !lstrcmpW(arguments[1],L"--asyncwait-only"))
	{
		hr = AsyncWaitTests(arguments[2]);
	}
	else if(count == 3 && !lstrcmpW(arguments[1],L"--async-only"))
	{
		hr = AsyncTests(arguments[2]);
	}
	else if(count == 3 && !lstrcmpW(arguments[1],L"--lifetime-only"))
	{
		hr = ApplicationLifetimeTests(arguments[2]);
	}
	else if(count == 3 && !lstrcmpW(arguments[1],L"--listview-contract-only"))
	{
		hr = ListContractProbe::Test(arguments[2]);
	}
	else if(count != 2 && !(count == 3 && !lstrcmpW(arguments[1],L"--tree-only")))
	{
		hr = E_INVALIDARG;
	}
	else
	{
		if(count == 3)
		{
			hr = TestRuntime(GetModuleHandleW(NULL),arguments[2],0,TRUE);
		}
		else
		{
			hr = TestRuntime(GetModuleHandleW(NULL),arguments[1]);
		}
		if(SUCCEEDED(hr) && count == 2)
		{
			hr = TestRuntime(GetModuleHandleW(NULL),arguments[1],1);
		}
		if(SUCCEEDED(hr) && count == 2)
		{
			hr = TestRuntime(GetModuleHandleW(NULL),arguments[1],2);
		}
		if(SUCCEEDED(hr))
		{
			hr = UnloadExternalExtension();
		}
	}
	CHAR summary[160];
	DWORD written;
	INT length = wsprintfA(summary,"Application runtime: %u assertions; result 0x%.8x\r\n",s_checks,hr);
	WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),summary,length,&written,NULL);
	OleUninitialize();
	return FAILED(hr) ? 1 : 0;
}
