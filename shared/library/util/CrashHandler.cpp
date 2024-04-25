#include <windows.h>
#include <Tlhelp32.h>
#include <DbgHelp.h>
#include "Library\Util\Formatting.h"
#include "CrashHandler.h"

// based on dbghelp.h
typedef BOOL (WINAPI* MINIDUMPWRITEDUMP)(HANDLE hProcess, DWORD dwPid, HANDLE hFile, MINIDUMP_TYPE DumpType,
									CONST PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
									CONST PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
									CONST PMINIDUMP_CALLBACK_INFORMATION CallbackParam
									);

typedef struct tagSTACK_FRAME
{
	struct tagSTACK_FRAME* Ebp;		// address of the calling function frame
	LPBYTE Ret_Addr;				// return address
} STACK_FRAME, *LPSTACK_FRAME;

typedef struct tagSTACK_FRAME_PARAM : tagSTACK_FRAME
{
	DWORD Param[1];					// parameter list - could be empty
} STACK_FRAME_PARAM, *LPSTACK_FRAME_PARAM;

inline bool ResolveLogicalAddress (LPCVOID addr, __out_ecount(cchMaxModule) PTSTR ptzModule, UINT cchMaxModule, __out ULONG* lpnSection, __out ULONG* lpnOffset)
{
	// http://axel.plinge.de/dev/060301excepvc6.html

	bool fResolved = false;
	MEMORY_BASIC_INFORMATION mbi = {0};
	if(::VirtualQuery(addr, &mbi, sizeof(mbi)))
	{
		HMODULE hModule = (HMODULE)mbi.AllocationBase;
		if(::GetModuleFileName(hModule, ptzModule, cchMaxModule))
		{
			PIMAGE_DOS_HEADER p_dos_hdr = (PIMAGE_DOS_HEADER)hModule;
			PIMAGE_NT_HEADERS p_nt_hdr = (PIMAGE_NT_HEADERS)((LPBYTE)p_dos_hdr + p_dos_hdr->e_lfanew);
			PIMAGE_SECTION_HEADER p_section = IMAGE_FIRST_SECTION( p_nt_hdr );

			ULONG rva = (ULONG)((LPBYTE)addr - (LPBYTE)hModule);
			for(UINT i = 0; i < p_nt_hdr->FileHeader.NumberOfSections; ++i, ++p_section)
			{
				ULONG section_start = p_section->VirtualAddress;
				ULONG section_end = section_start + max(p_section->SizeOfRawData, p_section->Misc.VirtualSize);
				if((rva >= section_start) && (rva <= section_end))
				{
					*lpnSection = i + 1;
					*lpnOffset = rva - section_start;
					fResolved = true;
					break;
				}
			}
		}
	}
	return fResolved;
}

BOOL WINAPI GetModuleByRetAddr (PBYTE Ret_Addr, __out_ecount(cchMaxModuleName) TCHAR* Module_Name, INT cchMaxModuleName, PBYTE& Module_Addr)
{
	BOOL fSuccess = FALSE;
	MODULEENTRY32 M = { sizeof(M) };
	HANDLE hSnapshot = NULL;

	hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);

	if((hSnapshot != INVALID_HANDLE_VALUE) &&
		Module32First(hSnapshot, &M))
	{
		do
		{
			if(DWORD(Ret_Addr - M.modBaseAddr) < M.modBaseSize)
			{
				lstrcpyn(Module_Name, M.szExePath, cchMaxModuleName);
				Module_Addr = M.modBaseAddr;
				fSuccess = TRUE;
				break;
			}
		} while(Module32Next(hSnapshot, &M));
	}

	CloseHandle(hSnapshot);

	return fSuccess;
}

INT WINAPI WriteCallStack (const EXCEPTION_POINTERS* pcException, HANDLE hFile)
{
	INT nStackDepth = 0;

#ifndef	_WIN64
	// http://www.codeproject.com/KB/applications/minidump.aspx

	TCHAR ModuleName[MAX_PATH];
	PBYTE ModuleAddr;
	const STACK_FRAME* Ebp;

	if(pcException)
		Ebp = (LPSTACK_FRAME)(LONG_PTR)pcException->ContextRecord->Ebp;
	else
	{
		// Frame address of Get_Call_Stack()
		Ebp = (LPSTACK_FRAME)&pcException - 1;
	}

	for(INT Ret_Addr_I = 0;
		(Ret_Addr_I < 20) && !IsBadReadPtr(Ebp, sizeof(LPSTACK_FRAME)) && !IsBadCodePtr(FARPROC(Ebp->Ret_Addr));
		Ret_Addr_I++, Ebp = Ebp->Ebp)
	{
		// Find the module by a return address inside that module
		if(GetModuleByRetAddr(Ebp->Ret_Addr, ModuleName, ARRAYSIZE(ModuleName),ModuleAddr))
		{
			TCHAR Str[1024];
			INT Str_Len = 0;
			DWORD cb;

			// Save module's address and path 
			Str_Len += wsprintf(Str + Str_Len, 
				TEXT("%08X  %s"), 
				ModuleAddr, ModuleName);
			// Save offset of return address
			Str_Len += wsprintf(Str + Str_Len, 
				TEXT("  +%08X"), 
				Ebp->Ret_Addr - ModuleAddr);

			// Save 5 parameters. We don't know the real number of parameters!
			if(!IsBadReadPtr(Ebp, sizeof(LPSTACK_FRAME) + 5 * sizeof(DWORD)))
			{
				const STACK_FRAME_PARAM* Ebpp = (LPSTACK_FRAME_PARAM)Ebp;

				Str_Len += wsprintf(Str + Str_Len, TEXT("  (%X, %X, %X, %X, %X)"),
					Ebpp->Param[0], Ebpp->Param[1], 
					Ebpp->Param[2], Ebpp->Param[3], Ebpp->Param[4]);
			}

			Str_Len += wsprintf(Str + Str_Len, TEXT("\r\n"));

			WriteFile(hFile,Str,Str_Len,&cb,NULL);
		}

		nStackDepth++;
	}
#endif

	return nStackDepth;
}

