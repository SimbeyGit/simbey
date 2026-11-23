#include "NativeLifetimeTests.h"

// Included by Tests.cpp; exercises the actual generated startup and real VM.
static HRESULT ScriptApplicationLifetime(HINSTANCE module, PCWSTR scriptPath, INT scenario)
{
	HRESULT hr;
	HRESULT registered;
	TStackRef<CAQVMHost> host;
	TStackRef<IQuadooInstanceLoader> loader;
	TStackRef<IQuadooVM> vm;
	TStackRef<IQuadooObject> exception;
	TStackRef<IJSONObject> catalog;
	CHostedModules modules;
	CMemoryStream byteCode;
	CRString source;
	CRString startup;
	CRString combined;
	QuadooVM::QVARIANT value = {QuadooVM::Null};
	CCompilerStatus compiler;
	ULONG ordinal;
	DWORD parameters;
	DWORD started = GetTickCount();
	PWSTR loaded = NULL;
	INT length;
	static const WCHAR catalogText[] = L"{\"schemaVersion\":2,\"startupClass\":\"LifetimeForm\",\"units\":["
		L"{\"class\":\"LifetimeForm\",\"kind\":\"form\",\"eventsInterface\":\"ILifetimeForm\","
		L"\"unitId\":\"{F6000000-0000-0000-0000-000000000001}\"}]}";
	Check(AQVM::ParseObject(SLP(catalogText), &catalog));
	Check(Text::LoadFromFile(scriptPath, &loaded, &length));
	Check(RStrCreateW(length, loaded, &source));
	Check(AQVM::GenerateStartup(RSTRING_CAST(L"LifetimeForm"), &startup));
	Check(RStrFormatW(&combined, L"%r%r", static_cast<RSTRING>(source), static_cast<RSTRING>(startup)));
	Check(QuadooParseTextToStream(scriptPath, TStrLenAssert(scriptPath), combined.c_wstr(), combined.Length(),
		0, &byteCode, NULL, &compiler));
	host.Attach(__new CAQVMHost);
	CheckAlloc(host);
	Check(host->Initialize(&modules, RSTRING_CAST(scriptPath), 0, NULL));
	Check(host->InitializeApplication(module, catalog));
	Check(QVMCreateLoader(NULL, &loader));
	host->AttachLoader(loader);
	Check(loader->AddInstance(RSTRING_CAST(L"LifetimeTests"), NULL, &byteCode, byteCode.DataRemaining(), NULL));
	Check(loader->LoadVM(RSTRING_CAST(L"LifetimeTests"), &registered, &vm));
	Check(vm->AddGlobal(RSTRING_CAST(L"Host"), host));
	Check(vm->AddExternalClassLoader(host));
	Check(vm->RunConstructor(&exception));
	REQUIRE(!exception);
	Check(vm->FindFunction(L"ConfigureLifetime", &ordinal, &parameters));
	value.eType = QuadooVM::I4;
	value.lVal = scenario;
	Check(vm->PushValue(&value));
	Check(vm->RunFunction(ordinal, &value));
	QVMClearVariant(&value);
	Check(host->Runtime()->BeginScriptEntry());
	Check(vm->FindFunction(L"WinMain", &ordinal, &parameters));
	REQUIRE(parameters == 2);
	value.eType = QuadooVM::I4;
	value.lVal = SW_SHOWNOACTIVATE;
	Check(vm->PushValue(&value));
	value.eType = QuadooVM::String;
	value.rstrVal = NULL;
	Check(vm->PushValue(&value));
	value.eType = QuadooVM::Null;
	Check(vm->RunFunction(ordinal, &value));
	host->Runtime()->EndScriptEntry();
	if(scenario == 11)
	{
		QuadooVM::QVARIANT description = {QuadooVM::Null};
		QuadooVM::QVPARAMS empty = {0};
		BOOL failed = value.eType == QuadooVM::Object &&
			SUCCEEDED(value.pObject->Invoke(NULL, RSTRING_CAST(L"ToString"), &empty, &description)) &&
			description.eType == QuadooVM::String &&
			wcsstr(RStrToWide(description.rstrVal), L"Deliberate startup helper failure") != NULL;
		QVMClearVariant(&description);
		REQUIRE(failed && SUCCEEDED(host->Runtime()->Error()));
		BOOL run;
		REQUIRE(host->Runtime()->BeginMessageLoop(&run) == QuadooForm::EInvalidState);
		hr = S_OK;
		goto Cleanup;
	}
	if(scenario == 10)
	{
		REQUIRE(FAILED(host->Runtime()->Error()));
		REQUIRE(wcsstr(RStrToWide(host->Runtime()->Diagnostic()), L"Deliberate destructor failure") != NULL);
	}
	else
	{
		Check(host->Runtime()->Error());
		REQUIRE(value.eType == QuadooVM::I4 && value.lVal == 0);
	}
	REQUIRE(GetTickCount() - started < 10000);
	QVMClearVariant(&value);
	Check(Call(vm, L"LifetimeCounts", &value));
	REQUIRE(value.eType == QuadooVM::I4 && value.lVal == (scenario == 3 || scenario == 7 || scenario == 10 ? 1111 : 2222));
	QVMClearVariant(&value);
	Check(Call(vm, L"LifetimeRejected", &value));
	REQUIRE(value.eType == QuadooVM::I4 && value.lVal == (scenario == 7 ? 1 : 0));
	{
		MSG message;
		REQUIRE(!PeekMessageW(&message, NULL, WM_QUIT, WM_QUIT, PM_REMOVE));
	}
Cleanup:
	SafeDeleteArray(loaded);
	if(host)
	{
		if(FAILED(hr) && host->Runtime() && RStrLen(host->Runtime()->Diagnostic()))
		{
			CHAR diagnostic[2048];
			WideCharToMultiByte(CP_UTF8, 0, RStrToWide(host->Runtime()->Diagnostic()), -1,
				diagnostic, ARRAYSIZE(diagnostic), NULL, NULL);
			Stage(diagnostic);
			Stage("\r\n");
		}
		host->ShutdownApplication();
	}
	QVMClearVariant(&value);
	exception.Release();
	if(vm)
	{
		vm->Unload();
	}
	if(host)
	{
		host->UnloadModules();
	}
	return hr;
}

static HRESULT ApplicationLifetimeTests(PCWSTR path)
{
	HRESULT hr = S_OK;
	Check(NativeApplicationLifetime());
	Check(ExternalApplicationQuit());
	Check(ExternalApplicationQuit(TRUE));
	for(INT scenario = 1; scenario <= 11; scenario++)
	{
		CHAR message[80];
		wsprintfA(message, "Application lifetime scenario %d.\r\n", scenario);
		Stage(message);
		Check(ScriptApplicationLifetime(GetModuleHandleW(NULL), path, scenario));
	}
Cleanup:
	return hr;
}
