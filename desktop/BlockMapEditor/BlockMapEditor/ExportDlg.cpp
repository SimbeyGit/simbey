#include <windows.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "Library\Util\StreamHelpers.h"
#include "Library\Util\Formatting.h"
#include "Library\ChooseFile.h"
#include "ConfigDlg.h"
#include "MapLine.h"
#include "MapThing.h"
#include "ExportDlg.h"

#define	WM_ADD_DRAW_ITEM			(WM_USER+1)
#define	WM_END_CONVERSION			(WM_USER+2)

#define	DI_LINES					0
#define	DI_MERGES					1
#define	DI_THINGS					2

BOOL GetLocalWindowRect (HWND hwnd, __out RECT* prc)
{
	if(GetWindowRect(hwnd, prc))
	{
		MapWindowPoints(HWND_DESKTOP, GetParent(hwnd), (LPPOINT)prc, 2);
		return TRUE;
	}
	return FALSE;
}

///////////////////////////////////////////////////////////////////////////////
// CZDBSPFile
///////////////////////////////////////////////////////////////////////////////

CZDBSPFile::CZDBSPFile ()
{
}

CZDBSPFile::CZDBSPFile (CMemFileData* pmfd) :
	m_memFile(pmfd)
{
}

HRESULT CZDBSPFile::InitializeNew (VOID)
{
	return m_memFile.InitializeNew();
}

CMemFileData* CZDBSPFile::GetFileData (VOID) const
{
	return m_memFile.GetFileData();
}

// IZDBSPFile

void CZDBSPFile::Close (void)
{
}

size_t CZDBSPFile::Read (void* pvData, size_t cbMaxData)
{
	ULONG cb = 0;
	m_memFile.Read(pvData, cbMaxData, &cb);
	return cb;
}

size_t CZDBSPFile::Write (const void* pcvData, size_t cbData)
{
	ULONG cb = 0;
	m_memFile.Write(pcvData, cbData, &cb);
	return cb;
}

int CZDBSPFile::Seek (long nOffset, int nOrigin)
{
	LARGE_INTEGER liMove;

	liMove.QuadPart = nOffset;
	return SUCCEEDED(m_memFile.Seek(liMove, nOrigin, NULL)) ? 0 : -1;
}

long CZDBSPFile::Tell (void)
{
	return (long)m_memFile.GetOffset();
}

///////////////////////////////////////////////////////////////////////////////
// CZDBSPFS
///////////////////////////////////////////////////////////////////////////////

CZDBSPFS::CZDBSPFS (CMemoryStream* pstmLogs) :
	m_mapFiles(TStrCmpIAssert)
{
	SetInterface(m_pstmLogs, pstmLogs);
}

CZDBSPFS::~CZDBSPFS ()
{
	m_mapFiles.DeleteAll();
	SafeRelease(m_pstmLogs);
}

HRESULT CZDBSPFS::AddFile (PCSTR pcszFile, CMemoryFile* pmemFile)
{
	HRESULT hr;
	CZDBSPFile* pFile = __new CZDBSPFile(pmemFile->GetFileData());

	CheckAlloc(pFile);
	Check(m_mapFiles.Add(pcszFile, pFile));
	pFile = NULL;

Cleanup:
	__delete pFile;
	return hr;
}

HRESULT CZDBSPFS::GetFileData (PCSTR pcszFile, __deref_out CMemFileData** ppmfd)
{
	HRESULT hr;
	CZDBSPFile* pFile;

	Check(m_mapFiles.Find(pcszFile, &pFile));
	*ppmfd = pFile->GetFileData();

Cleanup:
	return hr;
}

// IZDBSPFS

bool CZDBSPFS::CheckInOutNames (const char* pcszInName, const char* pcszOutName)
{
	return 0 == TStrCmpIAssert(pcszInName, pcszOutName);
}

int CZDBSPFS::Remove (const char* pcszName)
{
	CZDBSPFile* pFile;
	if(SUCCEEDED(m_mapFiles.Remove(pcszName, &pFile)))
	{
		__delete pFile;
		return 0;
	}
	return -1;
}