VOID WINAPI AppendCrashLog (const PEXCEPTION_POINTERS ep, LPCTSTR lpcszLog)
{
	TCHAR szModule[MAX_PATH];
	ULONG nSection, nOffset;
	const EXCEPTION_RECORD* er = ep->ExceptionRecord;

	if(ResolveLogicalAddress(er->ExceptionAddress,szModule,ARRAYSIZE(szModule),&nSection,&nOffset))
	{
		HANDLE hFile = CreateFile(lpcszLog,GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
		if(INVALID_HANDLE_VALUE == hFile)
			hFile = CreateFile(lpcszLog,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
		else
			SetFilePointer(hFile,0,NULL,FILE_END);
		if(INVALID_HANDLE_VALUE != hFile)
		{
			SYSTEMTIME st;
			TCHAR szLog[512];
			INT cchLog;
			DWORD dwWritten;

			GetSystemTime(&st);
			if(SUCCEEDED(Formatting::TPrintF(szLog, ARRAYSIZE(szLog), &cchLog,
				TEXT("CRASH at %d/%d/%d %d:%.2d:%.2d MODULE \"%s\" SECTION %.8x OFFSET %.8x\r\n"),
				st.wMonth, st.wDay, st.wYear,
				st.wHour, st.wMinute, st.wSecond,
				szModule, nSection, nOffset)))
			{
				WriteFile(hFile, szLog, cchLog * sizeof(TCHAR), &dwWritten, NULL);
			}
			WriteCallStack(ep,hFile);

			CloseHandle(hFile);
		}
	}
}

BOOL WINAPI WriteMiniDump (PEXCEPTION_POINTERS ep, LPCTSTR lpcszDump)
{
	BOOL fSuccess = FALSE;
	HMODULE hDll = NULL;
	TCHAR szDbgHelpPath[MAX_PATH];

	if(GetModuleFileName(NULL, szDbgHelpPath, MAX_PATH))
	{
		PTSTR ptzSlash = const_cast<PTSTR>(TStrRChr(szDbgHelpPath, static_cast<TCHAR>('\\')));
		if(ptzSlash)
		{
			ptzSlash++;
			if(SUCCEEDED(TStrCchCpy(ptzSlash, ARRAYSIZE(szDbgHelpPath) - static_cast<INT>(ptzSlash - szDbgHelpPath), TEXT("DBGHELP.DLL"))))
				hDll = ::LoadLibrary(szDbgHelpPath);
		}
	}

	if(NULL == hDll)
	{
		// load any version we can
		hDll = ::LoadLibrary(TEXT("DBGHELP.DLL"));
	}

	if(hDll)
	{
		MINIDUMPWRITEDUMP lpfnDump = (MINIDUMPWRITEDUMP)::GetProcAddress(hDll, "MiniDumpWriteDump");
		if(lpfnDump)
		{
			HANDLE hFile = ::CreateFile(lpcszDump, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if(hFile != INVALID_HANDLE_VALUE)
			{
				_MINIDUMP_EXCEPTION_INFORMATION ExInfo;

				ExInfo.ThreadId = ::GetCurrentThreadId();
				ExInfo.ExceptionPointers = ep;
				ExInfo.ClientPointers = NULL;

				fSuccess = lpfnDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, MiniDumpWithFullMemory, &ExInfo, NULL, NULL);
				::CloseHandle(hFile);
			}
		}

		FreeLibrary(hDll);
	}
	return fSuccess;
}

LONG WINAPI GlobalExceptionFilter (PEXCEPTION_POINTERS ep)
{
	TCHAR tzCrashDir[MAX_PATH];
	INT cchCrashDir;
	if(GetCrashDir(tzCrashDir, ARRAYSIZE(tzCrashDir), &cchCrashDir))
	{
		if(SUCCEEDED(TStrCchCpy(tzCrashDir + cchCrashDir, ARRAYSIZE(tzCrashDir) - cchCrashDir, TEXT("CrashLog.txt"))))
			AppendCrashLog(ep, tzCrashDir);

		if(SUCCEEDED(TStrCchCpy(tzCrashDir + cchCrashDir, ARRAYSIZE(tzCrashDir) - cchCrashDir, TEXT("MiniDump.dmp"))))
			WriteMiniDump(ep, tzCrashDir);
	}
	return EXCEPTION_CONTINUE_SEARCH;
}

VOID SetGlobalCrashHandler (VOID)
{
	SetUnhandledExceptionFilter(GlobalExceptionFilter);
}
