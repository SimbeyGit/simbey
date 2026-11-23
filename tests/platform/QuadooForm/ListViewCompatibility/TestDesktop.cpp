#include <windows.h>

// Run UI validation without switching or borrowing the user's input desktop.
INT wmain(INT count, WCHAR** arguments)
{
	HDESK desktop = NULL;
	STARTUPINFOW startup = {sizeof(startup)};
	PROCESS_INFORMATION process = {0};
	WCHAR desktopName[80];
	WCHAR desktopPath[100];
	WCHAR executable[MAX_PATH];
	WCHAR command[32768];
	DWORD result = ERROR_INVALID_PARAMETER;
	DWORD exitCode;
	DWORD length;
	if(count < 2 || count > 4)
	{
		goto Cleanup;
	}
	length = GetFullPathNameW(arguments[1],ARRAYSIZE(executable),executable,NULL);
	if(!length || length >= ARRAYSIZE(executable))
	{
		result = length ? ERROR_FILENAME_EXCED_RANGE : GetLastError();
		goto Cleanup;
	}
	wsprintfW(desktopName,L"QuadooListViewTests_%u",GetCurrentProcessId());
	wsprintfW(desktopPath,L"winsta0\\%s",desktopName);
	desktop = CreateDesktopW(desktopName,NULL,NULL,0,GENERIC_ALL,NULL);
	if(!desktop)
	{
		result = GetLastError();
		goto Cleanup;
	}
	if(lstrlenW(executable) + (count > 2 ? lstrlenW(arguments[2]) : 0) +
		(count > 3 ? lstrlenW(arguments[3]) : 0) > 32000)
	{
		goto Cleanup;
	}
	if(count == 2)
	{
		wsprintfW(command,L"\"%s\"",executable);
	}
	else if(count == 3)
	{
		wsprintfW(command,L"\"%s\" \"%s\"",executable,arguments[2]);
	}
	else
	{
		wsprintfW(command,L"\"%s\" \"%s\" \"%s\"",executable,arguments[2],arguments[3]);
	}
	startup.lpDesktop = desktopPath;
	startup.dwFlags = STARTF_USESTDHANDLES;
	startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
	startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
	SetHandleInformation(startup.hStdOutput,HANDLE_FLAG_INHERIT,HANDLE_FLAG_INHERIT);
	SetHandleInformation(startup.hStdError,HANDLE_FLAG_INHERIT,HANDLE_FLAG_INHERIT);
	if(!CreateProcessW(executable,command,NULL,NULL,TRUE,0,NULL,NULL,&startup,&process))
	{
		result = GetLastError();
		goto Cleanup;
	}
	if(WaitForSingleObject(process.hProcess,900000) != WAIT_OBJECT_0)
	{
		TerminateProcess(process.hProcess,ERROR_TIMEOUT);
		WaitForSingleObject(process.hProcess,10000);
		result = ERROR_TIMEOUT;
		goto Cleanup;
	}
	if(GetExitCodeProcess(process.hProcess,&exitCode))
	{
		result = exitCode;
	}
	else
	{
		result = GetLastError();
	}
Cleanup:
	if(process.hThread)
	{
		CloseHandle(process.hThread);
	}
	if(process.hProcess)
	{
		CloseHandle(process.hProcess);
	}
	if(desktop)
	{
		CloseDesktop(desktop);
	}
	return result;
}