int CZDBSPFS::Rename (const char* pcszOld, const char* pcszNew)
{
	CZDBSPFile* pFile;
	if(SUCCEEDED(m_mapFiles.Remove(pcszOld, &pFile)))
	{
		if(SUCCEEDED(m_mapFiles.Add(pcszNew, pFile)))
			return 0;

		m_mapFiles.Add(pcszOld, pFile);
	}
	return -1;
}

IZDBSPFile* CZDBSPFS::OpenForReading (const char* pcszFile)
{
	CZDBSPFile* pFile;
	if(SUCCEEDED(m_mapFiles.Find(pcszFile, &pFile)))
		return pFile;
	return NULL;
}

IZDBSPFile* CZDBSPFS::OpenForWriting (const char* pcszFile)
{
	CZDBSPFile* pFile = NULL;
	if(FAILED(m_mapFiles.Find(pcszFile, &pFile)))
	{
		pFile = __new CZDBSPFile;
		if(pFile)
		{
			if(SUCCEEDED(pFile->InitializeNew()))
			{
				if(FAILED(m_mapFiles.Add(pcszFile, pFile)))
				{
					__delete pFile;
					pFile = NULL;
				}
			}
			else
			{
				__delete pFile;
				pFile = NULL;
			}
		}
		else
		{
			__delete pFile;
			pFile = NULL;
		}
	}
	return pFile;
}

// IZDBSPLogger

int CZDBSPFS::Log (const char* pcszFormat, ...)
{
	INT cch;

	va_list vArgs;
	va_start(vArgs, pcszFormat);

	cch = LogV(pcszFormat, vArgs);

	va_end(vArgs);

	return cch;
}

int CZDBSPFS::LogV (const char* pcszFormat, va_list vArgs)
{
	INT cchLogs = m_pstmLogs->DataRemaining();
	if(SUCCEEDED(Stream::TPrintVF(m_pstmLogs, pcszFormat, vArgs)))
		return m_pstmLogs->DataRemaining() - cchLogs;
	return 0;
}

///////////////////////////////////////////////////////////////////////////////
// CMapConverterView
///////////////////////////////////////////////////////////////////////////////

CMapConverterView::CMapConverterView (CBlockMap* pBlockMap, PCWSTR pcwzName, INT xCell, INT zCell, CConfigDlg* pdlgConfig) :
	m_pAdapter(NULL),
	m_pBlockMap(pBlockMap),
	m_pdlgConfig(pdlgConfig),
	m_pConvert(NULL),
	m_hThread(NULL),
	m_xSpan(-1),
	m_ySpan(-1)
{
	PCWSTR pcwzExt = TStrChr(pcwzName, L'.');
	if(pcwzExt)
	{
		INT cch = WideCharToMultiByte(CP_ACP, 0, pcwzName, static_cast<INT>(pcwzExt - pcwzName), m_szLevel, ARRAYSIZE(m_szLevel), NULL, NULL);
		if(cch < ARRAYSIZE(m_szLevel))
			m_szLevel[cch] = '\0';
	}
	else
		WideCharToMultiByte(CP_ACP, 0, pcwzName, -1, m_szLevel, ARRAYSIZE(m_szLevel), NULL, NULL);
	TStrUprAssert(m_szLevel);

	m_Graph.SetGraphType(GRAPH_XZ);
	m_Graph.SetGridType(GRID_AXIS_POINTS);
	m_Graph.AttachContainer(this);
	m_Graph.SetGraphTarget(this);

	m_Graph.SetBGColor(RGB(96, 96, 96));

	m_Graph.SetCenter((xCell - MAP_WIDTH / 2) * CELL_SCALE, (zCell - MAP_HEIGHT / 2) * CELL_SCALE);
	m_Graph.SetScale(0.05f);
}

CMapConverterView::~CMapConverterView ()
{
	Assert(NULL == m_pConvert);

	m_aLines.DeleteAll();
	m_aMerges.DeleteAll();
	m_aThings.DeleteAll();

	m_Graph.AttachContainer(NULL);
}

