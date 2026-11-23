#include "..\..\..\..\desktop\QuadooStudio\QuadooStudio\DebugSession.h"

class CApplicationDebugTestStatus : public IStartDebuggerStatus
{
public:
	virtual VOID ReportStatus(PCWSTR, INT)
	{
	}

	virtual BOOL CheckAbortFlag()
	{
		return FALSE;
	}
};

static HRESULT DebugTestPort(INT* port)
{
	HRESULT hr;
	sockaddr_in address = {0};
	INT length = sizeof(address);
	SOCKET listener = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	CheckIf(listener == INVALID_SOCKET,HRESULT_FROM_WIN32(WSAGetLastError()));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	CheckIf(bind(listener,reinterpret_cast<sockaddr*>(&address),sizeof(address)) == SOCKET_ERROR,HRESULT_FROM_WIN32(WSAGetLastError()));
	CheckIf(getsockname(listener,reinterpret_cast<sockaddr*>(&address),&length) == SOCKET_ERROR,HRESULT_FROM_WIN32(WSAGetLastError()));
	*port = ntohs(address.sin_port);
	hr = S_OK;
Cleanup:
	if(listener != INVALID_SOCKET)
		closesocket(listener);
	return hr;
}

static DWORD DebugTestLine(PCWSTR source, PCWSTR marker)
{
	PCWSTR found = TStrStr(source,marker);
	DWORD line = found ? 1 : 0;
	for(PCWSTR at = source; found && at < found; at++)
	{
		if(*at == L'\n')
			line++;
	}
	return line;
}

static HRESULT WaitDebugTestEvent(CQuadooDebugSession& debugger, HWND window, BOOL pause)
{
	HRESULT hr;
	DWORD started = GetTickCount();
	for(;;)
	{
		MSG message;
		while(PeekMessageW(&message,window,WM_QUADOO_DEBUG_SOCKET,WM_QUADOO_DEBUG_SOCKET,PM_REMOVE))
		{
			hr = debugger.OnSocketEvent(message.wParam,message.lParam);
			if(FAILED(hr) && debugger.ApplicationError())
				Log(RStrToWide(debugger.ApplicationError()));
			if(!pause && !debugger.IsActive() && !debugger.ApplicationError())
			{
				hr = S_OK;
				goto Cleanup;
			}
			Check(hr);
		}
		if(pause && debugger.IsPaused() && debugger.GetStackFrameCount())
			break;
		CheckIf(GetTickCount() - started >= 12000,HRESULT_FROM_WIN32(ERROR_TIMEOUT));
		Sleep(10);
	}
	hr = S_OK;
Cleanup:
	return hr;
}

static HRESULT ExpectDebugTestLocation(CQuadooDebugSession& debugger, RSTRING source, DWORD line)
{
	HRESULT hr;
	CRString file;
	DWORD actual;
	BOOL exception;
	Check(debugger.GetCurrentLocation(&file,&actual,&exception));
	REQUIRE(!exception && actual == line && !lstrcmpiW(file.c_wstr(),RStrToWide(source)));
Cleanup:
	return hr;
}

class CProofVariable : public CBaseUnknown, public IQuadooEnumVariables
{
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooEnumVariables)
	END_UNK_MAP

	QUADOO_DEBUG_VARIABLE_QUERY query;

	CProofVariable()
	{
		ZeroMemory(&query,sizeof(query));
		query.requestId = 1;
	}

	STDMETHOD(EnumCallback)(const SCOPE_PARENT&, RSTRING name, PCWSTR, BOOL isStatic, LONG offset, BOOL* proceed)
	{
		if(!isStatic && !lstrcmpW(RStrToWide(name),L"proof"))
		{
			query.nKind = QDVC_LOCAL;
			query.nOffset = offset;
			*proceed = FALSE;
		}
		return S_OK;
	}
};

