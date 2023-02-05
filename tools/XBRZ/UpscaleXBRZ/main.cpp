#include <stdio.h>
#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Core\StringCore.h"
#include "3rdParty\lodepng\lodepng.h"
#include "..\XBRZ\XBRZ.h"

HRESULT UpscaleImage (__in_opt const SCALER_CONFIG* pcConfig, PCWSTR pcwzImage, PCWSTR pcwzTarget, INT nScale)
{
	HRESULT hr;
	PCWSTR pcwzSlash = TStrRChr(pcwzImage, L'\\'), pcwzName;
	if(pcwzSlash)
		pcwzName = pcwzSlash + 1;
	else
		pcwzName = pcwzImage;

	wprintf(L"Upscaling image: %ls\r\n", pcwzName);

	SIZE size, sizeOutput;
	PBYTE pbImage = NULL, pbData = NULL, pbEncoded = NULL;
	size_t cbEncoded;
	INT* pnOutput = NULL;
	HANDLE hFile = CreateFile(pcwzImage, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	CheckIfGetLastError(INVALID_HANDLE_VALUE == hFile);

	DWORD dwSize = GetFileSize(hFile, NULL);
	pbImage = __new BYTE[dwSize];
	CheckAlloc(pbImage);

	CheckIfGetLastError(!ReadFile(hFile, pbImage, dwSize, &dwSize, NULL));
	SafeCloseFileHandle(hFile);

	UINT nWidth, nHeight;
	CheckIf(0 != lodepng_decode32(&pbData, &nWidth, &nHeight, pbImage, dwSize), E_FAIL);

	size.cx = nWidth;
	size.cy = nHeight;
	Check(XBRZScaleImage(pcConfig, reinterpret_cast<const INT*>(pbData), &size, nScale, &pnOutput, &sizeOutput));

	CheckIf(0 != lodepng_encode32(&pbEncoded, &cbEncoded, reinterpret_cast<const BYTE*>(pnOutput), sizeOutput.cx, sizeOutput.cy, false), E_FAIL);

	{
		WCHAR wzOutput[MAX_PATH];
		PWSTR pwzPtr;
		INT cchRemaining;

		Check(TStrCchCpyEx(wzOutput, ARRAYSIZE(wzOutput), pcwzTarget, &pwzPtr, &cchRemaining));
		if(ARRAYSIZE(wzOutput) != cchRemaining && pwzPtr[-1] != L'\\')
		{
			*pwzPtr = L'\\';
			pwzPtr++;
			cchRemaining--;
		}

		Check(TStrCchCpy(pwzPtr, cchRemaining, pcwzName));

		hFile = CreateFile(wzOutput, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		CheckIfGetLastError(INVALID_HANDLE_VALUE == hFile);

		CheckIfGetLastError(!WriteFile(hFile, pbEncoded, cbEncoded, &dwSize, NULL));
	}

Cleanup:
	free(pbEncoded);
	free(pbData);
	XBRZFreeImage(pnOutput);
	SafeDeleteArray(pbImage);
	SafeCloseFileHandle(hFile);
	return hr;
}

HRESULT UpscaleSearchPath (__in_opt const SCALER_CONFIG* pcConfig, PCWSTR pcwzFind, PCWSTR pcwzTarget, INT nScale)
{
	HRESULT hr;
	WIN32_FIND_DATA FindData;
	HANDLE hFind = FindFirstFile(pcwzFind, &FindData);
	if(INVALID_HANDLE_VALUE != hFind)
	{
		WCHAR wzBase[MAX_PATH], *pwzPtr;
		PCWSTR pwzSlash = TStrRChr(pcwzFind, L'\\');
		INT cchBase;

		if(pwzSlash)
		{
			INT cchCopy = static_cast<INT>(pwzSlash - pcwzFind) + 1;
			Check(TStrCchCpyN(wzBase, ARRAYSIZE(wzBase), pcwzFind, cchCopy));
			pwzPtr = wzBase + cchCopy;
			cchBase = ARRAYSIZE(wzBase) - cchCopy;
		}
		else
		{
			pwzPtr = wzBase;
			cchBase = ARRAYSIZE(wzBase);
		}

		do
		{
			if((FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			{
				Check(TStrCchCpy(pwzPtr, cchBase, FindData.cFileName));
				Check(UpscaleImage(pcConfig, wzBase, pcwzTarget, nScale));
			}
		} while(FindNextFile(hFind, &FindData));
		FindClose(hFind);
	}
	else
		hr = HrEnsureLastError();

Cleanup:
	return hr;
}

INT wmain (INT cArgs, WCHAR* pwzArgs[])
{
#if defined(_DEBUG) && !defined(__VIRTUAL_DBGMEM)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	if(4 <= cArgs)
	{
		PCWSTR pcwzFind = pwzArgs[1];
		PCWSTR pcwzTarget = pwzArgs[2];
		INT nScale = _wtoi(pwzArgs[3]);
		SCALER_CONFIG config, *pConfigPtr = NULL;

		CreateDirectory(pcwzTarget, NULL);

		for(INT i = 4; i < cArgs; i++)
		{
			PCWSTR pcwzArg = pwzArgs[i];
			if(pcwzArg[0] == '-' || pcwzArg[0] == '/')
			{
				if(i + 1 == cArgs)
				{
					wprintf(L"ERROR: Missing parameter value!\r\n");
					goto PrintHelp;
				}
				PCWSTR pcwzValue = pwzArgs[++i];

				if(NULL == pConfigPtr)
				{
					pConfigPtr = &config;
					XBRZGetDefaults(&config);
				}

				switch(toupper(pcwzArg[1]))
				{
				case 'D':
					config.DominantDirectionThreshold = _wtof(pcwzValue);
					break;
				case 'E':
					config.EqualColorTolerance = _wtof(pcwzValue);
					break;
				case 'L':
					config.LuminanceWeight = _wtof(pcwzValue);
					break;
				case 'S':
					config.SteepDirectionThreshold = _wtof(pcwzValue);
					break;
				}
			}
		}

		DWORD dwAttribs = GetFileAttributes(pcwzFind);
		if(INVALID_FILE_ATTRIBUTES == dwAttribs)
			UpscaleSearchPath(pConfigPtr, pcwzFind, pcwzTarget, nScale);
		else if(dwAttribs & FILE_ATTRIBUTE_DIRECTORY)
		{
			WCHAR wzBuffer[MAX_PATH], *pwzPtr;
			INT cchRemaining;

			if(SUCCEEDED(TStrCchCpyEx(wzBuffer, ARRAYSIZE(wzBuffer), pcwzFind, &pwzPtr, &cchRemaining)))
			{
				if(ARRAYSIZE(wzBuffer) != cchRemaining && pwzPtr[-1] != L'\\')
				{
					*pwzPtr = L'\\';
					pwzPtr++;
					cchRemaining--;
				}

				if(SUCCEEDED(TStrCchCpy(pwzPtr, cchRemaining, L"*.png")))
					UpscaleSearchPath(pConfigPtr, wzBuffer, pcwzTarget, nScale);
			}
		}
		else
			UpscaleImage(pConfigPtr, pcwzFind, pcwzTarget, nScale);
	}
	else
	{
PrintHelp:
		PCWSTR pcwzName = pwzArgs[0];
		PCWSTR pcwzPtr = TStrRChr(pcwzName, L'\\');
		if(pcwzPtr)
			pcwzName = pcwzPtr + 1;
		wprintf(L"Required usage:\r\n\t%ls <source query> <target path> <scale value: 0 - 3>\r\n", pcwzName);
		wprintf(L"Optional parameters:\r\n\t[-D <Dominant Direction Threshold>]\r\n");
		wprintf(L"\t[-E <Equal Color Tolerance>]\r\n");
		wprintf(L"\t[-L <Luminance Weight>]\r\n");
		wprintf(L"\t[-S <Steep Direction Threshold>]\r\n");
	}

	return 0;
}