VOID CMapConverterView::GetMapSize (__out SHORT& x, __out SHORT& y)
{
	x = m_xSpan;
	y = m_ySpan;
}

// IAdapterWindowCallback

BOOL CMapConverterView::DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	BOOL fHandled = FALSE;

	switch(message)
	{
	case WM_PAINT:
		{
			HWND hwnd;

			if(SUCCEEDED(m_pAdapter->GetWindow(&hwnd)))
			{
				PAINTSTRUCT ps;
				HDC hdc = BeginPaint(hwnd, &ps);
				m_Graph.Paint(hdc);

				HGDIOBJ hDefault = SelectObject(hdc, GetStockObject(WHITE_PEN));
				for(sysint i = 0; i < m_aLines.Length(); i++)
					DrawLine(hdc, m_aLines[i]);
				for(sysint i = 0; i < m_aThings.Length(); i++)
					DrawThing(hdc, m_aThings[i]);
				SelectObject(hdc, hDefault);

				SelectObject(hdc, CreatePen(PS_SOLID, 1, RGB(255, 0, 0)));
				for(sysint i = 0; i < m_aMerges.Length(); i++)
					DrawLine(hdc, m_aMerges[i]);
				DeleteObject(SelectObject(hdc, hDefault));

				EndPaint(hwnd, &ps);
			}
		}
		break;

	case WM_SIZE:
		{
			RECT rc = { 0, 0, LOWORD(lParam), HIWORD(lParam) };
			m_Graph.Move(&rc);
		}
		break;

	case WM_GETDLGCODE:
		lResult = DLGC_WANTARROWS | DLGC_WANTCHARS;
		fHandled = TRUE;
		break;

	case WM_ADD_DRAW_ITEM:
		{
			HWND hwnd;
			HDC hdc;
			HGDIOBJ hDefault;

			m_pAdapter->GetWindow(&hwnd);
			hdc = GetDC(hwnd);

			switch(lParam)
			{
			case DI_LINES:
				hDefault = SelectObject(hdc, GetStockObject(WHITE_PEN));
				m_aLines.Append((DRAWLINE*)wParam);
				DrawLine(hdc, (DRAWLINE*)wParam);
				SelectObject(hdc, hDefault);
				break;
			case DI_MERGES:
				hDefault = SelectObject(hdc, CreatePen(PS_SOLID, 1, RGB(255, 0, 0)));
				m_aMerges.Append((DRAWLINE*)wParam);
				DrawLine(hdc, (DRAWLINE*)wParam);
				DeleteObject(SelectObject(hdc, hDefault));
				break;
			case DI_THINGS:
				hDefault = SelectObject(hdc, GetStockObject(WHITE_PEN));
				m_aThings.Append((DRAWTHING*)wParam);
				DrawThing(hdc, (DRAWTHING*)wParam);
				SelectObject(hdc, hDefault);
				break;
			}

			ReleaseDC(hwnd, hdc);
		}
		break;

	case WM_END_CONVERSION:
		{
			HWND hwnd;

			CloseConversion();
			if(SUCCEEDED(m_pAdapter->GetWindow(&hwnd)))
				PostMessage(GetParent(hwnd), WM_COMMAND, GetDlgCtrlID(hwnd), lParam);
		}
		break;

	default:
		fHandled = m_Graph.OnMessage(message, wParam, lParam, lResult);
		break;
	}

	return fHandled;
}

VOID CMapConverterView::OnAttachingAdapter (IBaseWindow* pAdapter)
{
	DWORD idThread;

	Assert(NULL == m_pAdapter);
	SetInterface(m_pAdapter, pAdapter);

	Assert(NULL == m_pConvert);
	m_pConvert = __new CMapConvert;
	if(m_pConvert)
	{
		Assert(NULL == m_hThread);
		m_hThread = CreateThread(NULL, 0, _ConversionEntry, this, 0, &idThread);
	}
}

VOID CMapConverterView::OnDetachingAdapter (IBaseWindow* pAdapter)
{
	if(m_pConvert)
	{
		CloseConversion();
		SafeDelete(m_pConvert);
	}

	Assert(pAdapter == m_pAdapter);
	SafeRelease(m_pAdapter);
}