static HRESULT ExpectDebugTestVariable(CQuadooDebugSession& debugger, HWND window, IQuadooDebugTree* tree, RSTRING source, DWORD line)
{
	HRESULT hr;
	TStackRef<CProofVariable> variable;
	DWORD request, start = GetTickCount();
	HRESULT valueResult;
	CRString value;
	variable.Attach(__new CProofVariable);
	CheckAlloc(variable);
	Check(tree->EnumVariables(source,line,variable));
	REQUIRE(variable->query.nKind == QDVC_LOCAL);
	Check(debugger.SendVariableQuery(variable->query));
	for(;;)
	{
		MSG message;
		while(PeekMessageW(&message,window,WM_QUADOO_DEBUG_SOCKET,WM_QUADOO_DEBUG_SOCKET,PM_REMOVE))
		{
			Check(debugger.OnSocketEvent(message.wParam,message.lParam));
		}
		Check(debugger.GetVariableValue(&request,&valueResult,&value));
		if(hr == S_OK)
			break;
		CheckIf(GetTickCount() - start >= 5000,HRESULT_FROM_WIN32(ERROR_TIMEOUT));
		Sleep(10);
	}
	REQUIRE(request == variable->query.requestId && SUCCEEDED(valueResult) && value == RSTRING_CAST(L"123"));
Cleanup:
	return hr;
}

static HRESULT StartManualDebugTest(PCWSTR debugger, PCWSTR directory, INT port, PROCESS_INFORMATION* process, BOOL loopback = FALSE)
{
	HRESULT hr;
	WCHAR command[MAX_PATH * 2];
	STARTUPINFOW startup = {sizeof(startup)};
	Check(Formatting::TPrintF(command,ARRAYSIZE(command),NULL,L"\"%ls\" -debug-port %d%ls",debugger,port,loopback ? L" -loopback" : L""));
	CheckIfGetLastError(!CreateProcessW(debugger,command,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,directory,&startup,process));
Cleanup:
	return hr;
}

static DWORD DebugTestNetworkAddress()
{
	CHAR host[256];
	if(gethostname(host,ARRAYSIZE(host)) == 0)
	{
		hostent* addresses = gethostbyname(host);
		if(addresses && addresses->h_addrtype == AF_INET && addresses->h_length == sizeof(DWORD))
		{
			for(CHAR** next = addresses->h_addr_list; *next; next++)
			{
				DWORD address;
				CopyMemory(&address,*next,sizeof(address));
				if((ntohl(address) >> 24) != 127 && address)
					return address;
			}
		}
	}
	return htonl(INADDR_LOOPBACK);
}

static HRESULT DebugTestConnect(DWORD address, INT port, UINT attempts, SOCKET* connected)
{
	HRESULT hr = HRESULT_FROM_WIN32(WSAECONNREFUSED);
	sockaddr_in target = {0};
	SOCKET client = INVALID_SOCKET;
	*connected = INVALID_SOCKET;
	target.sin_family = AF_INET;
	target.sin_port = htons(static_cast<u_short>(port));
	target.sin_addr.s_addr = address;
	for(UINT n = 0; n < attempts; n++)
	{
		client = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
		CheckIf(client == INVALID_SOCKET,HRESULT_FROM_WIN32(WSAGetLastError()));
		if(connect(client,reinterpret_cast<sockaddr*>(&target),sizeof(target)) == 0)
		{
			*connected = client;
			client = INVALID_SOCKET;
			hr = S_OK;
			break;
		}
		hr = HRESULT_FROM_WIN32(WSAGetLastError());
		closesocket(client);
		client = INVALID_SOCKET;
		Sleep(25);
	}
Cleanup:
	if(client != INVALID_SOCKET)
		closesocket(client);
	return hr;
}

static HRESULT DebugTestTransfer(SOCKET client, PVOID buffer, DWORD bytes, BOOL transmit)
{
	HRESULT hr = S_OK;
	PCHAR data = static_cast<PCHAR>(buffer);
	while(bytes)
	{
		INT result;
		if(transmit)
		{
			// Split the header and hello into individual writes.
			result = send(client,data,1,0);
		}
		else
		{
			result = recv(client,data,bytes,0);
		}
		CheckIf(result == SOCKET_ERROR,HRESULT_FROM_WIN32(WSAGetLastError()));
		CheckIf(!result,HRESULT_FROM_WIN32(ERROR_CONNECTION_ABORTED));
		bytes -= result;
		data += result;
	}
Cleanup:
	return hr;
}

