#include <windows.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "Library\Util\Formatting.h"
#include "Library\Util\Options.h"
#include "Library\Util\Registry.h"
#include "Library\Util\TextHelpers.h"
#include "Library\Window\DialogHost.h"
#include "Library\ChooseFile.h"
#include "Library\DPI.h"
#include "Published\SimbeyCore.h"
#include "Published\JSON.h"
#include "Ribbon.h"
#include "RibbonMappings.h"
#include "BlockMap.h"
#include "PropertiesDlg.h"
#include "ExportDlg.h"
#include "DifficultyDlg.h"
#include "BlockMapEditorApp.h"

const WCHAR c_wzAppClassName[] = L"BlockMapEditorAppCls";
const WCHAR c_wzAppTitle[] = L"Block Map Editor";
const WCHAR c_wzAppKey[] = L"Software\\Simbey\\BlockMapEditor";

const WCHAR c_wzTexturesSlash[] = L"TEXTURES\\";
const WCHAR c_wzSpritesSlash[] = L"SPRITES\\";

#define	BALLOON_RADIUS		20

namespace Balloon
{
	enum Dir
	{
		None,
		Left,
		Up,
		Right,
		Down
	};
}

VOID BalloonSide (HDC hdc, Balloon::Dir ePoint, INT xPoint, INT yPoint, INT x1, INT y1, INT x2, INT y2)
{
	LineTo(hdc, x1, y1);

	switch(ePoint)
	{
	case Balloon::Left:
		LineTo(hdc, x1, yPoint - BALLOON_RADIUS / 2);
		LineTo(hdc, xPoint, yPoint);
		LineTo(hdc, x1, yPoint + BALLOON_RADIUS / 2);
		break;
	case Balloon::Up:
		LineTo(hdc, xPoint + BALLOON_RADIUS / 2, y1);
		LineTo(hdc, xPoint, yPoint);
		LineTo(hdc, xPoint - BALLOON_RADIUS / 2, y1);
		break;
	case Balloon::Right:
		LineTo(hdc, x1, yPoint + BALLOON_RADIUS / 2);
		LineTo(hdc, xPoint, yPoint);
		LineTo(hdc, x1, yPoint - BALLOON_RADIUS / 2);
		break;
	case Balloon::Down:
		LineTo(hdc, xPoint - BALLOON_RADIUS / 2, y1);
		LineTo(hdc, xPoint, yPoint);
		LineTo(hdc, xPoint + BALLOON_RADIUS / 2, y1);
		break;
	}

	LineTo(hdc, x2, y2);
}

VOID PaintBalloon (HDC hdc, Balloon::Dir eDir, const RECT* pcrcBounds, INT x, INT y, INT nWidth, INT nHeight, __out RECT* prc)
{
	RECT rcBalloon;
	INT xMid = pcrcBounds->left + (pcrcBounds->right - pcrcBounds->left) / 2;
	INT yMid = pcrcBounds->top + (pcrcBounds->bottom - pcrcBounds->top) / 2;

	switch(eDir)
	{
	case Balloon::Up:
	case Balloon::Down:
		if(x < pcrcBounds->left + nWidth)
		{
			rcBalloon.left = x - BALLOON_RADIUS * 2;
			rcBalloon.right = rcBalloon.left + (nWidth + BALLOON_RADIUS * 2);
		}
		else if(x > pcrcBounds->right - nWidth)
		{
			rcBalloon.right = x + BALLOON_RADIUS * 2;
			rcBalloon.left = rcBalloon.right - (nWidth + BALLOON_RADIUS * 2);
		}
		else
		{
			rcBalloon.left = x - (nWidth / 2 + BALLOON_RADIUS);
			rcBalloon.right = x + nWidth / 2 + BALLOON_RADIUS;
		}

		if(Balloon::Up == eDir)
		{
			rcBalloon.top = y + BALLOON_RADIUS;
			rcBalloon.bottom = rcBalloon.top + nHeight + BALLOON_RADIUS * 2;
		}
		else
		{
			rcBalloon.bottom = y - BALLOON_RADIUS;
			rcBalloon.top = rcBalloon.bottom - (nHeight + BALLOON_RADIUS * 2);
		}
		break;
	case Balloon::Left:
	case Balloon::Right:
		// TODO
		ZeroMemory(&rcBalloon, sizeof(rcBalloon));
		break;
	}

	BeginPath(hdc);
	MoveToEx(hdc, rcBalloon.left + BALLOON_RADIUS, rcBalloon.top, NULL);

	// Top Left
	ArcTo(hdc,
		rcBalloon.left, rcBalloon.top, rcBalloon.left + 2 * BALLOON_RADIUS, rcBalloon.top + 2 * BALLOON_RADIUS,
		rcBalloon.left + BALLOON_RADIUS, rcBalloon.top, rcBalloon.left, rcBalloon.top + BALLOON_RADIUS);

	// Left Side
	BalloonSide(hdc, Balloon::Left == eDir ? Balloon::Left : Balloon::None, x, y,
		rcBalloon.left, rcBalloon.top + BALLOON_RADIUS,
		rcBalloon.left, rcBalloon.bottom - BALLOON_RADIUS);

	// Bottom Left
	ArcTo(hdc,
		rcBalloon.left, rcBalloon.bottom - 2 * BALLOON_RADIUS, rcBalloon.left + 2 * BALLOON_RADIUS, rcBalloon.bottom,
		rcBalloon.left, rcBalloon.bottom - BALLOON_RADIUS, rcBalloon.left + BALLOON_RADIUS, rcBalloon.bottom);

	// Bottom Side
	BalloonSide(hdc, Balloon::Down == eDir ? Balloon::Down : Balloon::None, x, y,
		rcBalloon.left + BALLOON_RADIUS, rcBalloon.bottom,
		rcBalloon.right - BALLOON_RADIUS, rcBalloon.bottom);

	// Bottom Right
	ArcTo(hdc,
		rcBalloon.right - 2 * BALLOON_RADIUS, rcBalloon.bottom - 2 * BALLOON_RADIUS, rcBalloon.right, rcBalloon.bottom,
		rcBalloon.right - BALLOON_RADIUS, rcBalloon.bottom, rcBalloon.right, rcBalloon.bottom - BALLOON_RADIUS);

	// Right Side
	BalloonSide(hdc, Balloon::Right == eDir ? Balloon::Right : Balloon::None, x, y,
		rcBalloon.right, rcBalloon.bottom - BALLOON_RADIUS,
		rcBalloon.right, rcBalloon.top + BALLOON_RADIUS);

	// Top Right
	ArcTo(hdc,
		rcBalloon.right - 2 * BALLOON_RADIUS, rcBalloon.top, rcBalloon.right, rcBalloon.top + BALLOON_RADIUS * 2,
		rcBalloon.right, rcBalloon.top + BALLOON_RADIUS, rcBalloon.right - BALLOON_RADIUS, rcBalloon.top);

	// Top Side
	BalloonSide(hdc, Balloon::Up == eDir ? Balloon::Up : Balloon::None, x, y,
		rcBalloon.right - BALLOON_RADIUS, rcBalloon.top,
		rcBalloon.left + BALLOON_RADIUS, rcBalloon.top);

	CloseFigure(hdc);
	EndPath(hdc);

	HPEN hpnTemp = (HPEN)SelectObject(hdc, CreatePen(PS_SOLID, 2, RGB(200, 200, 130)));
	HBRUSH hbrTemp = (HBRUSH)SelectObject(hdc, CreateSolidBrush(RGB(220, 220, 160)));
	StrokeAndFillPath(hdc);
	DeleteObject(SelectObject(hdc, hbrTemp));
	DeleteObject(SelectObject(hdc, hpnTemp));

	prc->left = rcBalloon.left + BALLOON_RADIUS;
	prc->top = rcBalloon.top + BALLOON_RADIUS;
	prc->right = rcBalloon.right - BALLOON_RADIUS;
	prc->bottom = rcBalloon.left - BALLOON_RADIUS;
}

///////////////////////////////////////////////////////////////////////////////
// CActorDef
///////////////////////////////////////////////////////////////////////////////

CActorDef::CActorDef (RSTRING rstrName) :
	m_pParent(NULL),
	m_rstrParent(NULL),
	m_rstrDef(NULL),
	m_idActor(0),
	m_fMonster(false)
{
	RStrSet(m_rstrName, rstrName);
}

CActorDef::~CActorDef ()
{
	RStrRelease(m_rstrParent);
	RStrRelease(m_rstrName);
	RStrRelease(m_rstrDef);
}

///////////////////////////////////////////////////////////////////////////////
// CBlockMapEditorApp
///////////////////////////////////////////////////////////////////////////////