// IGraphContainer

HRESULT WINAPI CMapConverterView::SetFocus (__in IGrapher* pGraphCtrl)
{
	HRESULT hr;
	HWND hwnd;

	Check(m_pAdapter->GetWindow(&hwnd));
	::SetFocus(hwnd);

Cleanup:
	return hr;
}

HRESULT WINAPI CMapConverterView::ScreenToWindowless (__in IGrapher* pGraphCtrl, __inout PPOINT ppt)
{
	HRESULT hr;
	HWND hwnd;

	Check(m_pAdapter->GetWindow(&hwnd));
	CheckIfGetLastError(!ScreenToClient(hwnd, ppt));

Cleanup:
	return hr;
}

HRESULT WINAPI CMapConverterView::InvalidateContainer (__in IGrapher* pGraphCtrl)
{
	return m_pAdapter ? m_pAdapter->Invalidate(FALSE) : S_FALSE;
}

VOID WINAPI CMapConverterView::DrawDib24 (HDC hdcDest, LONG x, LONG y, HDC hdcSrc, const BYTE* pcDIB24, LONG xSize, LONG ySize, LONG lPitch)
{
	BitBlt(hdcDest, x, y, xSize, ySize, hdcSrc, 0, 0, SRCCOPY);
}

BOOL WINAPI CMapConverterView::CaptureMouse (__in IGrapher* pGraphCtrl, BOOL fCapture)
{
	if(fCapture)
	{
		HWND hwnd;

		if(SUCCEEDED(m_pAdapter->GetWindow(&hwnd)))
		{
			SetCapture(hwnd);
			return TRUE;
		}
		return FALSE;
	}
	else
		return ReleaseCapture();
}

// IGraphClient

VOID CMapConverterView::onGraphPaint (IGrapher* lpGraph)
{
	// TODO
}

VOID CMapConverterView::onGraphMouseMove (DWORD dwKeys, FLOAT x, FLOAT y)
{
}

VOID CMapConverterView::onGraphLBtnDown (DWORD dwKeys, FLOAT x, FLOAT y)
{
}

VOID CMapConverterView::onGraphLBtnUp (DWORD dwKeys, FLOAT x, FLOAT y)
{
}

BOOL CMapConverterView::onGraphRBtnDown (DWORD dwKeys, FLOAT x, FLOAT y)
{
	return FALSE;
}

VOID CMapConverterView::onGraphRBtnUp (DWORD dwKeys, FLOAT x, FLOAT y)
{
}

VOID CMapConverterView::onGraphLBtnDbl (DWORD dwKeys, FLOAT x, FLOAT y)
{
}

VOID CMapConverterView::onGraphRBtnDbl (DWORD dwKeys, FLOAT x, FLOAT y)
{
}

VOID CMapConverterView::onGraphViewChanged (BOOL fZoomChanged)
{
}

BOOL CMapConverterView::onGraphKeyDown (WPARAM iKey)
{
	return FALSE;
}

BOOL CMapConverterView::onGraphKeyUp (WPARAM iKey)
{
	return FALSE;
}

BOOL CMapConverterView::onGraphChar (WPARAM iKey)
{
	return FALSE;
}

BOOL CMapConverterView::onGraphWheel (SHORT sDistance, FLOAT x, FLOAT y)
{
	return FALSE;
}

HRESULT CMapConverterView::onGraphGetAcc (IAccessible** lplpAccessible)
{
	return E_NOTIMPL;
}

// IMapConvertProgress

VOID CMapConverterView::ReportAddLine (CMapLine* pLine)
{
	HWND hwnd;

	if(SUCCEEDED(m_pAdapter->GetWindow(&hwnd)))
	{
		DRAWLINE* pItem = __new DRAWLINE;
		if(pItem)
		{
			pItem->x1 = pLine->m_vFrom.x;
			pItem->y1 = -pLine->m_vFrom.y;
			pItem->x2 = pLine->m_vTo.x;
			pItem->y2 = -pLine->m_vTo.y;

			PostMessage(hwnd, WM_ADD_DRAW_ITEM, (WPARAM)pItem, DI_LINES);
		}
	}
}