static HRESULT DebugTestWireStartup(PCWSTR host, PCWSTR directory, DWORD address, UINT mode)
{
	HRESULT hr;
	INT port;
	PROCESS_INFORMATION process = {0};
	SOCKET client = INVALID_SOCKET;
	DWORD timeout = 4000, exitCode;
	QUADOO_DEBUG_MESSAGE_HEADER header = {QDM_HELLO,sizeof(QUADOO_APP_DEBUG_HELLO)};
	const DWORD bits = sizeof(void*) * 8;
	QUADOO_APP_DEBUG_HELLO hello = {QUADOO_DEBUG_PROTOCOL_VERSION,QUADOO_APP_DEBUG_REVISION,mode == 0 ? 96 - bits : bits};
	QUADOO_APP_DEBUG_HELLO reply;
	QUADOO_APP_DEBUG_STATUS status;

	Check(DebugTestPort(&port));
	Check(StartManualDebugTest(host,directory,port,&process,mode == 2));
	if(mode == 2 && address != htonl(INADDR_LOOPBACK))
	{
		hr = DebugTestConnect(address,port,8,&client);
		REQUIRE(FAILED(hr) && client == INVALID_SOCKET);
	}
	Check(DebugTestConnect(mode == 2 ? htonl(INADDR_LOOPBACK) : address,port,80,&client));
	CheckIf(setsockopt(client,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<PCSTR>(&timeout),sizeof(timeout)) == SOCKET_ERROR,
		HRESULT_FROM_WIN32(WSAGetLastError()));
	Check(DebugTestTransfer(client,&header,sizeof(header),TRUE));
	Check(DebugTestTransfer(client,&hello,sizeof(hello),TRUE));
	Check(DebugTestTransfer(client,&header,sizeof(header),FALSE));
	REQUIRE(header.nMessage == QDM_HELLO && header.cbPayload == sizeof(reply));
	Check(DebugTestTransfer(client,&reply,sizeof(reply),FALSE));
	REQUIRE(reply.nProtocol == hello.nProtocol && reply.nApplication == hello.nApplication && reply.nBits == bits);
	if(mode == 1)
	{
		header.nMessage = QDM_SCRIPT_PATH;
		header.cbPayload = QUADOO_DEBUG_MAX_MESSAGE + 1;
		Check(DebugTestTransfer(client,&header,sizeof(header),TRUE));
		Check(DebugTestTransfer(client,&header,sizeof(header),FALSE));
		REQUIRE(header.nMessage == QDM_APP_STATUS && header.cbPayload == sizeof(status));
		Check(DebugTestTransfer(client,&status,sizeof(status),FALSE));
		REQUIRE(FAILED(status.hr) && status.wzDetail[0]);
	}
	else if(mode == 2)
	{
		header.nMessage = QDM_SCRIPT_PATH;
		header.cbPayload = 32;
		Check(DebugTestTransfer(client,&header,sizeof(header),TRUE));
		shutdown(client,SD_SEND);
	}
	REQUIRE(WaitForSingleObject(process.hProcess,5000) == WAIT_OBJECT_0);
	CheckIfGetLastError(!GetExitCodeProcess(process.hProcess,&exitCode));
	REQUIRE(FAILED(static_cast<HRESULT>(exitCode)));
Cleanup:
	if(client != INVALID_SOCKET)
		closesocket(client);
	if(process.hProcess && WaitForSingleObject(process.hProcess,0) == WAIT_TIMEOUT)
	{
		TerminateProcess(process.hProcess,1);
		WaitForSingleObject(process.hProcess,2000);
	}
	SafeCloseHandle(process.hProcess);
	SafeCloseHandle(process.hThread);
	return hr;
}

