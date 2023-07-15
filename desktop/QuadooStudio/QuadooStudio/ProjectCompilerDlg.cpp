#include <windows.h>
#include <shlwapi.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "Library\Core\StringCore.h"
#include "Library\Util\RString.h"
#include "Library\Util\Formatting.h"
#include "Library\ChooseFile.h"
#include "Library\Sorting.h"
#include "Published\SimbeyCore.h"
#include "Published\JSON.h"
#include "ProjectCompilerDlg.h"

#define	IDR_EXE_QBC		101
#define	IDR_EXE_ARGS	102
#define	IDI_EXE_MAIN	103

CProjectCompilerDlg::CProjectCompilerDlg (IJSONObject* pProject, RSTRING rstrTarget, RSTRING rstrEngine, RSTRING rstrProjectDir, PCWSTR pcwzScript) :
	CBaseDialog(IDD_COMPILE_TO_EXE),
	m_pcwzScript(pcwzScript),
	m_rstrTemplate(NULL),
	m_hIcon(NULL),
	m_fDisplayedErrors(false)
{
	SetInterface(m_pProject, pProject);
	RStrSet(m_rstrTarget, rstrTarget);
	RStrSet(m_rstrEngine, rstrEngine);
	RStrSet(m_rstrProjectDir, rstrProjectDir);
}

CProjectCompilerDlg::~CProjectCompilerDlg ()
{
	if(m_hIcon)
		DestroyIcon(m_hIcon);

	RStrRelease(m_rstrTemplate);

	RStrRelease(m_rstrProjectDir);
	RStrRelease(m_rstrEngine);
	RStrRelease(m_rstrTarget);
	SafeRelease(m_pProject);
}

BOOL CProjectCompilerDlg::DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	switch(message)
	{
	case WM_INITDIALOG:
		Initialize();
		CenterHost();
		break;

	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDOK:
			if(FAILED(Compile()))
				break;
			WriteSettings();
			__fallthrough;
		case IDCANCEL:
			End(LOWORD(wParam));
			break;

		case IDC_SELECT_ICON:
			SelectIcon();
			break;

		case IDC_CLEAR_ICON:
			if(m_hIcon)
			{
				DestroyIcon(m_hIcon);
				m_hIcon = NULL;
				m_stmIcon.Reset();
				SendMessage(GetDlgItem(IDC_EMBED_ICON), STM_SETICON, 0, 0);
			}
			break;
		}
		break;
	}

	return FALSE;
}

HRESULT CProjectCompilerDlg::Initialize (VOID)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	PWSTR pwzPtr;
	WCHAR wzTemplate[MAX_PATH];
	INT cchTemplate;

	Check(RStrCopyToW(m_rstrEngine, ARRAYSIZE(wzTemplate), wzTemplate, &cchTemplate));
	pwzPtr = const_cast<PWSTR>(TStrCchRChr(wzTemplate, cchTemplate, L'.'));
	CheckIf(NULL == pwzPtr, E_FAIL);
	cchTemplate = static_cast<INT>(pwzPtr - wzTemplate);
	Check(TStrCchCpy(wzTemplate + cchTemplate, ARRAYSIZE(wzTemplate) - cchTemplate, L"T.BIN"));
	cchTemplate += 5;

	Check(RStrCreateW(cchTemplate, wzTemplate, &m_rstrTemplate));
	SetWindowText(GetDlgItem(IDC_BINARY_TEMPLATE), wzTemplate);

	if(SUCCEEDED(m_pProject->FindNonNullValueW(L"argsEmbedded", &srv)))
	{
		RSTRING rstrArgs;
		Check(srv->GetString(&rstrArgs));
		SetWindowText(GetDlgItem(IDC_EMBED_ARGS), RStrToWide(rstrArgs));
		RStrRelease(rstrArgs);
		srv.Release();
	}

	if(SUCCEEDED(m_pProject->FindNonNullValueW(L"icon", &srv)))
	{
		RSTRING rstrIcon;
		Check(srv->GetString(&rstrIcon));
		DecodeIcon(rstrIcon);
		RStrRelease(rstrIcon);
		srv.Release();
	}