VOID CMapConverterView::ReportMergeDone (CMapLine* pLine)
{
	HWND hwnd;

	if(SUCCEEDED(m_pAdapter->GetWindow(&hwnd)))
	{
		DRAWLINE* pItem = __new DRAWLINE;
		if(pItem)
		{
			pItem->x1 = pLine->m_vFrom.x;
			pItem->y1 = -pLine->m_vFrom.y;
			pItem->x2 = pLine->m_vTo.x;
			pItem->y2 = -pLine->m_vTo.y;

			PostMessage(hwnd, WM_ADD_DRAW_ITEM, (WPARAM)pItem, DI_MERGES);
		}
	}
}

VOID CMapConverterView::ReportAddThing (CMapThing* pThing)
{
	HWND hwnd;

	if(SUCCEEDED(m_pAdapter->GetWindow(&hwnd)))
	{
		DRAWTHING* pItem = __new DRAWTHING;
		if(pItem)
		{
			pItem->x = pThing->m_x;
			pItem->y = -pThing->m_y;

			PostMessage(hwnd, WM_ADD_DRAW_ITEM, (WPARAM)pItem, DI_THINGS);
		}
	}
}

VOID CMapConverterView::DrawLine (HDC hdc, DRAWLINE* pLine)
{
	INT x, y;

	m_Graph.GraphToClient((FLOAT)pLine->x1, (FLOAT)pLine->y1, x, y);
	MoveToEx(hdc, x, y, NULL);

	m_Graph.GraphToClient((FLOAT)pLine->x2, (FLOAT)pLine->y2, x, y);
	LineTo(hdc, x, y);
}

VOID CMapConverterView::DrawThing (HDC hdc, DRAWTHING* pThing)
{
	INT xLeft, yTop, xRight, yBottom;

	m_Graph.GraphToClient((FLOAT)pThing->x - 8.0f, (FLOAT)pThing->y - 8.0f, xLeft, yTop);
	m_Graph.GraphToClient((FLOAT)pThing->x + 8.0f, (FLOAT)pThing->y + 8.0f, xRight, yBottom);

	Rectangle(hdc, xLeft, yTop, xRight, yBottom);
}

HRESULT CMapConverterView::BuildNodes (VOID)
{
	HRESULT hr;
	HMODULE hZDBSP = LoadLibrary(m_pdlgConfig->m_wzZDBSPPath);
	int (__stdcall* pfnzdbsp_core)(ZDBSPArgs* pArgs);
	CZDBSPFS zdbspfs(&m_stmLogs);
	ZDBSPArgs args;
	PSTR pszArgs[7];
	CMemFileData* pmfdOutput;

	Check(zdbspfs.AddFile("input.wad", &m_memFile));

	pszArgs[0] = "zdbsp.exe";
	pszArgs[1] = "-m";
	pszArgs[2] = m_szLevel;
	pszArgs[3] = "-f";
	pszArgs[4] = "input.wad";
	pszArgs[5] = "-o";
	pszArgs[6] = "output.wad";
	args.argv = pszArgs;
	args.argc = ARRAYSIZE(pszArgs);

	CheckIfGetLastError(NULL == hZDBSP);
	Check(TGetFunction(hZDBSP, "zdbsp_core", &pfnzdbsp_core));

	args.pFS = &zdbspfs;
	args.pLogger = &zdbspfs;
	CheckIf(0 != pfnzdbsp_core(&args), E_FAIL);

	Check(zdbspfs.GetFileData("output.wad", &pmfdOutput));
	m_memFile.ReplaceData(pmfdOutput);

Cleanup:
	if(hZDBSP)
		FreeLibrary(hZDBSP);
	return hr;
}