static HRESULT DebugTestRejectedBundle(IJSONObject* project, RSTRING projectPath, PCWSTR host, PCWSTR directory)
{
	HRESULT hr;
	CApplicationDebugTestStatus status;
	StudioApplication::CApplicationDebugData snapshot;
	CMemoryStream code, debug, breakpoints;
	TStackRef<IQuadooDebugTree> tree;
	CRString diagnostic;
	DWORD count = 0, exitCode, first, second;
	ULONG written;
	PROCESS_INFORMATION process = {0};
	INT port;
	Check(StudioApplication::CompileDebug(project,projectPath,snapshot,code,debug,&tree,diagnostic));
	Check(breakpoints.TWrite(&count,1,&written));
	const BYTE* records = snapshot.bundle.GetReadPtr();
	CopyMemory(&first,records,sizeof(first));
	CopyMemory(&second,records + sizeof(DWORD) + first,sizeof(second));
	DWORD pair = 2 * sizeof(DWORD) + first + second;
	for(UINT mode = 0; mode < 2; mode++)
	{
		CMemoryStream invalid;
		CQuadooDebugSession remote(FALSE);
		Check(invalid.Write(records,pair,&written));
		if(mode)
			Check(invalid.Write(records + sizeof(DWORD) + first,sizeof(DWORD) + second,&written));
		Check(remote.Initialize());
		Check(DebugTestPort(&port));
		Check(StartManualDebugTest(host,directory,port,&process));
		diagnostic = static_cast<RSTRING>(NULL);
		hr = remote.Start(NULL,NULL,RSTRING_CAST(L"127.0.0.1"),port,snapshot.program.c_wstr(),NULL,
			&code,&debug,&breakpoints,&status,&invalid,&diagnostic);
		REQUIRE(FAILED(hr) && diagnostic.Length());
		REQUIRE(WaitForSingleObject(process.hProcess,5000) == WAIT_OBJECT_0);
		CheckIfGetLastError(!GetExitCodeProcess(process.hProcess,&exitCode));
		REQUIRE(FAILED(static_cast<HRESULT>(exitCode)));
		SafeCloseHandle(process.hProcess);
		SafeCloseHandle(process.hThread);
	}
	hr = S_OK;
Cleanup:
	if(process.hProcess && WaitForSingleObject(process.hProcess,0) == WAIT_TIMEOUT)
	{
		TerminateProcess(process.hProcess,1);
		WaitForSingleObject(process.hProcess,2000);
	}
	SafeCloseHandle(process.hProcess);
	SafeCloseHandle(process.hThread);
	return hr;
}

static HRESULT LegacyDebuggerSmoke(RSTRING runtimeDirectory, PCWSTR executable, HWND window)
{
	HRESULT hr;
	static const WCHAR source[] = L"function main()\n{\n\treturn 0;\n}\n";
	static const WCHAR program[] = L"C:\\LegacyDebugSmoke.quadoo";
	CMemoryStream code, debug, breakpoints;
	CSourceCompilerStatus compiler;
	TStackRef<IQuadooDebugTree> tree;
	CApplicationDebugTestStatus status;
	CQuadooDebugSession session(FALSE);
	PROCESS_INFORMATION process = {0};
	CRString host;
	INT port;
	DWORD count = 1;
	ULONG written;
	QUADOO_DEBUG_BREAKPOINT point = {StaticLength(program),3};

	Check(RStrFormatW(&host,L"%r\\%ls",runtimeDirectory,executable));
	Check(QuadooParseTextToStream(SLP(program),SLP(source),QUADOO_COMPILE_LINE_NUMBER_MAP | QUADOO_COMPILE_FOR_DEBUGGING,
		&code,&debug,&compiler,&tree));
	Check(breakpoints.TWrite(&count,1,&written));
	Check(breakpoints.TWrite(&point,1,&written));
	Check(breakpoints.TWrite(SLP(program),&written));
	Check(session.Initialize());
	Check(DebugTestPort(&port));
	Check(StartManualDebugTest(host.c_wstr(),RStrToWide(runtimeDirectory),port,&process,TRUE));
	Check(session.Start(NULL,NULL,RSTRING_CAST(L"127.0.0.1"),port,program,RSTRING_CAST(program),&code,&debug,&breakpoints,&status));
	Check(session.AssociateSocket(window));
	Check(WaitDebugTestEvent(session,window,TRUE));
	Check(ExpectDebugTestLocation(session,RSTRING_CAST(program),point.nLine));
	Check(session.SendCommand(QDM_CONTINUE));
	Check(WaitDebugTestEvent(session,window,FALSE));
	REQUIRE(WaitForSingleObject(process.hProcess,5000) == WAIT_OBJECT_0);
Cleanup:
	session.Stop();
	if(process.hProcess && WaitForSingleObject(process.hProcess,0) == WAIT_TIMEOUT)
	{
		TerminateProcess(process.hProcess,1);
		WaitForSingleObject(process.hProcess,2000);
	}
	SafeCloseHandle(process.hProcess);
	SafeCloseHandle(process.hThread);
	return hr;
}