CBlockMapEditorApp::CBlockMapEditorApp (HINSTANCE hInstance) :
	m_hInstance(hInstance),
	m_pRibbon(NULL),
	m_rstrFile(NULL),
	m_pBlockMap(NULL),
	m_fPainting(FALSE),
	m_xInfo(0), m_zInfo(0),
	m_fInfoBalloon(FALSE),
	m_idInfoTimer(0),
	m_pWall(NULL),
	m_pActor(NULL),
	m_pSelection(NULL)
{
	m_Graph.SetGraphType(GRAPH_XZ);
	m_Graph.SetGridType(GRID_AXIS_POINTS);
	m_Graph.AttachContainer(this);
	m_Graph.SetGraphTarget(this);

	m_Graph.SetBGColor(RGB(96, 96, 96));
}

CBlockMapEditorApp::~CBlockMapEditorApp ()
{
	ClearPaintItems();

	SafeRelease(m_pWall);
	SafeRelease(m_pActor);
	SafeRelease(m_pSelection);

	m_mapExternal.DeleteAll();
	m_mapActors.DeleteAll();
	m_mapSprites.DeleteAll();
	m_mapTextures.DeleteAll();

	m_Graph.AttachContainer(NULL);

	RStrRelease(m_rstrFile);
	SafeDelete(m_pBlockMap);
	SafeRelease(m_pRibbon);
}

HRESULT CBlockMapEditorApp::Register (HINSTANCE hInstance)
{
	WNDCLASSEX wnd = {0};

	wnd.cbSize = sizeof(WNDCLASSEX);
	wnd.style = CS_HREDRAW | CS_VREDRAW;
	wnd.cbClsExtra = 0;
	wnd.cbWndExtra = 0;
	wnd.hInstance = hInstance;
	wnd.hIcon = LoadIcon(hInstance, MAKEINTRESOURCEW(IDI_MAIN));
	wnd.hCursor = LoadCursor(NULL, IDC_ARROW);
	wnd.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wnd.lpszMenuName = NULL;
	wnd.lpszClassName = c_wzAppClassName;

	return RegisterClass(&wnd,NULL);
}

HRESULT CBlockMapEditorApp::Initialize (LPWSTR lpCmdLine, INT nCmdShow)
{
	HRESULT hr;
	CDialogHost dlgHost(m_hInstance);
	CLoaderDlg dlgLoader(this);

	Check(m_dlgConfig.Load(c_wzAppKey));
	Check(dlgHost.Display(GetDesktopWindow(), &m_dlgConfig));
	CheckIfIgnore(IDOK != dlgHost.GetReturnValue(), E_ABORT);
	Check(m_dlgConfig.Save(c_wzAppKey));

	Check(dlgHost.Display(GetDesktopWindow(), &dlgLoader));
	Check((HRESULT)dlgHost.GetReturnValue());

	Check(CSIFRibbon::Create(DPI::Scale, &m_pRibbon));
	Check(CreateBlockMap(&m_pBlockMap));
	Check(InitializePaintItems(m_pBlockMap));

	Check(Create(WS_EX_APPWINDOW | WS_EX_WINDOWEDGE, WS_OVERLAPPEDWINDOW, c_wzAppClassName, c_wzAppTitle, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, nCmdShow));
	Check(UpdateAppTitle());

Cleanup:
	return hr;
}

HRESULT CBlockMapEditorApp::AddFloorItem (USHORT nFloor)
{
	HRESULT hr;
	WCHAR wzFloor[16];
	TStackRef<CFloorItem> srItem;

	Check(Formatting::TPrintF(wzFloor, ARRAYSIZE(wzFloor), NULL, L"Floor %d", nFloor));
	Check(CFloorItem::Create(m_pRibbon, nFloor, wzFloor, &srItem));
	Check(RegisterPaintItem(srItem, PAINT_ITEM::Special));

Cleanup:
	return hr;
}

VOID CBlockMapEditorApp::ClearPaintItems (VOID)
{
	for(sysint i = 0; i < m_aPaintItems.Length(); i++)
		m_aPaintItems[i].pItem->Release();
	m_aPaintItems.Clear();
}

// IUIApplication

HRESULT STDMETHODCALLTYPE CBlockMapEditorApp::OnViewChanged (UINT32 viewId, UI_VIEWTYPE typeID, IUnknown* view, UI_VIEWVERB verb, INT32 uReasonCode)
{
	HRESULT hr = S_OK;

	switch(verb)
	{
	case UI_VIEWVERB_CREATE:
		break;
	case UI_VIEWVERB_DESTROY:
		break;
	case UI_VIEWVERB_SIZE:
		m_rcClient.top = m_pRibbon->GetHeight();
		SizeWindow();
		break;
	}

	return hr;
}