HRESULT CMapConverterView::ConvertMapAsync (VOID)
{
	HRESULT hr;
	HWND hwnd = NULL;
	WOLFDATA data;

	Check(m_pAdapter->GetWindow(&hwnd));

	data.xSize = MAP_WIDTH;
	data.zSize = MAP_HEIGHT;
	data.pMap = m_pBlockMap;
	data.yFloor = 0;
	Check(m_pConvert->RunConversion(this, m_szLevel, &m_memFile, &data, m_pdlgConfig));
	m_pConvert->GetMapSize(m_xSpan, m_ySpan);
	Check(BuildNodes());

Cleanup:
	if(hwnd)
		PostMessage(hwnd, WM_END_CONVERSION, 0, hr);
	return hr;
}

VOID CMapConverterView::CloseConversion (VOID)
{
	if(m_hThread)
	{
		WaitForSingleObject(m_hThread, INFINITE);
		SafeCloseHandle(m_hThread);
	}
}

DWORD WINAPI CMapConverterView::_ConversionEntry (PVOID pvParam)
{
	return reinterpret_cast<CMapConverterView*>(pvParam)->ConvertMapAsync();
}

///////////////////////////////////////////////////////////////////////////////
// CExportDlg
///////////////////////////////////////////////////////////////////////////////

CExportDlg::CExportDlg () :
	CBaseDialog(IDD_EXPORT),
	m_pConverter(NULL)
{
}

CExportDlg::~CExportDlg ()
{
	SafeRelease(m_pConverter);
}

HRESULT CExportDlg::Initialize (CBlockMap* pBlockMap, PCWSTR pcwzName, CConfigDlg* pdlgConfig)
{
	HRESULT hr;
	INT xCell, zCell;

	Check(pBlockMap->FindStartSpot(xCell, zCell));

	m_pConverter = __new CMapConverterView(pBlockMap, pcwzName, xCell, zCell, pdlgConfig);
	CheckAlloc(m_pConverter);
	Check(m_pConverter->GetFile()->InitializeNew());

Cleanup:
	return hr;
}

BOOL CExportDlg::DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	BOOL fHandled = FALSE;

	switch(message)
	{
	case WM_INITDIALOG:
		{
			HWND hwnd;
			HWND hwndConversion = GetDlgItem(IDC_CONVERSION);
			HWND hwndOK = GetDlgItem(IDOK);
			RECT rc, rcConversion, rcButtons, rcLabel;

			SideAssertHr(GetWindow(&hwnd));
			GetClientRect(hwnd, &rc);

			SetWindowLongPtr(hwnd, GWL_STYLE, GetWindowLongPtr(hwnd, GWL_STYLE) | WS_MAXIMIZEBOX);

			CDialogControlAdapter::Attach(hwndConversion, m_pConverter);
			EnableWindow(hwndOK, FALSE);

			GetLocalWindowRect(hwndConversion, &rcConversion);
			m_nBottomMargin = rc.bottom - rcConversion.bottom;

			GetLocalWindowRect(hwndOK, &rcButtons);
			m_xSaveRight = rc.right - rcButtons.left;
			m_yButtonsDelta = rcButtons.top - rcConversion.bottom;

			GetLocalWindowRect(GetDlgItem(IDCANCEL), &rcButtons);
			m_xCancelRight = rc.right - rcButtons.left;

			GetLocalWindowRect(GetDlgItem(IDC_STATUS), &rcLabel);
			m_xStatus = rcLabel.left;
			m_yStatusDelta = rcLabel.top - rcConversion.bottom;

			m_sizeMin.cx = (rcButtons.right - rcButtons.left) * 3;
			m_sizeMin.cy = (rcButtons.bottom - rcButtons.top) * 2;

			AdjustLayout(rc.right, rc.bottom, FALSE);
			SetFocus(hwndConversion);
		}
		break;

	case WM_SIZE:
		AdjustLayout(LOWORD(lParam), HIWORD(lParam), TRUE);
		fHandled = TRUE;
		break;

	case WM_GETMINMAXINFO:
		{
			MINMAXINFO* pMinMax = (MINMAXINFO*)lParam;
			pMinMax->ptMinTrackSize.x = m_sizeMin.cx;
			pMinMax->ptMinTrackSize.y = m_sizeMin.cy;
			fHandled = TRUE;
		}
		break;

	case WM_COMMAND:
		if(IDOK == LOWORD(wParam))
		{
			CChooseFile Save;
			HWND hwnd;

			if(SUCCEEDED(Save.Initialize()) && SUCCEEDED(GetWindow(&hwnd)))
			{
				if(Save.SaveFile(hwnd, L"Export to WAD File", L"WAD Files (*.WAD\0*.wad\0\0"))
				{
					CMemoryFile* pmemFile = m_pConverter->GetFile();
					HANDLE hFile = CreateFile(Save.GetFile(0), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
					if(INVALID_HANDLE_VALUE != hFile)
					{
						DWORD cbWritten;
						WriteFile(hFile, pmemFile->GetReadPtr(), (DWORD)pmemFile->GetRemaining(), &cbWritten, NULL);
						CloseHandle(hFile);
						End(IDOK);
					}
				}
			}
		}
		else if(IDCANCEL == LOWORD(wParam))
			End(IDCANCEL);
		else if(IDC_CONVERSION == LOWORD(wParam))
		{
			if(S_OK == static_cast<HRESULT>(lParam))
			{
				SHORT x, y;
				WCHAR wzSize[32];
				m_pConverter->GetMapSize(x, y);
				Formatting::TPrintF(wzSize, ARRAYSIZE(wzSize), NULL, L"%d, %d", x, y);
				SetWindowText(GetDlgItem(IDC_EXPORT_SIZE), wzSize);

				SetWindowText(GetDlgItem(IDC_STATUS), L"Map conversion successful!");
				EnableWindow(GetDlgItem(IDOK), TRUE);
			}
			else
			{
				SetWindowText(GetDlgItem(IDC_STATUS), L"The map could not be converted!");
				CheckZDBSPLogs();
			}
		}
		break;
	}

	return fHandled;
}