static HRESULT WaitForStudioDebugCommand(CQuadooProject* project, UINT command, BOOL enabled)
{
	HRESULT hr;
	DWORD started = GetTickCount();
	while(Enabled(project,command) != enabled)
	{
		Pump();
		CheckIf(GetTickCount() - started >= 12000,HRESULT_FROM_WIN32(ERROR_TIMEOUT));
		Sleep(10);
	}
	hr = S_OK;
Cleanup:
	return hr;
}

static HRESULT StudioDebugSession(IJSONObject* project, PCWSTR projectPath, PCWSTR directory, INT port, DWORD line)
{
	HRESULT hr;
	TStackRef<CQuadooStudio> studio;
	TStackRef<IJSONValue> value;
	CQuadooProject* ide = NULL;
	CProjectFile* file;
	QFD::CDesignerEditor* designer;
	CRString packages;
	TStackRef<IOleWindow> codeWindow;
	HWND code;
	GUID id;

	Check(JSONCreateBool(false,&value));
	Check(project->AddValueW(L"autoStartDebugger",value));
	value.Release();
	Check(JSONCreateInteger(port,&value));
	Check(project->AddValueW(L"debugPort",value));
	Check(QFD::PutString(project,L"remoteHost",RSTRING_CAST(L"127.0.0.1")));
	Check(SaveStudioProject(project,projectPath));
	Check(RStrFormatW(&packages,L"%ls\\Packages",directory));
	studio.Attach(__new CQuadooStudio(GetModuleHandleW(NULL)));
	CheckAlloc(studio);
	Check(studio->Initialize(L"",950,650,SW_SHOWNOACTIVATE,packages.c_wstr()));
	Check(studio->OpenProject(projectPath));
	Pump();
	ide = studio->Project();
	REQUIRE(ide && ide->DesignerActive() && Enabled(ide,ID_DEBUG_SCRIPT));
	Check(ide->FindSource(RSTRING_CAST(L".\\MainForm.quadoo"),&file));
	Check(file->ToggleBreakpoint(line - 1));
	Check(ide->Exec(NULL,ID_DEBUG_SCRIPT,0,NULL,NULL));
	Check(WaitForStudioDebugCommand(ide,ID_STEP_INTO,TRUE));
	REQUIRE(ide->ActiveSourceEditor() && !ide->DesignerActive());
	Check(ide->ActiveSourceEditor()->QueryInterface(&codeWindow));
	Check(codeWindow->GetWindow(&code));
	SendMessageW(code,WM_CHAR,L'X',1);
	REQUIRE(!ide->ActiveSourceEditor()->IsModified());
	Check(ide->SetDesignView(TRUE));
	designer = ide->ActiveDesigner();
	REQUIRE(designer && !IsWindowEnabled(designer->ToolsWindow()));
	REQUIRE(designer->AddComponent(RSTRING_CAST(L"Timer"),&id) == E_ACCESSDENIED);
	Check(ide->Exec(NULL,ID_DEBUG_SCRIPT,0,NULL,NULL));
	Check(WaitForStudioDebugCommand(ide,ID_DEBUG_STOP,FALSE));
	REQUIRE(IsWindowEnabled(designer->ToolsWindow()) && Enabled(ide,ID_DEBUG_SCRIPT));
Cleanup:
	if(ide && Enabled(ide,ID_DEBUG_STOP))
		ide->Exec(NULL,ID_DEBUG_STOP,0,NULL,NULL);
	if(studio)
		studio->Destroy();
	return hr;
}