HRESULT STDMETHODCALLTYPE CBlockMapEditorApp::OnCreateUICommand (UINT32 commandId, UI_COMMANDTYPE typeID, IUICommandHandler** commandHandler)
{
	*commandHandler = this;
	AddRef();
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CBlockMapEditorApp::OnDestroyUICommand (UINT32 commandId, UI_COMMANDTYPE typeID, IUICommandHandler* commandHandler)
{
	return S_OK;
}

// IRibbonHost

HRESULT WINAPI CBlockMapEditorApp::GetRibbonSettingsKey (__out_ecount(cchMaxKey) PSTR pszKeyName, INT cchMaxKey)
{
	return TStrCchCpy(pszKeyName, cchMaxKey, "\\Software\\Simbey\\InfiniteWolfenstein");
}

HRESULT WINAPI CBlockMapEditorApp::GetRibbonSettingsValue (__out_ecount(cchMaxValue) PSTR pszValueName, INT cchMaxValue)
{
	return TStrCchCpy(pszValueName, cchMaxValue, "RibbonSettings");
}

HRESULT WINAPI CBlockMapEditorApp::GetRibbonResource (__out HMODULE* phModule, __out_ecount(cchMaxResource) PWSTR pwzResource, INT cchMaxResource)
{
	*phModule = m_hInstance;
	return TStrCchCpy(pwzResource, cchMaxResource, L"APPLICATION_RIBBON");
}

UINT WINAPI CBlockMapEditorApp::TranslateGroupToImage (UINT nID)
{
	// Use the tool-generated MapGroupsToImages() function.
	return MapGroupsToImages(nID);
}

UINT WINAPI CBlockMapEditorApp::TranslateImageToLargeImage (UINT nID)
{
	// Use the tool-generated MapImagesToLargeImages() function.
	return MapImagesToLargeImages(nID);
}

// IUICommandHandler

HRESULT STDMETHODCALLTYPE CBlockMapEditorApp::Execute (UINT32 commandId, UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* currentValue, IUISimplePropertySet* commandExecutionProperties)
{
	HRESULT hr = S_OK;

	if(UI_EXECUTIONVERB_EXECUTE == verb)
	{
		switch(commandId)
		{
		case ID_NEW:
			SafeDelete(m_pBlockMap);
			RStrRelease(m_rstrFile); m_rstrFile = NULL;
			Check(CreateBlockMap(&m_pBlockMap));
			Check(InitializePaintItems(m_pBlockMap));
			Check(UpdateAppTitle());
			Check(Invalidate(FALSE));
			Check(m_pRibbon->InvalidateEnabled());
			break;
		case ID_OPEN:
			CheckNoTrace(OpenMap());
			break;
		case ID_SAVE:
			CheckNoTrace(SaveMap());
			break;
		case ID_PROPERTIES:
			CheckNoTrace(ShowProperties());
			break;
		case ID_EXPORT:
			CheckNoTrace(ExportMap());
			break;
		case ID_EXIT:
			PostMessage(m_hwnd, WM_CLOSE, 0, 0);
			break;
		case ID_WALLS:
			Check(SelectPaintItem(currentValue, ID_WALLS, &m_pWall));
			break;
		case ID_OBJECTS:
			Check(SelectPaintItem(currentValue, ID_OBJECTS, &m_pActor));
			break;
		case ID_PAINT_TYPE:
			Check(SelectPaintItem(currentValue, ID_PAINT_TYPE, NULL));
			break;
		case ID_ADD_FLOOR:
			Check(AddFloorItem(m_pBlockMap->AddFloor()));

			ReplaceInterface(m_pSelection, m_aPaintItems[m_aPaintItems.Length() - 1].pItem);

			m_pRibbon->UpdateProperty(ID_PAINT_TYPE, &UI_PKEY_ItemsSource);
			m_pRibbon->UpdateProperty(ID_PAINT_TYPE, &UI_PKEY_LargeImage);
			break;
		}
	}

Cleanup:
	return hr;
}

HRESULT STDMETHODCALLTYPE CBlockMapEditorApp::UpdateProperty (UINT32 commandId, REFPROPERTYKEY key, const PROPVARIANT* currentValue, PROPVARIANT* newValue)
{
	HRESULT hr = E_NOTIMPL;

	if(UI_PKEY_LargeImage == key || UI_PKEY_SmallImage == key)
		hr = m_pRibbon->LoadImageForCommand(commandId, key, newValue);

	if(FAILED(hr))
	{
		if(UI_PKEY_Enabled == key)
		{
			//switch(commandId)
			newValue->boolVal = VARIANT_TRUE;
			newValue->vt = VT_BOOL;
			hr = S_OK;
		}
		else if(UI_PKEY_ItemsSource == key)
		{
			TStackRef<IUICollection> srItems;

			Check(currentValue->punkVal->QueryInterface(&srItems));
			srItems->Clear();

			if(ID_WALLS == commandId)
			{
				for(sysint i = 0; i < m_aPaintItems.Length(); i++)
				{
					if(m_aPaintItems[i].eType == PAINT_ITEM::Wall)
						Check(srItems->Add(static_cast<IUISimplePropertySet*>(m_aPaintItems[i].pItem)));
				}
			}
			else if(ID_OBJECTS == commandId)
			{
				for(sysint i = 0; i < m_aPaintItems.Length(); i++)
				{
					if(m_aPaintItems[i].eType == PAINT_ITEM::Object)
						Check(srItems->Add(static_cast<IUISimplePropertySet*>(m_aPaintItems[i].pItem)));
				}
			}
			else if(ID_PAINT_TYPE == commandId)
			{
				for(sysint i = 0; i < m_aPaintItems.Length(); i++)
				{
					if(m_aPaintItems[i].eType == PAINT_ITEM::Special)
						Check(srItems->Add(static_cast<IUISimplePropertySet*>(m_aPaintItems[i].pItem)));
				}
				if(m_pWall)
					Check(srItems->Add(static_cast<IUISimplePropertySet*>(m_pWall)));
				if(m_pActor)
					Check(srItems->Add(static_cast<IUISimplePropertySet*>(m_pActor)));
			}
		}
		else if(ID_PAINT_TYPE == commandId && (UI_PKEY_LargeImage == key || UI_PKEY_SmallImage == key))
		{
			if(m_pSelection)
				Check(m_pSelection->GetValue(key, newValue));
		}
	}

Cleanup:
	return hr;
}

// IGraphContainer

HRESULT WINAPI CBlockMapEditorApp::SetFocus (__in IGrapher* pGraphCtrl)
{
	::SetFocus(m_hwnd);
	return S_OK;
}

HRESULT WINAPI CBlockMapEditorApp::ScreenToWindowless (__in IGrapher* pGraphCtrl, __inout PPOINT ppt)
{
	if(ScreenToClient(m_hwnd, ppt))
		return S_OK;
	return E_FAIL;
}

HRESULT WINAPI CBlockMapEditorApp::InvalidateContainer (__in IGrapher* pGraphCtrl)
{
	return Invalidate(FALSE);
}

VOID WINAPI CBlockMapEditorApp::DrawDib24 (HDC hdcDest, LONG x, LONG y, HDC hdcSrc, const BYTE* pcDIB24, LONG xSize, LONG ySize, LONG lPitch)
{
	BitBlt(hdcDest, x, y, xSize, ySize, hdcSrc, 0, 0, SRCCOPY);
}

BOOL WINAPI CBlockMapEditorApp::CaptureMouse (__in IGrapher* pGraphCtrl, BOOL fCapture)
{
	if(fCapture)
	{
		SetCapture(m_hwnd);
		return TRUE;
	}
	else
		return ReleaseCapture();
}

// IGraphClient

VOID CBlockMapEditorApp::onGraphPaint (IGrapher* lpGraph)
{
	m_pBlockMap->Paint(lpGraph);

	if(m_fInfoBalloon)
	{
		TStackRef<CPaintItem> srBlock, srObject;
		INT xCell = m_xInfo + MAP_WIDTH / 2;
		INT zCell = m_zInfo + MAP_HEIGHT / 2;

		if(SUCCEEDED(m_pBlockMap->GetCellData(xCell, zCell, &srBlock, &srObject)) && (srBlock || srObject))
		{
			HDC hdc = lpGraph->GetClientDC();
			FLOAT x = static_cast<FLOAT>(m_xInfo * 16) + 8.0f;
			FLOAT z = static_cast<FLOAT>(m_zInfo * 16);
			INT xClient, yClient;
			Balloon::Dir eDir;
			RECT rcClient = { 0, 0, m_rcClient.right - m_rcClient.left, m_rcClient.bottom - m_rcClient.top };

			LOGFONT logFont = {0};
			SideAssertHr(TStrCchCpy(logFont.lfFaceName, ARRAYSIZE(logFont.lfFaceName), L"Arial"));
			logFont.lfHeight = 14;
			logFont.lfWeight = FW_NORMAL;
			HFONT hOldFont = (HFONT)SelectObject(hdc, CreateFontIndirect(&logFont));

			m_Graph.GraphToClient(x, z, xClient, yClient);
			if(yClient < rcClient.bottom / 2)
			{
				z += 16.0f;
				eDir = Balloon::Up;
			}
			else
				eDir = Balloon::Down;
			m_Graph.GraphToClient(x, z, xClient, yClient);

			RECT rc;
			PaintBalloon(hdc, eDir, &rcClient, xClient, yClient, srObject ? 160 : 80, 80, &rc);

			if(srBlock)
				DrawInfoPart(hdc, srBlock, rc.left + 8, rc.top);
			if(srObject)
				DrawInfoPart(hdc, srObject, rc.left + 88, rc.top);

			DeleteObject(SelectObject(hdc, hOldFont));
		}
	}
}

VOID CBlockMapEditorApp::onGraphMouseMove (DWORD dwKeys, FLOAT x, FLOAT y)
{
	if(m_fPainting)
		SetCellData(x, y);
	else
	{
		INT xInfo = static_cast<INT>(x / CELL_SCALE);
		if(x < 0.0f)
			xInfo--;
		INT zInfo = static_cast<INT>(y / CELL_SCALE);
		if(y < 0.0f)
			zInfo--;

		if(xInfo != m_xInfo || zInfo != m_zInfo)
		{
			TStackRef<CPaintItem> srBlock, srObject;

			if(m_fInfoBalloon)
			{
				m_fInfoBalloon = FALSE;
				InvalidateRect(m_hwnd, NULL, FALSE);
			}

			m_xInfo = xInfo;
			m_zInfo = zInfo;

			INT xCell = xInfo + MAP_WIDTH / 2;
			INT zCell = zInfo + MAP_HEIGHT / 2;

			if(SUCCEEDED(m_pBlockMap->GetCellData(xCell, zCell, &srBlock, &srObject)) && (srBlock || srObject))
			{
				if(m_idInfoTimer)
				{
					UnregisterTimer(m_idInfoTimer);
					m_idInfoTimer = 0;
				}

				RegisterTimer(1500, &m_idInfoTimer);
			}
		}
	}
}

VOID CBlockMapEditorApp::onGraphLBtnDown (DWORD dwKeys, FLOAT x, FLOAT y)
{
	if(m_fInfoBalloon)
	{
		m_fInfoBalloon = FALSE;
		InvalidateRect(m_hwnd, NULL, FALSE);
	}

	if(m_idInfoTimer)
	{
		UnregisterTimer(m_idInfoTimer);
		m_idInfoTimer = 0;
	}

	if(dwKeys & MK_CONTROL)
	{
		TStackRef<CPaintItem> srBlock, srObject;

		x /= CELL_SCALE;
		y /= CELL_SCALE;

		x = floor(x);
		y = floor(y);

		INT xCell = (INT)x + MAP_WIDTH / 2;
		INT yCell = (INT)y + MAP_HEIGHT / 2;
		SHORT sObjectFlags;

		if(SUCCEEDED(m_pBlockMap->GetCellData(xCell, yCell, &srBlock, &srObject)) && srObject)
		{
			if(SUCCEEDED(m_pBlockMap->GetObjectFlags(xCell, yCell, &sObjectFlags)))
			{
				CDialogHost dlgHost(m_hInstance);
				CDifficultyDlg dlgDifficulty(sObjectFlags);

				if(SUCCEEDED(dlgHost.Display(m_hwnd, &dlgDifficulty)) && IDOK == dlgHost.GetReturnValue())
					m_pBlockMap->SetObjectFlags(xCell, yCell, dlgDifficulty.GetObjectFlags());
			}
		}
	}
	else if(SUCCEEDED(SetCellData(x, y)))
	{
		CaptureMouse(&m_Graph, TRUE);
		m_fPainting = TRUE;
	}
}

VOID CBlockMapEditorApp::onGraphLBtnUp (DWORD dwKeys, FLOAT x, FLOAT y)
{
	if(m_fPainting)
	{
		m_fPainting = FALSE;
		CaptureMouse(&m_Graph, FALSE);
	}
}

BOOL CBlockMapEditorApp::onGraphRBtnDown (DWORD dwKeys, FLOAT x, FLOAT y)
{
	return FALSE;
}

VOID CBlockMapEditorApp::onGraphRBtnUp (DWORD dwKeys, FLOAT x, FLOAT y)
{
}

VOID CBlockMapEditorApp::onGraphLBtnDbl (DWORD dwKeys, FLOAT x, FLOAT y)
{
}

VOID CBlockMapEditorApp::onGraphRBtnDbl (DWORD dwKeys, FLOAT x, FLOAT y)
{
}

VOID CBlockMapEditorApp::onGraphViewChanged (BOOL fZoomChanged)
{
}

BOOL CBlockMapEditorApp::onGraphKeyDown (WPARAM iKey)
{
	return FALSE;
}

BOOL CBlockMapEditorApp::onGraphKeyUp (WPARAM iKey)
{
	return FALSE;
}

BOOL CBlockMapEditorApp::onGraphChar (WPARAM iKey)
{
	return FALSE;
}

BOOL CBlockMapEditorApp::onGraphWheel (SHORT sDistance, FLOAT x, FLOAT y)
{
	return FALSE;
}

HRESULT CBlockMapEditorApp::onGraphGetAcc (IAccessible** lplpAccessible)
{
	return E_NOTIMPL;
}

// ILoaderThread

HRESULT CBlockMapEditorApp::LoaderCallback (HWND hwndStatus)
{
	HRESULT hr;

	Check(LoadPackage(hwndStatus, m_dlgConfig.m_wzTexturePath));

	SendMessage(hwndStatus, WM_SETTEXT, 0, (LPARAM)L"Loading external things...");
	Check(LoadExternal(m_dlgConfig.m_wzTexturePath));

Cleanup:
	return hr;
}

// IResolveItemPalette

HRESULT CBlockMapEditorApp::InitializePaintItems (CBlockMap* pMap)
{
	HRESULT hr;
	TStackRef<CVoidItem> srVoidItem;
	TStackRef<CDoorObject> srDoor;
	TStackRef<CElevatorSwitch> srElevator;
	TStackRef<CStartItem> srStart;
	TStackRef<CSecretDoor> srSecret;
	TStackRef<CEndSpot> srEndSpot;
	TStackRef<CSkyLight> srSkyLight;
	TEXTURE* pTexture;

	ClearPaintItems();

	SafeRelease(m_pWall);
	SafeRelease(m_pActor);

	for(sysint i = 0; i < m_mapTextures.Length(); i++)
	{
		TStackRef<CTextureItem> srItem;
		RSTRING rstrName;
		TEXTURE* pTexture;

		Check(m_mapTextures.GetKeyAndValue(i, &rstrName, &pTexture));
		Check(CTextureItem::Create(m_pRibbon, pTexture, &srItem));
		Check(srItem->SetItemText(RStrToWide(rstrName)));
		Check(RegisterPaintItem(srItem, PAINT_ITEM::Wall));
	}

	if(SUCCEEDED(m_mapTextures.Find(RSTRING_CAST(L"BLUESTN0"), &pTexture)))
	{
		TStackRef<CWallCage> srItem;

		Check(CWallCage::Create(m_pRibbon, pTexture, 0, &srItem));
		Check(srItem->SetItemText(L"Wall Cage"));
		Check(RegisterPaintItem(srItem, PAINT_ITEM::Wall));
	}

	if(SUCCEEDED(m_mapTextures.Find(RSTRING_CAST(L"BRWNSTN0"), &pTexture)))
	{
		TStackRef<CWallCage> srItem;

		Check(CWallCage::Create(m_pRibbon, pTexture, 1, &srItem));
		Check(srItem->SetItemText(L"Hidden Cage"));
		Check(RegisterPaintItem(srItem, PAINT_ITEM::Wall));
	}

	if(SUCCEEDED(m_mapTextures.Find(RSTRING_CAST(L"WOODWAL0"), &pTexture)))
	{
		TStackRef<CWallCage> srItem;

		Check(CWallCage::Create(m_pRibbon, pTexture, 2, &srItem));
		Check(srItem->SetItemText(L"Hidden Cabinet"));
		Check(RegisterPaintItem(srItem, PAINT_ITEM::Wall));
	}

	for(sysint i = 0; i < m_mapActors.Length(); i++)
	{
		TStackRef<CActorItem> srItem;
		RSTRING rstrName;
		ACTOR* pActor;

		Check(m_mapActors.GetKeyAndValue(i, &rstrName, &pActor));
		Check(CActorItem::Create(m_pRibbon, pActor, &srItem));
		Check(srItem->SetItemText(RStrToWide(rstrName)));
		Check(RegisterPaintItem(srItem, PAINT_ITEM::Object));
	}

	Check(CVoidItem::Create(m_pRibbon, &srVoidItem));
	Check(RegisterPaintItem(srVoidItem, PAINT_ITEM::Special));
	ReplaceInterface(m_pSelection, srVoidItem.StaticCast<CPaintItem>());
	srVoidItem.Release();

	Check(m_mapTextures.Find(RSTRING_CAST(L"DOORNORM"), &pTexture));

	Check(CDoorObject::Create(m_pRibbon, pMap, pTexture, CDoorObject::Normal, &srDoor));
	Check(RegisterPaintItem(srDoor, PAINT_ITEM::Special));
	srDoor.Release();

	Check(m_mapTextures.Find(RSTRING_CAST(L"LOCKDOOR"), &pTexture));

	Check(CDoorObject::Create(m_pRibbon, pMap, pTexture, CDoorObject::SilverKey, &srDoor));
	Check(RegisterPaintItem(srDoor, PAINT_ITEM::Special));
	srDoor.Release();

	Check(CDoorObject::Create(m_pRibbon, pMap, pTexture, CDoorObject::GoldKey, &srDoor));
	Check(RegisterPaintItem(srDoor, PAINT_ITEM::Special));
	srDoor.Release();

	Check(CDoorObject::Create(m_pRibbon, pMap, pTexture, CDoorObject::RubyKey, &srDoor));
	Check(RegisterPaintItem(srDoor, PAINT_ITEM::Special));
	srDoor.Release();

	Check(m_mapTextures.Find(RSTRING_CAST(L"ELVRDOOR"), &pTexture));
	Check(CDoorObject::Create(m_pRibbon, pMap, pTexture, CDoorObject::Elevator, &srDoor));
	Check(RegisterPaintItem(srDoor, PAINT_ITEM::Special));
	srDoor.Release();

	Check(m_mapTextures.Find(RSTRING_CAST(L"ELVRDOWN"), &pTexture));

	Check(CElevatorSwitch::Create(m_pRibbon, pTexture, false /* normal */, &srElevator));
	Check(RegisterPaintItem(srElevator, PAINT_ITEM::Special));
	srElevator.Release();

	Check(CElevatorSwitch::Create(m_pRibbon, pTexture, true /* secret */, &srElevator));
	Check(RegisterPaintItem(srElevator, PAINT_ITEM::Special));
	srElevator.Release();

	Check(CSecretDoor::Create(m_pRibbon, 0, &srSecret));
	Check(RegisterPaintItem(srSecret, PAINT_ITEM::Special));
	srSecret.Release();

	Check(CSecretDoor::Create(m_pRibbon, 1, &srSecret));
	Check(RegisterPaintItem(srSecret, PAINT_ITEM::Special));
	srSecret.Release();

	Check(CStartItem::Create(m_pRibbon, 90, &srStart));
	Check(RegisterPaintItem(srStart, PAINT_ITEM::Special));
	srStart.Release();

	Check(CStartItem::Create(m_pRibbon, 0, &srStart));
	Check(RegisterPaintItem(srStart, PAINT_ITEM::Special));
	srStart.Release();

	Check(CStartItem::Create(m_pRibbon, 270, &srStart));
	Check(RegisterPaintItem(srStart, PAINT_ITEM::Special));
	srStart.Release();

	Check(CStartItem::Create(m_pRibbon, 180, &srStart));
	Check(RegisterPaintItem(srStart, PAINT_ITEM::Special));
	srStart.Release();

	Check(CEndSpot::Create(m_pRibbon, &srEndSpot));
	Check(RegisterPaintItem(srEndSpot, PAINT_ITEM::Special));
	srEndSpot.Release();

	Check(CSkyLight::Create(m_pRibbon, &srSkyLight));
	Check(RegisterPaintItem(srSkyLight, PAINT_ITEM::Special));
	srSkyLight.Release();

	// TODO - add more painting items here

	INT nMaxFloor = pMap->GetHighestFloor();
	for(INT i = 0; i < nMaxFloor; i++)
		Check(AddFloorItem(i + 1));

	m_pRibbon->UpdateProperty(ID_WALLS, &UI_PKEY_ItemsSource);
	m_pRibbon->UpdateProperty(ID_OBJECTS, &UI_PKEY_ItemsSource);
	m_pRibbon->UpdateProperty(ID_PAINT_TYPE, &UI_PKEY_ItemsSource);

	m_pRibbon->UpdateProperty(ID_PAINT_TYPE, &UI_PKEY_LargeImage);

Cleanup:
	return hr;
}

HRESULT CBlockMapEditorApp::ResolveItemPalette (MapCell::Type eType, const BYTE* pcb, DWORD cb, __deref_out CPaintItem** ppItem)
{
	for(sysint i = 0; i < m_aPaintItems.Length(); i++)
	{
		PAINT_ITEM& item = m_aPaintItems[i];
		if(item.pItem->GetType() == eType && item.pItem->Deserialize(pcb, cb))
		{
			SetInterface(*ppItem, item.pItem);
			return S_OK;
		}
	}

	return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
}

VOID CBlockMapEditorApp::SizeWindow (VOID)
{
	m_Graph.Move(&m_rcClient);
}

VOID CBlockMapEditorApp::PaintWindow (HDC hdc, const RECT* prc)
{
	m_Graph.Paint(hdc);
}

HRESULT CBlockMapEditorApp::CreateBlockMap (__deref_out CBlockMap** ppBlockMap)
{
	HRESULT hr;
	CBlockMap* pBlockMap = __new CBlockMap;

	CheckAlloc(pBlockMap);
	Check(pBlockMap->Initialize());
	*ppBlockMap = pBlockMap;
	pBlockMap = NULL;

Cleanup:
	__delete pBlockMap;
	return hr;
}

HRESULT CBlockMapEditorApp::SelectPaintItem (const PROPVARIANT* currentValue, UINT32 commandId, CPaintItem** ppItem)
{
	HRESULT hr;
	TStackRef<IUICollection> srItems;
	TStackRef<IUnknown> srUnk;
	TStackRef<CPaintItem> srItem;
	PROPVARIANT value;

	value.vt = VT_EMPTY;
	value.punkVal = NULL;

	Check(m_pRibbon->ReadProperty(commandId, UI_PKEY_ItemsSource, &value));

	Check(value.punkVal->QueryInterface(&srItems));
	Check(srItems->GetItem(currentValue->uintVal, &srUnk));
	Check(srUnk.QueryInterface(&srItem));
	if(ppItem)
	{
		SafeRelease(*ppItem);
		*ppItem = srItem.Detach();
		ReplaceInterface(m_pSelection, *ppItem);

		m_pRibbon->UpdateProperty(ID_PAINT_TYPE, &UI_PKEY_ItemsSource);
	}
	else
		ReplaceInterface(m_pSelection, srItem.StaticCast<CPaintItem>());

	m_pRibbon->UpdateProperty(ID_PAINT_TYPE, &UI_PKEY_LargeImage);

Cleanup:
	SafeRelease(value.punkVal);
	return hr;
}

HRESULT CBlockMapEditorApp::RegisterPaintItem (CPaintItem* pItem, PAINT_ITEM::Type eType)
{
	PAINT_ITEM* pSlot;
	HRESULT hr = m_aPaintItems.AppendSlot(&pSlot);
	if(SUCCEEDED(hr))
	{
		SetInterface(pSlot->pItem, pItem);
		pSlot->eType = eType;
	}
	return hr;
}

HRESULT CBlockMapEditorApp::AddFakeSpriteTexture (RSTRING rstrName, __deref_out TEXTURE** ppTexture)
{
	HRESULT hr;
	TEXTURE* pTexture = __new TEXTURE;
	PBYTE pbTemp;

	CheckAlloc(pTexture);
	pTexture->pcwzName = RStrToWide(rstrName);
	pTexture->crAverage = RGB(255, 255, 0);
	pTexture->xSize = 64;
	pTexture->ySize = 64;
	Check(pTexture->stmBits32.WriteAdvance(&pbTemp, 64 * 64 * 32));
	ZeroMemory(pbTemp, 64 * 64 * 32);
	Check(m_mapSprites.Add(rstrName, pTexture));
	*ppTexture = pTexture;

Cleanup:
	if(FAILED(hr))
		__delete pTexture;
	return hr;
}

HRESULT CBlockMapEditorApp::OpenMap (VOID)
{
	HRESULT hr;
	CChooseFile choose;
	RSTRING rstrFile = NULL;
	CBlockMap* pBlockMap = NULL;

	Check(choose.Initialize());
	CheckIfIgnore(!choose.OpenSingleFile(m_hwnd, L"Block Map (*.map)\0*.map\0\0"), E_ABORT);

	PCWSTR pcwzFile = choose.GetFile(0);
	Check(RStrCreateW(TStrLenAssert(pcwzFile), pcwzFile, &rstrFile));

	Check(CreateBlockMap(&pBlockMap));
	Check(pBlockMap->Load(this, pcwzFile, m_dlgConfig.m_wzCeilingName, m_dlgConfig.m_wzFloorName));

	SwapData(m_pBlockMap, pBlockMap);
	SwapData(m_rstrFile, rstrFile);

	Check(UpdateAppTitle());
	Check(Invalidate(FALSE));
	Check(m_pRibbon->InvalidateEnabled());

Cleanup:
	__delete pBlockMap;
	RStrRelease(rstrFile);
	return hr;
}

HRESULT CBlockMapEditorApp::SaveMap (VOID)
{
	HRESULT hr;
	RSTRING rstrFile = NULL;

	if(NULL == m_rstrFile)
	{
		CChooseFile choose;
		Check(choose.Initialize());
		CheckIfIgnore(!choose.SaveFile(m_hwnd, L"Map (*.map)\0*.map\0\0"), E_ABORT);

		PCWSTR pcwzFile = choose.GetFile(0);
		Check(RStrCreateW(TStrLenAssert(pcwzFile), pcwzFile, &rstrFile));

		Check(m_pBlockMap->Save(pcwzFile, m_dlgConfig.m_wzCeilingName, m_dlgConfig.m_wzFloorName));

		m_rstrFile = rstrFile;
		rstrFile = NULL;
	}
	else
		Check(m_pBlockMap->Save(RStrToWide(m_rstrFile), m_dlgConfig.m_wzCeilingName, m_dlgConfig.m_wzFloorName));

	Check(UpdateAppTitle());

Cleanup:
	RStrRelease(rstrFile);
	return hr;
}

HRESULT CBlockMapEditorApp::ShowProperties (VOID)
{
	HRESULT hr;
	CDialogHost dlgHost(m_hInstance);
	CPropertiesDlg dlgProperties(&m_dlgConfig, m_pBlockMap->GetLighting(), m_mapTextures);

	Check(dlgHost.Display(m_hwnd, &dlgProperties));
	CheckIfIgnore(IDOK != dlgHost.GetReturnValue(), E_ABORT);
	m_pBlockMap->SetLighting(dlgProperties.GetLighting());

Cleanup:
	return hr;
}

HRESULT CBlockMapEditorApp::ExportMap (VOID)
{
	HRESULT hr;
	CDialogHost dlgHost(m_hInstance);
	CExportDlg dlgExport;
	PCWSTR pcwzName, pcwzExt;
	WCHAR wzLevel[12];

	if(NULL == m_rstrFile)
	{
		// Just use "MAP01" if the map hasn't yet been saved.
		pcwzName = L"MAP01";
	}
	else
	{
		pcwzName = TStrRChr(RStrToWide(m_rstrFile), L'\\');
		CheckIf(NULL == pcwzName, E_UNEXPECTED);
		pcwzName++;
	}

	pcwzExt = TStrChr(pcwzName, L'.');
	if(pcwzExt)
		Check(TStrCchCpyN(wzLevel, ARRAYSIZE(wzLevel), pcwzName, static_cast<INT>(pcwzExt - pcwzName)));
	else
		Check(TStrCchCpy(wzLevel, ARRAYSIZE(wzLevel), pcwzName));

	Check(dlgExport.Initialize(m_pBlockMap, pcwzName, &m_dlgConfig));
	Check(dlgHost.Display(m_hwnd, &dlgExport));
	CheckIf(IDOK != dlgHost.GetReturnValue(), E_ABORT);

Cleanup:
	return hr;
}

HRESULT CBlockMapEditorApp::LoadPackage (HWND hwndStatus, PCWSTR pcwzPackage)
{
	HRESULT hr;
	TStackRef<IZipFS> srStandardFS;
	TStackRef<ISimbeyUnzip> srPackage;
	PWSTR pwzText = NULL, pwzDeco = NULL;
	INT cchText, cchDeco;

	SendMessage(hwndStatus, WM_SETTEXT, 0, (LPARAM)L"Opening package...");

	Check(szCreateStandardFS(&srStandardFS));
	Check(szUnzipOpen(srStandardFS, pcwzPackage, &srPackage));

	SendMessage(hwndStatus, WM_SETTEXT, 0, (LPARAM)L"Loading textures...");
	Check(LoadTextures(srPackage, SLP(c_wzTexturesSlash), _LoadTextures, &m_mapTextures));

	SendMessage(hwndStatus, WM_SETTEXT, 0, (LPARAM)L"Loading sprites...");
	Check(LoadTextures(srPackage, SLP(c_wzSpritesSlash), _LoadSprites, &m_mapSprites));

	Check(LoadTextEntry(srPackage, L"TEXTURES", &pwzText, &cchText));
	Check(LoadTextEntry(srPackage, L"DECORATE", &pwzDeco, &cchDeco));

	SendMessage(hwndStatus, WM_SETTEXT, 0, (LPARAM)L"Parsing decorate script...");
	Check(LoadActors(pwzText, pwzDeco));

Cleanup:
	__delete_array pwzText;
	__delete_array pwzDeco;
	return hr;
}

HRESULT CBlockMapEditorApp::LoadActors (PCWSTR pcwzText, PCWSTR pcwzDeco)
{
	HRESULT hr = S_FALSE;
	TRStrMap<CActorDef*> mapActors;
	RSTRING rstrName = NULL;
	PCWSTR pcwzToken;
	INT cchToken;
	CActorDef* pActorDef = NULL;
	bool fDeleteActorDef = false;
	ACTOR* pActor = NULL;

	// Use a very basic parsing system to build a hierarchy of actor definitions.
	while(ReadToken(pcwzDeco, &pcwzToken, &cchToken))
	{
		if(0 == TStrICmpNAssert(pcwzToken, SLP(L"ACTOR")))
		{
			PCWSTR pcwzDef;
			INT cGroups = 1;

			CheckIf(!ReadToken(pcwzDeco, &pcwzToken, &cchToken), E_FAIL);
			Check(RStrCreateW(cchToken, pcwzToken, &rstrName));

			pActorDef = __new CActorDef(rstrName);
			CheckAlloc(pActorDef);
			fDeleteActorDef = true;

			Check(mapActors.Add(rstrName, pActorDef));
			fDeleteActorDef = false;

			CheckIf(!ReadToken(pcwzDeco, &pcwzToken, &cchToken), E_FAIL);
			if(1 == cchToken && L':' == *pcwzToken)
			{
				CheckIf(!ReadToken(pcwzDeco, &pcwzToken, &cchToken), E_FAIL);
				Check(RStrCreateW(cchToken, pcwzToken, &pActorDef->m_rstrParent));

				// It's not an error if the parent doesn't exist in the decoration script.
				mapActors.Find(pActorDef->m_rstrParent, &pActorDef->m_pParent);
			}

			if(1 != cchToken || L'{' != *pcwzToken)
			{
				if(L'0' > *pcwzToken || L'9' < *pcwzToken)
					CheckIf(!ReadToken(pcwzDeco, &pcwzToken, &cchToken), E_FAIL);

				if(L'0' <= *pcwzToken && *pcwzToken <= L'9')
				{
					pActorDef->m_idActor = Formatting::TAscToInt32(pcwzToken);
					CheckIf(!ReadToken(pcwzDeco, &pcwzToken, &cchToken), E_FAIL);
				}

				CheckTrue(1 == cchToken && L'{' == *pcwzToken, E_FAIL);
			}

			pcwzDef = pcwzDeco;
			while(ReadToken(pcwzDeco, &pcwzToken, &cchToken))
			{
				if(1 == cchToken)
				{
					if(L'{' == *pcwzToken)
						cGroups++;
					else if(L'}' == *pcwzToken)
					{
						cGroups--;
						if(0 == cGroups)
							break;
					}
				}
				else if(0 == TStrICmpNAssert(pcwzToken, SLP(L"MONSTER")))
					pActorDef->m_fMonster = true;
			}
			CheckIf(0 != cGroups, E_FAIL);

			// If your parent is a monster, then you're also a monster.
			if(!pActorDef->m_fMonster && pActorDef->m_pParent && pActorDef->m_pParent->m_fMonster)
				pActorDef->m_fMonster = true;

			Check(RStrCreateW(static_cast<INT>(pcwzDeco - pcwzDef), pcwzDef, &pActorDef->m_rstrDef));

			RStrRelease(rstrName); rstrName = NULL;
		}
	}

	// Add the actors with IDs into the actor set.
	for(sysint i = 0; i < mapActors.Length(); i++)
	{
		bool fFoundSpawnSprite = false;

		pActorDef = *mapActors.GetValuePtr(i);
		if(0 == pActorDef->m_idActor)
			continue;

		CActorDef* pDefPtr = pActorDef;
		do
		{
			pcwzDeco = RStrToWide(pDefPtr->m_rstrDef);

			while(ReadToken(pcwzDeco, &pcwzToken, &cchToken))
			{
				if((0 == TStrICmpNAssert(pcwzToken, SLP(L"SPAWN")) || 0 == TStrICmpNAssert(pcwzToken, SLP(L"STATIC"))) &&
					ReadToken(pcwzDeco, &pcwzToken, &cchToken) &&
					1 == cchToken && L':' == *pcwzToken)
				{
					WCHAR wzSprite[8];
					HRESULT hrFindTexture;

					// Read the sprite name
					CheckIf(!ReadToken(pcwzDeco, &pcwzToken, &cchToken), E_FAIL);

					pActor = __new ACTOR;
					CheckAlloc(pActor);
					pActor->idActor = pActorDef->m_idActor;
					pActor->nDirection = 0;

					// Copy the sprite name (four characters)
					Check(TStrCchCpyN(wzSprite, 5, pcwzToken, cchToken));

					// Read the sprite direction
					CheckIf(!ReadToken(pcwzDeco, &pcwzToken, &cchToken), E_FAIL);

					wzSprite[4] = *pcwzToken;
					wzSprite[5] = L'0';
					wzSprite[6] = L'\0';

					hrFindTexture = FindSpriteTexture(pcwzText, wzSprite, &pActor->pTexture);
					if(HRESULT_FROM_WIN32(ERROR_NOT_FOUND) == hrFindTexture)
						Check(LoadExternalTexture(pActorDef->m_rstrName, &pActor->pTexture));
					else
						Check(hrFindTexture);

					if(pActorDef->m_fMonster)
					{
						Check(AddDirectionalActor(pActorDef->m_rstrName, 0, pActor, true));
						Check(AddDirectionalActor(pActorDef->m_rstrName, 90, pActor, false));
						Check(AddDirectionalActor(pActorDef->m_rstrName, 180, pActor, false));
						Check(AddDirectionalActor(pActorDef->m_rstrName, 270, pActor, false));
					}
					else
						Check(m_mapActors.Add(pActorDef->m_rstrName, pActor));

					pActor = NULL;
					fFoundSpawnSprite = true;

					break;
				}
			}

			if(fFoundSpawnSprite)
				break;

			pDefPtr = pDefPtr->m_pParent;
		} while(pDefPtr);
	}

Cleanup:
	if(fDeleteActorDef)
		__delete pActorDef;
	__delete pActor;
	RStrRelease(rstrName);
	mapActors.DeleteAll();
	return hr;
}

HRESULT CBlockMapEditorApp::LoadExternal (PCWSTR pcwzSearchPath)
{
	HRESULT hr;
	WCHAR wzThingsPath[MAX_PATH];
	ACTOR* pActor = NULL;
	RSTRING rstrName = NULL;
	PWSTR pwzThings = NULL;
	INT cchThingsPath, cchText;
	TStackRef<IJSONValue> srv;
	TStackRef<IJSONArray> srThings;

	CheckIf(FAILED(ScSearchPaths(L"THINGS.JSON", pcwzSearchPath, TStrLenChecked(pcwzSearchPath), 2, TRUE, wzThingsPath, ARRAYSIZE(wzThingsPath), &cchThingsPath)), S_FALSE);
	Check(Text::LoadFromFile(wzThingsPath, &pwzThings, &cchText));
	Check(JSONParse(NULL, pwzThings, cchText, &srv));
	Check(srv->GetArray(&srThings));

	for(sysint i = 0; i < srThings->Count(); i++)
	{
		TEXTURE* pTexture;
		TStackRef<IJSONObject> srThing;
		DWORD id;
		bool fDirectional;

		Check(srThings->GetObject(i, &srThing));

		srv.Release();
		Check(srThing->FindNonNullValueW(L"id", &srv));
		Check(srv->GetDWord(&id));

		srv.Release();
		Check(srThing->FindNonNullValueW(L"name", &srv));
		Check(srv->GetString(&rstrName));

		srv.Release();
		Check(srThing->FindNonNullValueW(L"directional", &srv));
		Check(srv->GetBoolean(&fDirectional));

		Check(AddFakeSpriteTexture(rstrName, &pTexture));

		pActor = __new ACTOR;
		CheckAlloc(pActor);

		pActor->idActor = id;
		pActor->nDirection = 0;
		pActor->pTexture = pTexture;

		if(fDirectional)
		{
			Check(AddDirectionalActor(rstrName, 0, pActor, true));
			Check(AddDirectionalActor(rstrName, 90, pActor, false));
			Check(AddDirectionalActor(rstrName, 180, pActor, false));
			Check(AddDirectionalActor(rstrName, 270, pActor, false));
		}
		else
			Check(m_mapActors.Add(rstrName, pActor));

		pActor = NULL;
		RStrRelease(rstrName); rstrName = NULL;
	}

Cleanup:
	RStrRelease(rstrName);
	__delete_array pwzThings;
	__delete pActor;
	return hr;
}

HRESULT CBlockMapEditorApp::FindSpriteTexture (PCWSTR pcwzText, PCWSTR pcwzSprite, __deref_out TEXTURE** ppTexture)
{
	HRESULT hr;
	PCWSTR pcwzLine = TStrIStr(pcwzText, pcwzSprite), pcwzGraphic, pcwzComma;
	RSTRING rstrTexture = NULL;

	if(NULL == pcwzLine)
	{
		WCHAR wzAltSprite[12];
		INT cchAlt = TStrLenAssert(pcwzSprite);

		CheckIf(pcwzSprite[cchAlt - 1] != L'0', E_FAIL);
		Check(TStrCchCpy(wzAltSprite, ARRAYSIZE(wzAltSprite), pcwzSprite));
		wzAltSprite[cchAlt - 1] = L'1';
		pcwzLine = TStrIStr(pcwzText, wzAltSprite);
		CheckIfIgnore(NULL == pcwzLine, HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
	}

	pcwzGraphic = TStrIStr(pcwzLine, L"Graphic");
	CheckIf(NULL == pcwzGraphic, E_FAIL);

	pcwzGraphic += 7;
	while(Formatting::IsSpace(*pcwzGraphic))
		pcwzGraphic++;

	pcwzComma = TStrChr(pcwzGraphic, L',');
	CheckIf(NULL == pcwzComma, E_FAIL);

	Check(RStrCreateW(static_cast<INT>(pcwzComma - pcwzGraphic), pcwzGraphic, &rstrTexture));
	Check(m_mapSprites.Find(rstrTexture, ppTexture));

Cleanup:
	RStrRelease(rstrTexture);
	return hr;
}

HRESULT CBlockMapEditorApp::AddDirectionalActor (RSTRING rstrName, INT nDirection, ACTOR* pActor, bool fUseThisActor)
{
	HRESULT hr;
	ACTOR* pDup = NULL;
	RSTRING rstrDirName = NULL;
	PCWSTR pcwzDir;

	switch(nDirection)
	{
	case 0:
		pcwzDir = L"E";
		break;
	case 270:
		pcwzDir = L"S";
		break;
	case 180:
		pcwzDir = L"W";
		break;
	case 90:
		pcwzDir = L"N";
		break;
	}

	Check(RStrFormatW(&rstrDirName, L"%r %ls", rstrName, pcwzDir));

	if(!fUseThisActor)
	{
		pDup = __new ACTOR;
		CheckAlloc(pDup);
		pDup->idActor = pActor->idActor;
		pDup->nDirection = pActor->nDirection;
		pDup->pTexture = pActor->pTexture;
		pActor = pDup;
	}

	pActor->nDirection = nDirection;
	Check(m_mapActors.Add(rstrDirName, pActor));
	pDup = NULL;

Cleanup:
	RStrRelease(rstrDirName);
	__delete pDup;
	return hr;
}

HRESULT CBlockMapEditorApp::LoadExternalTexture (RSTRING rstrName, __deref_out TEXTURE** ppTexture)
{
	HRESULT hr;
	RSTRING rstrFile = NULL;
	HANDLE hFile = INVALID_HANDLE_VALUE;
	DWORD cbSize, cbBits;
	DWORD r = 0, g = 0, b = 0, cPixels = 0;
	PBYTE pbFile = NULL;
	const BYTE* pBits;
	TEXTURE* pTexture = NULL;

	Check(RStrFormatW(&rstrFile, L"%r.png", rstrName));

	hFile = CreateFileW(RStrToWide(rstrFile), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	CheckIfGetLastError(INVALID_HANDLE_VALUE == hFile);

	cbSize = GetFileSize(hFile, NULL);

	pbFile = __new BYTE[cbSize];
	CheckAlloc(pbFile);
	CheckIfGetLastError(!ReadFile(hFile, pbFile, cbSize, &cbSize, NULL));

	pTexture = __new TEXTURE;
	CheckAlloc(pTexture);

	Check(sifReadPNGToBits32Stream(pbFile, cbSize, &pTexture->xSize, &pTexture->ySize, &pTexture->stmBits32, &cbBits));
	Check(m_mapExternal.Add(rstrName, pTexture));
	*ppTexture = pTexture;

	pBits = pTexture->stmBits32.TGetReadPtr<BYTE>();
	for(DWORD i = 0; i < cbBits; i += sizeof(DWORD))
	{
		if(pBits[i + 3])
		{
			r += pBits[i];
			g += pBits[i + 1];
			b += pBits[i + 2];
			cPixels++;
		}
	}

	pTexture->crAverage = RGB(r / cPixels, g / cPixels, b / cPixels);
	pTexture = NULL;

Cleanup:
	__delete_array pbFile;
	__delete pTexture;
	SafeCloseFileHandle(hFile);
	RStrRelease(rstrFile);
	return hr;
}

HRESULT CBlockMapEditorApp::LoadFile (ISimbeyUnzip* pPackage, PCWSTR pcwzFile, __out CMemoryStream* pstmFile)
{
	HRESULT hr;
	bool fOpened = false;

	CheckNoTrace(pPackage->unzLocateFile(pcwzFile, 0));
	Check(pPackage->unzOpenCurrentFile());
	fOpened = true;

	for(;;)
	{
		PBYTE pbPtr;
		DWORD cb;

		Check(pstmFile->TWriteAdvance(&pbPtr, 4096));
		Check(pPackage->unzReadCurrentFile(pbPtr, 4096, &cb));
		Check(pstmFile->PopWritePtr(4096 - cb, NULL));
		if(cb < 4096)
			break;
	}

Cleanup:
	if(fOpened)
		pPackage->unzCloseCurrentFile();
	return hr;
}

HRESULT CBlockMapEditorApp::LoadTextEntry (ISimbeyUnzip* pPackage, PCWSTR pcwzName, PWSTR* ppwzText, INT* pcchText)
{
	HRESULT hr;
	CMemoryStream stmText;

	hr = LoadFile(pPackage, pcwzName, &stmText);
	if(FAILED(hr))
	{
		WCHAR wzTxtName[MAX_PATH], *pwzPtr;
		INT cchRemaining;

		Check(TStrCchCpyEx(wzTxtName, ARRAYSIZE(wzTxtName), pcwzName, &pwzPtr, &cchRemaining));
		Check(TStrCchCpy(pwzPtr, cchRemaining, L".TXT"));
		Check(LoadFile(pPackage, wzTxtName, &stmText));
	}

	Check(Text::ConvertRawTextToUnicode(stmText.TGetReadPtr<BYTE>(), stmText.DataRemaining(), ppwzText, pcchText));

Cleanup:
	return hr;
}

HRESULT CBlockMapEditorApp::LoadTextures (ISimbeyUnzip* pPackage, PCWSTR pcwzFolder, INT cchFolder, BOOL (WINAPI* pfnLoadTextures)(PCWSTR pcwzFile, INT cchFile, unz_file_info64* pufi, PVOID pvParam), TRStrMap<TEXTURE*>* pmapTextures)
{
	HRESULT hr;
	TStackRef<IRStringArray> srFiles;
	CMemoryStream stmBuffer;
	WCHAR wzFile[MAX_PATH];
	RSTRING rstrFile = NULL;

	Check(ScCreateRStringArray(&srFiles));
	Check(pPackage->unzEnumFiles(TRUE, pfnLoadTextures, srFiles));
	Check(TStrCchCpy(wzFile, ARRAYSIZE(wzFile), pcwzFolder));

	for(sysint i = 0; i < srFiles->Length(); i++)
	{
		INT cchFile, cchMaxName = ARRAYSIZE(wzFile) - cchFolder;
		PWSTR pwzNamePtr = wzFile + cchFolder;

		stmBuffer.Reset();

		Check(srFiles->GetStringW(i, pwzNamePtr, cchMaxName, &cchFile));
		Check(TStrCchCpy(pwzNamePtr + cchFile, cchMaxName - cchFile, L".PNG"));
		Check(LoadFile(pPackage, wzFile, &stmBuffer));
		Check(srFiles->GetRString(i, &rstrFile));
		Check(LoadTexture(pmapTextures, rstrFile, stmBuffer.TGetReadPtr<BYTE>(), stmBuffer.DataRemaining()));
		RStrRelease(rstrFile); rstrFile = NULL;
	}

Cleanup:
	RStrRelease(rstrFile);
	return hr;
}

HRESULT CBlockMapEditorApp::LoadTexture (TRStrMap<TEXTURE*>* pmapTextures, RSTRING rstrFileW, const BYTE* pcbPNG, DWORD cbPNG)
{
	HRESULT hr;
	DWORD cbBits;
	DWORD r = 0, g = 0, b = 0, cPixels = 0;
	TEXTURE* pTexture = __new TEXTURE;
	const BYTE* pBits;

	CheckAlloc(pTexture);
	Check(sifReadPNGToBits32Stream(pcbPNG, cbPNG, &pTexture->xSize, &pTexture->ySize, &pTexture->stmBits32, &cbBits));
	Check(pmapTextures->Add(rstrFileW, pTexture));
	pTexture->pcwzName = RStrToWide(rstrFileW);

	pBits = pTexture->stmBits32.TGetReadPtr<BYTE>();
	for(DWORD i = 0; i < cbBits; i += sizeof(DWORD))
	{
		if(pBits[i + 3])
		{
			r += pBits[i];
			g += pBits[i + 1];
			b += pBits[i + 2];
			cPixels++;
		}
	}

	pTexture->crAverage = RGB(r / cPixels, g / cPixels, b / cPixels);
	pTexture = NULL;

Cleanup:
	__delete pTexture;
	return hr;
}

HRESULT CBlockMapEditorApp::DrawInfoPart (HDC hdc, CPaintItem* pInfo, LONG x, LONG y)
{
	HRESULT hr;
	PROPVARIANT propLabel; propLabel.vt = VT_EMPTY;
	RECT rcLabel = { x - 12, y + 64, x + 76, y + 100 };
	SIF_SURFACE sifSurface;

	m_Graph.GetRawBuffer(sifSurface.pbSurface, sifSurface.xSize, sifSurface.ySize, sifSurface.lPitch);
	sifSurface.cBitsPerPixel = 24;

	pInfo->InfoPaint(&sifSurface, hdc, x, y);

	Check(pInfo->GetValue(UI_PKEY_Label, &propLabel));
	DrawText(hdc, propLabel.bstrVal, SysStringLen(propLabel.bstrVal), &rcLabel, DT_CENTER | DT_WORDBREAK);

Cleanup:
	PropVariantClear(&propLabel);
	return hr;
}

HRESULT CBlockMapEditorApp::SetCellData (FLOAT x, FLOAT z)
{
	HRESULT hr;

	CheckIfIgnore(NULL == m_pSelection, HRESULT_FROM_WIN32(ERROR_EMPTY));
	Check(m_pBlockMap->SetCellData(x, z, m_pSelection));
	Check(Invalidate(FALSE));

Cleanup:
	return hr;
}

HRESULT CBlockMapEditorApp::UpdateAppTitle (VOID)
{
	HRESULT hr;
	RSTRING rstrTitle = NULL;
	PCWSTR pcwzFile = L"Untitled";

	if(m_rstrFile)
	{
		pcwzFile = TStrCchRChr(RStrToWide(m_rstrFile), RStrLen(m_rstrFile), L'\\');
		CheckIf(NULL == pcwzFile, E_UNEXPECTED);
		pcwzFile++;
	}

	Check(RStrFormatW(&rstrTitle, L"%ls - %ls", c_wzAppTitle, pcwzFile));
	CheckIfGetLastError(!SetWindowText(m_hwnd, RStrToWide(rstrTitle)));

Cleanup:
	RStrRelease(rstrTitle);
	return hr;
}

HINSTANCE CBlockMapEditorApp::GetInstance (VOID)
{
	return m_hInstance;
}

VOID CBlockMapEditorApp::OnFinalDestroy (HWND hwnd)
{
	if(m_pRibbon)
		m_pRibbon->Unload();
}

HRESULT CBlockMapEditorApp::FinalizeAndShow (DWORD dwStyle, INT nCmdShow)
{
	HRESULT hr;

	Check(m_pRibbon->Initialize(m_hwnd, this));
	Check(m_pRibbon->SetModes(1));
	Registry::LoadWindowPosition(m_hwnd, c_wzAppKey, L"WindowPlacement", &nCmdShow);
	Check(CBaseWindow::FinalizeAndShow(dwStyle, nCmdShow));

Cleanup:
	return hr;
}

BOOL CBlockMapEditorApp::DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	BOOL fHandled = FALSE;

	switch(message)
	{
	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(m_hwnd, &ps);

			// Don't allow painting beneath the ribbon.
			if(ps.rcPaint.top < m_rcClient.top)
				ps.rcPaint.top = m_rcClient.top;

			PaintWindow(hdc, &ps.rcPaint);
			EndPaint(m_hwnd, &ps);
		}
		break;
	case WM_ERASEBKGND:
		lResult = TRUE;
		fHandled = TRUE;
		break;
	case WM_SIZE:
		GetClientRect(m_hwnd, &m_rcClient);
		m_rcClient.top = m_pRibbon->GetHeight();
		SizeWindow();
		break;
	case WM_TIMER:
		if(m_idInfoTimer)
		{
			UnregisterTimer(m_idInfoTimer);
			m_idInfoTimer = 0;
		}
		m_fInfoBalloon = TRUE;
		InvalidateRect(m_hwnd, NULL, FALSE);
		break;
	case WM_CLOSE:
		Registry::SaveWindowPosition(m_hwnd, c_wzAppKey, L"WindowPlacement");
		break;
	case WM_DESTROY:
		if(m_pRibbon)
			m_pRibbon->SaveSettings();
		PostQuitMessage(0);
		break;
	default:
		fHandled = m_Graph.OnMessage(message, wParam, lParam, lResult);
		break;
	}

	return fHandled;
}

BOOL WINAPI CBlockMapEditorApp::_LoadTextures (PCWSTR pcwzFile, INT cchFile, unz_file_info64* pufi, PVOID pvParam)
{
	if(0 == TStrICmpNAssert(pcwzFile, SLP(c_wzTexturesSlash)))
	{
		PCWSTR pcwzName = pcwzFile + StaticLength(c_wzTexturesSlash);
		INT cchName = cchFile - StaticLength(c_wzTexturesSlash);

		PCWSTR pcwzExt = TStrCchRChr(pcwzName, cchName, L'.');
		if(pcwzExt && 0 == TStrCmpIAssert(pcwzExt + 1, L"PNG"))
			reinterpret_cast<IRStringArray*>(pvParam)->AddStringW(pcwzName, static_cast<INT>(pcwzExt - pcwzName));
	}
	return FALSE;
}

BOOL WINAPI CBlockMapEditorApp::_LoadSprites (PCWSTR pcwzFile, INT cchFile, unz_file_info64* pufi, PVOID pvParam)
{
	if(0 == TStrICmpNAssert(pcwzFile, SLP(c_wzSpritesSlash)))
	{
		PCWSTR pcwzName = pcwzFile + StaticLength(c_wzSpritesSlash);
		INT cchName = cchFile - StaticLength(c_wzSpritesSlash);

		PCWSTR pcwzExt = TStrCchRChr(pcwzName, cchName, L'.');
		if(pcwzExt && 0 == TStrCmpIAssert(pcwzExt + 1, L"PNG"))
			reinterpret_cast<IRStringArray*>(pvParam)->AddStringW(pcwzName, static_cast<INT>(pcwzExt - pcwzName));
	}
	return FALSE;
}

BOOL CBlockMapEditorApp::ReadToken (__inout PCWSTR& pcwzTokens, __deref_out PCWSTR* ppcwzToken, INT* pcchToken)
{
	BOOL fSuccess = FALSE;
	PCWSTR pcwzPtr = pcwzTokens;

	if(pcwzPtr)
	{
		for(;;)
		{
			while(Formatting::IsSpace(*pcwzPtr))
				pcwzPtr++;
			if(L'\0' == *pcwzPtr)
			{
				pcwzPtr = NULL;
				break;
			}

			if(L'/' == pcwzPtr[0] && L'/' == pcwzPtr[1])
			{
				pcwzPtr = TStrChr(pcwzPtr, L'\n');
				if(NULL == pcwzPtr)
					break;
				continue;
			}

			*ppcwzToken = pcwzPtr;

			if(L'"' == *pcwzPtr)
			{
				pcwzPtr = TStrChr(pcwzPtr + 1, L'"');
				if(NULL == pcwzPtr)
					break;
				pcwzPtr++;
			}
			else
			{
				while(!Formatting::IsSpace(*pcwzPtr))
				{
					// Also break on commas, colons, and braces
					if(*pcwzPtr == L',' || *pcwzPtr == L':' || *pcwzPtr == L'{' || *pcwzPtr == L'}')
					{
						// If the first character is breaking, then be sure to increment
						if(pcwzPtr == *ppcwzToken)
							pcwzPtr++;
						break;
					}
					pcwzPtr++;
				}
			}

			*pcchToken = static_cast<INT>(pcwzPtr - *ppcwzToken);
			fSuccess = TRUE;
			break;
		}

		pcwzTokens = pcwzPtr;
	}

	return fSuccess;
}