Cleanup:
	return hr;
}

HRESULT CProjectCompilerDlg::DecodeIcon (RSTRING rstrIcon)
{
	HRESULT hr;
	RSTRING rstrBase64A = NULL;
	DWORD cbIcon;

	Check(RStrConvertToA(RStrLen(rstrIcon), RStrToWide(rstrIcon), &rstrBase64A));
	Check(ScDecodeBase64A(RStrToAnsi(rstrBase64A), RStrLen(rstrBase64A), &m_stmIcon, &cbIcon));
	Check(GetIconFromLoadedStream(32, &m_hIcon));
	CheckIfGetLastError(NULL == m_hIcon);
	SendMessage(GetDlgItem(IDC_EMBED_ICON), STM_SETICON, (WPARAM)m_hIcon, 0);

Cleanup:
	RStrRelease(rstrBase64A);
	return hr;
}

HRESULT CProjectCompilerDlg::GetIconFromLoadedStream (INT nDesiredSize, __out HICON* phIcon)
{
	HRESULT hr;
	ICONHEAD ih;
	ICONDIRENTRY* pDirectory = NULL;

	Check(m_stmIcon.CopyTo(&ih, 0, sizeof(ih)));

	pDirectory = __new ICONDIRENTRY[ih.wCount];
	CheckAlloc(pDirectory);

	Check(m_stmIcon.CopyTo(pDirectory, sizeof(ih), sizeof(ICONDIRENTRY) * ih.wCount));

	// Find the best matching icon based on desired dimensions
	int bestMatchIndex = -1;
	int bestMatchDiff = INT_MAX;

	for(WORD i = 0; i < ih.wCount; i++)
	{
		INT width = pDirectory[i].bWidth;
		INT height = pDirectory[i].bHeight;

		INT diff = abs(width - nDesiredSize) + abs(height - nDesiredSize);
		if(diff < bestMatchDiff)
		{
			bestMatchIndex = i;
			bestMatchDiff = diff;
		}
	}

	CheckIf(-1 == bestMatchIndex, HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
	ICONDIRENTRY& selectedEntry = pDirectory[bestMatchIndex];

	// Calculate the offset of the selected icon image
	DWORD imageOffset = selectedEntry.dwImageOffset;

	// Read the icon image header
	const BITMAPINFOHEADER* pcIconHeader = reinterpret_cast<const BITMAPINFOHEADER*>(m_stmIcon.GetReadPtr() + imageOffset);

	*phIcon = CreateIconFromResourceEx(reinterpret_cast<PBYTE>(const_cast<BITMAPINFOHEADER*>(pcIconHeader)), sizeof(BITMAPINFOHEADER) + pcIconHeader->biSizeImage, TRUE, 0x00030000, pcIconHeader->biWidth, pcIconHeader->biHeight >> 1, LR_DEFAULTCOLOR);
	CheckIfGetLastError(NULL == *phIcon);

Cleanup:
	__delete_array pDirectory;
	return hr;
}

HRESULT CProjectCompilerDlg::SelectIcon (VOID)
{
	HRESULT hr;
	HWND hwnd;
	CChooseFile choose;
	HANDLE hFile = INVALID_HANDLE_VALUE;
	LARGE_INTEGER liSize;
	PBYTE pbPtr;
	DWORD cb;

	Check(GetWindow(&hwnd));
	Check(choose.Initialize());
	CheckIfIgnore(!choose.OpenSingleFile(hwnd, L"Select Program Icon", L"Icon (*.ico)\0*.ico\0\0"), E_ABORT);

	hFile = CreateFileW(choose.GetFile(0), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	CheckIfGetLastError(INVALID_HANDLE_VALUE == hFile);
	CheckIfGetLastError(!GetFileSizeEx(hFile, &liSize));

	m_stmIcon.Reset();
	Check(m_stmIcon.WriteAdvance(&pbPtr, liSize.LowPart));
	CheckIfGetLastError(!ReadFile(hFile, pbPtr, liSize.LowPart, &cb, NULL));

	if(m_hIcon)
		DestroyIcon(m_hIcon);

	Check(GetIconFromLoadedStream(32, &m_hIcon));
	CheckIfGetLastError(NULL == m_hIcon);
	SendMessage(GetDlgItem(IDC_EMBED_ICON), STM_SETICON, (WPARAM)m_hIcon, 0);

Cleanup:
	SafeCloseFileHandle(hFile);
	return hr;
}

HRESULT CProjectCompilerDlg::WriteSettings (VOID)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	RSTRING rstrValueW = NULL;
	INT cch;
	DWORD cb;
	PWSTR pwzPtr;
	HWND hwndArgs = GetDlgItem(IDC_EMBED_ARGS);

	cch = GetWindowTextLength(hwndArgs);
	Check(RStrAllocW(cch, &rstrValueW, &pwzPtr));
	GetWindowText(hwndArgs, pwzPtr, cch + 1);
	Check(JSONCreateString(rstrValueW, &srv));
	Check(m_pProject->AddValueW(L"argsEmbedded", srv));

	if(0 < m_stmIcon.DataRemaining())
	{
		CMemoryStream stmBase64;

		srv.Release();
		RStrRelease(rstrValueW); rstrValueW = NULL;

		Check(ScEncodeBase64A(m_stmIcon.GetReadPtr(), m_stmIcon.DataRemaining(), &stmBase64, &cb));
		Check(RStrConvertToW(CP_ACP, stmBase64.DataRemaining(), stmBase64.TGetReadPtr<CHAR>(), &rstrValueW));
		Check(JSONCreateString(rstrValueW, &srv));
		Check(m_pProject->AddValueW(L"icon", srv));
	}
	else
		m_pProject->RemoveValueW(L"icon");

Cleanup:
	RStrRelease(rstrValueW);
	return hr;
}

HRESULT CProjectCompilerDlg::Compile (VOID)
{
	HRESULT hr;
	HWND hwnd;
	CMemoryStream stmQBC;
	CChooseFile choose;
	HANDLE hExecutable = NULL;
	LONG idxIconOffset = 0;
	PWSTR pwzCmdLine = NULL, *ppwzArgs = NULL;
	INT cchCmdLine;

	m_fDisplayedErrors = false;

	hwnd = GetDlgItem(IDC_EMBED_ARGS);
	cchCmdLine = GetWindowTextLength(hwnd);
	if(0 < cchCmdLine)
	{
		pwzCmdLine = __new WCHAR[cchCmdLine + 1];
		CheckAlloc(pwzCmdLine);
		GetWindowText(hwnd, pwzCmdLine, cchCmdLine + 1);
	}

	Check(GetWindow(&hwnd));
	Check(choose.Initialize());
	Check(choose.SetInitialDirectory(RStrToWide(m_rstrProjectDir)));
	CheckIfIgnore(!choose.SaveFile(hwnd, L"Select Compilation Output", L"Executable (*.exe)\0*.exe\0\0"), E_ABORT);

	CheckIfGetLastError(!CopyFile(RStrToWide(m_rstrTemplate), choose.GetFile(0), FALSE));
	Check(QuadooParseToStream(m_pcwzScript, 0, &stmQBC, NULL, this));

	if(0 < m_stmIcon.DataRemaining())
		Check(GetIconCountFromModule(choose.GetFile(0), &idxIconOffset));

	hExecutable = BeginUpdateResource(choose.GetFile(0), FALSE);
	CheckIfGetLastError(NULL == hExecutable);

	if(UpdateResource(hExecutable, L"QBC", MAKEINTRESOURCE(IDR_EXE_QBC), 0, const_cast<PBYTE>(stmQBC.GetReadPtr()), stmQBC.DataRemaining()))
	{
		CMemoryStream stmArgs;
		ULONG cb;

		// The input file needs to be added as the first command line argument.
		Check(stmArgs.TWrite(SLP(L"\""), &cb));
		Check(stmArgs.TWrite(m_pcwzScript, TStrLenAssert(m_pcwzScript), &cb));
		Check(stmArgs.TWrite(SLP(L"\""), &cb));

		if(pwzCmdLine)
		{
			INT cArgs;

			ppwzArgs = CommandLineToArgvW(pwzCmdLine, &cArgs);
			CheckAlloc(ppwzArgs);

			for(INT i = 0; i < cArgs; i++)
			{
				PCWSTR pcwzArg = ppwzArgs[i];

				Check(stmArgs.TWrite(SLP(L" "), &cb));

				if(TStrChr(pcwzArg, L' ') || TStrChr(pcwzArg, L'\t'))
				{
					Check(stmArgs.TWrite(SLP(L"\""), &cb));
					Check(stmArgs.TWrite(pcwzArg, TStrLenAssert(pcwzArg), &cb));
					Check(stmArgs.TWrite(SLP(L"\""), &cb));
				}
				else
					Check(stmArgs.TWrite(pcwzArg, TStrLenAssert(pcwzArg), &cb));
			}
		}

		CheckIfGetLastError(!UpdateResource(hExecutable, L"ARGS", MAKEINTRESOURCE(IDR_EXE_ARGS), 0, const_cast<PBYTE>(stmArgs.GetReadPtr()), stmArgs.DataRemaining()));

		if(0 < m_stmIcon.DataRemaining())
			Check(AddIconToModule(hExecutable, m_stmIcon, MAKEINTRESOURCE(IDI_EXE_MAIN), idxIconOffset));
	}
	else
		hr = HrEnsureLastError();

Cleanup:
	if(ppwzArgs)
		LocalFree(ppwzArgs);
	SafeDeleteArray(pwzCmdLine);
	if(hExecutable)
	{
		EndUpdateResource(hExecutable, FAILED(hr));
		if(FAILED(hr))
			DeleteFile(choose.GetFile(0));
	}
	return hr;
}

// IQuadooCompilerStatus

VOID STDMETHODCALLTYPE CProjectCompilerDlg::OnCompilerAddFile (PCWSTR pcwzFile, INT cchFile)
{
}

VOID STDMETHODCALLTYPE CProjectCompilerDlg::OnCompilerStatus (PCWSTR pcwzStatus)
{
}

VOID STDMETHODCALLTYPE CProjectCompilerDlg::OnCompilerError (HRESULT hrCode, INT nLine, PCWSTR pcwzFile, PCWSTR pcwzError)
{
	HWND hwnd;
	if(!m_fDisplayedErrors && SUCCEEDED(GetWindow(&hwnd)))
	{
		RSTRING rstrError;
		if(SUCCEEDED(RStrFormatW(&rstrError, L"%ls(%d) - %ls", pcwzFile, nLine, pcwzError)))
		{
			MessageBox(hwnd, RStrToWide(rstrError), L"Compilation Error", MB_OK | MB_ICONHAND);
			RStrRelease(rstrError);

			m_fDisplayedErrors = true;
		}
	}
}

STDMETHODIMP CProjectCompilerDlg::OnCompilerResolvePath (PCWSTR pcwzPath, __out_ecount(cchMaxAbsolutePath) PWSTR pwzAbsolutePath, INT cchMaxAbsolutePath)
{
	return E_NOTIMPL;
}

HRESULT CProjectCompilerDlg::AddIconToModule (HANDLE hModule, CMemoryStream& stmIcon, PCWSTR pcwzName, LONG idxIconOffset)
{
	HRESULT hr;
	ICONHEAD ih;
	ICONDIRENTRY* pDirectory = NULL;
	ULONG cb;
	CMemoryStream stmGroup;
	GROUPICONHEADER gih;
	GROUPICONENTRY* pIcons;

	Check(stmIcon.CopyTo(&ih, 0, sizeof(ih)));

	pDirectory = __new ICONDIRENTRY[ih.wCount];
	CheckAlloc(pDirectory);

	Check(stmIcon.CopyTo(pDirectory, sizeof(ih), sizeof(ICONDIRENTRY) * ih.wCount));

	if(1 < ih.wCount)
	{
		// Sort the icons, smallest to largest.
		Sorting::TQuickSort(pDirectory, ih.wCount, _SortIcons, NULL);
	}

	gih.wReserved = 0;
	gih.wResourceType = 1;
	gih.wImageCount = ih.wCount;
	Check(stmGroup.Write(&gih, sizeof(gih), &cb));

	Check(stmGroup.TWriteAdvance(&pIcons, gih.wImageCount));

	for(WORD i = 0; i < ih.wCount; i++)
	{
		pIcons[i].bWidth = pDirectory[i].bWidth;
		pIcons[i].bHeight = pDirectory[i].bHeight;
		pIcons[i].bColors = pDirectory[i].bColorCount;
		pIcons[i].bReserved = pDirectory[i].bReserved;
		pIcons[i].wPlanes = pDirectory[i].wPlanes;
		pIcons[i].wBitsPerPixel = pDirectory[i].wBitCount;
		pIcons[i].dwImageSize = pDirectory[i].dwBytesInRes;
		pIcons[i].wResourceID = static_cast<WORD>(i + idxIconOffset + 1);
		pIcons[i].wReserved = 0;

		CheckIfGetLastError(!UpdateResource(hModule, RT_ICON, MAKEINTRESOURCE(pIcons[i].wResourceID), 0, const_cast<PBYTE>(stmIcon.GetReadPtr() + pDirectory[i].dwImageOffset), pDirectory[i].dwBytesInRes));
	}

	CheckIfGetLastError(!UpdateResource(hModule, RT_GROUP_ICON, pcwzName, 0, const_cast<PBYTE>(stmGroup.GetReadPtr()), stmGroup.DataRemaining()));

Cleanup:
	__delete_array pDirectory;
	return hr;
}

HRESULT CProjectCompilerDlg::GetIconCountFromModule (PCWSTR pcwzExecutable, __out LONG* pidxIconOffset)
{
	HRESULT hr;
	HMODULE hModule = LoadLibraryEx(pcwzExecutable, NULL, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);

	*pidxIconOffset = 0;

	CheckIfGetLastError(NULL == hModule);
	if(!EnumResourceNames(reinterpret_cast<HMODULE>(hModule), RT_ICON, CountIcons, reinterpret_cast<LONG_PTR>(pidxIconOffset)))
	{
		LONG lError = GetLastError();
		CheckIf(ERROR_RESOURCE_TYPE_NOT_FOUND != lError && ERROR_RESOURCE_DATA_NOT_FOUND != lError, HRESULT_FROM_WIN32(lError));
	}
	hr = S_OK;

Cleanup:
	if(hModule)
		FreeLibrary(hModule);
	return hr;
}

int WINAPI CProjectCompilerDlg::_SortIcons (ICONDIRENTRY* pLeft, ICONDIRENTRY* pRight, PVOID pParam)
{
	if(pLeft->bWidth > pRight->bWidth)
		return 1;
	if(pLeft->bWidth < pRight->bWidth)
		return -1;
	return 0;
}

BOOL CALLBACK CProjectCompilerDlg::CountIcons (HMODULE hModule, PCWSTR pcwzType, PWSTR pwzName, LONG_PTR lParam)
{
	LONG* pidxIconOffset = reinterpret_cast<LONG*>(lParam);
	(*pidxIconOffset)++;
	return TRUE;
}