static HRESULT ApplicationDebugAndRun(IJSONObject* project, RSTRING projectPath, RSTRING folder,
	RSTRING runtimeDirectory, RSTRING sourcePath)
{
	HRESULT hr;
	TStackRef<IJSONValue> value;
	TStackRef<IQuadooDebugTree> tree;
	StudioApplication::CApplicationDebugData application;
	CMemoryStream code, debug, breakpoints;
	CApplicationDebugTestStatus status;
	CQuadooDebugSession debugger(TRUE);
	CRString diagnostic, source, host, directory, hostSource, moduleSource, moduleTarget;
	ULONGLONG hostBefore, hostAfter, moduleBefore, moduleAfter;
	HWND window = NULL;
	INT port;
	ULONG written;
	DWORD lines[2];
	DWORD count = ARRAYSIZE(lines);
	PROCESS_INFORMATION manual = {0};
	DWORD address;
	BOOL removedSource = FALSE;
	CRString movedSource;

	Log(L"AQVMDbg socket startup, event breakpoints and deployment...\r\n");
	Check(RStrFormatW(&directory,L"%r\\Debugger",folder));
	CheckIfGetLastError(!CreateDirectoryW(directory.c_wstr(),NULL));
	Check(RStrFormatW(&host,L"%r\\AQVMDbg.exe",static_cast<RSTRING>(directory)));
	Check(RStrFormatW(&hostSource,L"%r\\AQVMDbg.exe",runtimeDirectory));
	CheckIfGetLastError(!CopyFileW(hostSource.c_wstr(),host.c_wstr(),TRUE));
	Check(FileFingerprint(host.c_wstr(),&hostBefore));
	Check(RStrFormatW(&moduleSource,L"%r\\DebugOnlyModule.quadoo",folder));
	Check(RStrFormatW(&moduleTarget,L"%r\\DebugOnlyModule.quadoo",static_cast<RSTRING>(directory)));
	Check(Text::SaveToFile(SLP(L"class DebugModule { function Ping() { return 456; } };\nfunction CreateObject(options) { return new DebugModule; }\n"),moduleSource.c_wstr()));
	{
		TStackRef<IJSONArray> selected, other;
		Check(JSONCreateArray(&selected));
		Check(JSONCreateArray(&other));
		Check(JSONCreateString(moduleSource,&value));
		Check(selected->Add(value));
		value.Release();
		Check(JSONCreateString(RSTRING_CAST(L"MissingOtherArchitecture.qbc"),&value));
		Check(other->Add(value));
		value.Release();
#ifdef _WIN64
		Check(StudioApplication::WrapModuleSettings(other,selected,&value));
#else
		Check(StudioApplication::WrapModuleSettings(selected,other,&value));
#endif
		Check(project->AddValueW(L"modules",value));
		Check(SaveStudioProject(project,RStrToWide(projectPath)));
	}
	Check(StudioApplication::CompileDebug(project,projectPath,application,code,debug,&tree,diagnostic));
	REQUIRE(tree && code.DataRemaining() && debug.DataRemaining());
	Check(application.PrepareDependencies(host.c_wstr(),&status,diagnostic));
	Check(FileFingerprint(moduleTarget.c_wstr(),&moduleBefore));
	CheckIfGetLastError(!DeleteFileW(moduleSource.c_wstr()));
	Check(application.PrepareDependencies(host.c_wstr(),&status,diagnostic));
	Check(FileFingerprint(moduleTarget.c_wstr(),&moduleAfter));
	REQUIRE(moduleBefore == moduleAfter);
	Check(ReadText(RStrToWide(sourcePath),source));
	lines[0] = DebugTestLine(source.c_wstr(),L"Form = Form.Create(this);");
	lines[1] = DebugTestLine(source.c_wstr(),L"Form.Close();");
	REQUIRE(lines[0] && lines[1]);
	Check(breakpoints.TWrite(&count,1,&written));
	for(UINT n = 0; n < ARRAYSIZE(lines); n++)
	{
		QUADOO_DEBUG_BREAKPOINT point = {static_cast<DWORD>(RStrLen(sourcePath)),lines[n]};
		Check(breakpoints.TWrite(&point,1,&written));
		Check(breakpoints.TWrite(RStrToWide(sourcePath),point.cchFile,&written));
	}
	window = CreateWindowExW(0,L"STATIC",NULL,0,0,0,0,0,HWND_MESSAGE,NULL,GetModuleHandleW(NULL),NULL);
	CheckIfGetLastError(!window);
	Check(debugger.Initialize());
	address = DebugTestNetworkAddress();
	if(address == htonl(INADDR_LOOPBACK))
		Log(L"No non-loopback IPv4 address available for the listener check.\r\n");
	for(UINT mode = 0; mode < 3; mode++)
	{
		Check(DebugTestWireStartup(host.c_wstr(),directory.c_wstr(),address,mode));
	}
	Check(DebugTestRejectedBundle(project,projectPath,host.c_wstr(),directory.c_wstr()));
	Check(LegacyDebuggerSmoke(runtimeDirectory,L"QVMDbg.exe",window));
	Check(LegacyDebuggerSmoke(runtimeDirectory,L"WQVMDbg.exe",window));
	Check(DebugTestPort(&port));
	Check(debugger.Start(host.c_wstr(),RStrToWide(folder),NULL,port,application.program.c_wstr(),RSTRING_CAST(L"\"test argument\""),
		&code,&debug,&breakpoints,&status,&application.bundle,&diagnostic));
	Check(debugger.AssociateSocket(window));
	Check(WaitDebugTestEvent(debugger,window,TRUE));
	Check(ExpectDebugTestLocation(debugger,sourcePath,lines[0]));
	Check(debugger.SendCommand(QDM_STEP_OVER));
	Check(WaitDebugTestEvent(debugger,window,TRUE));
	Check(debugger.SendBreakpoint(FALSE,RStrToWide(sourcePath),lines[1]));
	Check(debugger.SendBreakpoint(TRUE,RStrToWide(sourcePath),lines[1]));
	Check(debugger.SendCommand(QDM_CONTINUE));
	Check(WaitDebugTestEvent(debugger,window,TRUE));
	Check(ExpectDebugTestLocation(debugger,sourcePath,lines[1]));
	Check(ExpectDebugTestVariable(debugger,window,tree,sourcePath,lines[1]));
	Check(debugger.SendCommand(QDM_CONTINUE));
	Check(WaitDebugTestEvent(debugger,window,FALSE));
	Check(FileFingerprint(host.c_wstr(),&hostAfter));
	REQUIRE(hostBefore == hostAfter);

	// A manually started host can run the compiled snapshot after the source
	// disappears. Its missing local module source is never consulted.
	{
		StudioApplication::CApplicationDebugData remoteData;
		CQuadooDebugSession remote(FALSE);
		CMemoryStream remoteCode, remoteDebug;
		TStackRef<IQuadooDebugTree> remoteTree;
		Check(StudioApplication::CompileDebug(project,projectPath,remoteData,remoteCode,remoteDebug,&remoteTree,diagnostic));
		Check(RStrFormatW(&movedSource,L"%r.hidden",sourcePath));
		CheckIfGetLastError(!MoveFileW(RStrToWide(sourcePath),movedSource.c_wstr()));
		removedSource = TRUE;
		Check(remote.Initialize());
		Check(DebugTestPort(&port));
		Check(StartManualDebugTest(host.c_wstr(),directory.c_wstr(),port,&manual));
		Check(remote.Start(NULL,NULL,RSTRING_CAST(L"127.0.0.1"),port,remoteData.program.c_wstr(),NULL,
			&remoteCode,&remoteDebug,&breakpoints,&status,&remoteData.bundle,&diagnostic));
		Check(remote.AssociateSocket(window));
		Check(WaitDebugTestEvent(remote,window,TRUE));
		Check(ExpectDebugTestLocation(remote,sourcePath,lines[0]));
		remote.Stop();
		REQUIRE(WaitForSingleObject(manual.hProcess,5000) == WAIT_OBJECT_0);
	}
	REQUIRE(GetFileAttributesW(moduleSource.c_wstr()) == INVALID_FILE_ATTRIBUTES);
	CheckIfGetLastError(!MoveFileW(movedSource.c_wstr(),RStrToWide(sourcePath)));
	removedSource = FALSE;
	SafeCloseHandle(manual.hProcess);
	SafeCloseHandle(manual.hThread);
	debugger.Stop();
	Check(debugger.Initialize());
	Check(DebugTestPort(&port));
	Check(StartManualDebugTest(host.c_wstr(),directory.c_wstr(),port,&manual));
	Check(StudioDebugSession(project,RStrToWide(projectPath),RStrToWide(folder),port,lines[0]));
	REQUIRE(WaitForSingleObject(manual.hProcess,5000) == WAIT_OBJECT_0);
Cleanup:
	debugger.Stop();
	if(manual.hProcess && WaitForSingleObject(manual.hProcess,0) == WAIT_TIMEOUT)
	{
		TerminateProcess(manual.hProcess,1);
		WaitForSingleObject(manual.hProcess,2000);
	}
	SafeCloseHandle(manual.hProcess);
	SafeCloseHandle(manual.hThread);
	if(removedSource)
		MoveFileW(movedSource.c_wstr(),RStrToWide(sourcePath));
	if(window)
		DestroyWindow(window);
	if(FAILED(hr) && diagnostic.Length())
		Log(diagnostic.c_wstr());
	return hr;
}