VOID CExportDlg::AdjustLayout (INT xSize, INT ySize, BOOL fRepaint)
{
	RECT rc;
	INT xSave;

	INT yConversion = ySize - m_nBottomMargin;
	MoveWindow(GetDlgItem(IDC_CONVERSION), 0, 0, xSize, yConversion, fRepaint);

	GetLocalWindowRect(GetDlgItem(IDOK), &rc);
	xSave = xSize - m_xSaveRight;
	MoveWindow(GetDlgItem(IDOK), xSave, yConversion + m_yButtonsDelta, rc.right - rc.left, rc.bottom - rc.top, fRepaint);

	GetLocalWindowRect(GetDlgItem(IDCANCEL), &rc);
	MoveWindow(GetDlgItem(IDCANCEL), xSize - m_xCancelRight, yConversion + m_yButtonsDelta, rc.right - rc.left, rc.bottom - rc.top, fRepaint);

	GetLocalWindowRect(GetDlgItem(IDC_STATUS), &rc);
	MoveWindow(GetDlgItem(IDC_STATUS), m_xStatus, yConversion + m_yStatusDelta, xSave - m_xStatus, rc.bottom - rc.top, fRepaint);
}

VOID CExportDlg::CheckZDBSPLogs (VOID)
{
	CMemoryStream* pstmLogs = m_pConverter->GetLogs();
	if(0 < pstmLogs->DataRemaining())
	{
		INT cchLogs = MultiByteToWideChar(CP_ACP, 0, pstmLogs->TGetReadPtr<CHAR>(), pstmLogs->DataRemaining(), NULL, 0);
		if(0 < cchLogs)
		{
			PWSTR pwzLogs = __new WCHAR[cchLogs + 1];
			if(pwzLogs)
			{
				HWND hwnd;

				MultiByteToWideChar(CP_ACP, 0, pstmLogs->TGetReadPtr<CHAR>(), pstmLogs->DataRemaining(), pwzLogs, cchLogs);
				if(SUCCEEDED(GetWindow(&hwnd)))
				{
					pwzLogs[cchLogs] = L'\0';
					MessageBox(hwnd, pwzLogs, L"ZDBSP Nodes Errors", MB_OK);
				}

				__delete_array pwzLogs;
			}
		}
	}
}
