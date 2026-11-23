#pragma once

#include <windows.h>
#include <strsafe.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Core\StringCore.h"

inline HRESULT TestScratchDirectory(PCWSTR suite, PWSTR path, INT capacity)
{
	HRESULT hr;
	WCHAR module[MAX_PATH];
	INT length = GetModuleFileNameW(NULL,module,ARRAYSIZE(module));
	PWSTR separator;

	CheckIf(!length || length >= ARRAYSIZE(module),HRESULT_FROM_WIN32(ERROR_FILENAME_EXCED_RANGE));
	separator = const_cast<PWSTR>(TStrRChr(module,L'\\'));
	while(separator)
	{
		*separator = 0;
		separator = const_cast<PWSTR>(TStrRChr(module,L'\\'));
		if(separator && !lstrcmpiW(separator + 1,L"results"))
			break;
	}
	CheckIf(!separator,HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND));
	Check(StringCchPrintfW(path,capacity,L"%ls\\scratch",module));
	CheckIf(!CreateDirectoryW(path,NULL) && GetLastError() != ERROR_ALREADY_EXISTS,HrEnsureLastError());
	Check(StringCchPrintfW(path,capacity,L"%ls\\scratch\\%ls",module,suite));
	CheckIf(!CreateDirectoryW(path,NULL) && GetLastError() != ERROR_ALREADY_EXISTS,HrEnsureLastError());
	Check(StringCchPrintfW(path,capacity,L"%ls\\scratch\\%ls\\",module,suite));

Cleanup:
	return hr;
}
