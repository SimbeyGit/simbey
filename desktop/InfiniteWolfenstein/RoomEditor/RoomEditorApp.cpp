#include <windows.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "Library\Core\StringCore.h"
#include "Library\Util\Options.h"
#include "Library\Util\Registry.h"
#include "Library\Window\DialogHost.h"
#include "Library\ChooseFile.h"
#include "Library\DPI.h"
#include "Package\SIFPackage.h"
#include "Selector\SelectSIFDlg.h"
#include "Published\JSON.h"
#include "Ribbon.h"
#include "RibbonMappings.h"
#include "Rooms.h"
#include "RenameRoomDlg.h"
#include "RoomEditorApp.h"

const WCHAR c_wzAppClassName[] = L"RoomEditorAppCls";
const WCHAR c_wzAppTitle[] = L"Infinite Wolfenstein Room Editor";
const WCHAR c_wzAppKey[] = L"Software\\Simbey\\InfiniteWolfenstein\\RoomEditor";

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

CRoomEditorApp::CRoomEditorApp (HINSTANCE hInstance) :
	m_hInstance(hInstance),
	m_pRibbon(NULL),
	m_idxSelected(0),
	m_rstrFile(NULL),
	m_pRooms(NULL),
	m_fPainting(FALSE),
	m_xInfo(0), m_zInfo(0),
	m_fInfoBalloon(FALSE),
	m_idInfoTimer(0)
{
	m_Graph.SetGraphType(GRAPH_XZ);
	m_Graph.SetGridType(GRID_AXIS_POINTS);
	m_Graph.AttachContainer(this);
	m_Graph.SetGraphTarget(this);

	m_Graph.SetBGColor(RGB(96, 96, 96));
}

CRoomEditorApp::~CRoomEditorApp ()
{
	for(sysint i = 0; i < m_mapPackages.Length(); i++)
	{
		PACKAGE_DATA* pData = m_mapPackages.GetValuePtr(i);
		FreePackage(*pData);
	}

	for(sysint i = 0; i < m_aTypes.Length(); i++)
		m_aTypes[i]->Release();

	m_Graph.AttachContainer(NULL);

	RStrRelease(m_rstrFile);
	SafeDelete(m_pRooms);
	SafeRelease(m_pRibbon);
}

HRESULT CRoomEditorApp::Register (HINSTANCE hInstance)
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

HRESULT CRoomEditorApp::Initialize (LPWSTR lpCmdLine, INT nCmdShow)
{
	HRESULT hr;
	COptions options;
	sysint idxPkgCmd;

	Check(options.Parse(lpCmdLine));

	if(GetFileAttributes(L"InfiniteWolfenstein.pkg") != INVALID_FILE_ATTRIBUTES)
		Check(LoadPackage(L"InfiniteWolfenstein.pkg", TRUE));
	else
		Check(LoadPackage(L"..\\InfiniteWolfenstein.pkg", TRUE));

	if(options.FindParam(L"pkg", &idxPkgCmd))
	{
		for(sysint i = idxPkgCmd + 1; i < options.Length(); i++)
		{
			RSTRING rstrArg = options[i];
			PCWSTR pcwzArg = RStrToWide(rstrArg);

			if(L'-' == pcwzArg[0] || L'/' == pcwzArg[0])
				break;

			Check(LoadPackage(pcwzArg, FALSE));
		}
	}

	Check(CSIFRibbon::Create(DPI::Scale, &m_pRibbon));
	Check(CreateRooms(&m_pRooms));

	Check(CreateSwatchItem(SLP(L"Empty"), SLP(L"any.empty"), RGB(127, 127, 127)));
	Check(CreateSwatchItem(SLP(L"Tunnel Start"), SLP(L"any.tunnel-start"), COLOR_TUNNEL_START));
	Check(CreateSwatchItem(SLP(L"Connector"), SLP(L"any.connector"), COLOR_CONNECTOR));
	Check(CreateSwatchItem(SLP(L"Anchor"), SLP(L"any.anchor"), COLOR_ANCHOR));
	Check(CreateSwatchItem(SLP(L"Floor"), SLP(L"any.floor"), COLOR_FLOOR));
	Check(CreateSwatchItem(SLP(L"Wall"), SLP(L"any.wall"), COLOR_WALL));
	Check(CreateSwatchItem(SLP(L"Decoration"), SLP(L"any.decoration"), COLOR_DECORATION));
	Check(CreateSwatchItem(SLP(L"Door"), SLP(L"any.door"), COLOR_DOOR));
	Check(CreateSwatchItem(SLP(L"Elevator Door"), SLP(L"any.elevator-door"), COLOR_ELEVATOR_DOOR));
	Check(CreateSwatchItem(SLP(L"Locked Door [Gold]"), SLP(L"any.locked-gold"), COLOR_GOLD_LOCKED));
	Check(CreateSwatchItem(SLP(L"Locked Door [Silver]"), SLP(L"any.locked-silver"), COLOR_SILVER_LOCKED));
	Check(CreateSwatchItem(SLP(L"Split Door"), SLP(L"any.split-door"), COLOR_SPLIT_DOOR));
	Check(CreateSwatchItem(SLP(L"Railing"), SLP(L"any.railing"), COLOR_RAILING));
	Check(CreateSwatchItem(SLP(L"Switch"), SLP(L"any.switch"), COLOR_SWITCH));
	Check(CreateSwatchItem(SLP(L"Pick Wall"), SLP(L"any.pick_wall"), RGB(0, 128, 192)));
	Check(CreateSwatchItem(SLP(L"Pick Entity"), SLP(L"any.pick_entity"), RGB(192, 0, 160), FALSE));
	Check(CreateIconItem(SLP(L"Delete Entity"), SLP(L"any.delete_entity"), ID_DELETE_ROOM));
	m_cDefaultTypes = m_aTypes.Length();

	Check(Create(WS_EX_APPWINDOW | WS_EX_WINDOWEDGE, WS_OVERLAPPEDWINDOW, c_wzAppClassName, c_wzAppTitle, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, nCmdShow));
	Check(UpdateAppTitle());
	Check(UpdatePaintType());

Cleanup:
	return hr;
}

// IUIApplication

HRESULT STDMETHODCALLTYPE CRoomEditorApp::OnViewChanged (UINT32 viewId, UI_VIEWTYPE typeID, IUnknown* view, UI_VIEWVERB verb, INT32 uReasonCode)
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

HRESULT STDMETHODCALLTYPE CRoomEditorApp::OnCreateUICommand (UINT32 commandId, UI_COMMANDTYPE typeID, IUICommandHandler** commandHandler)
{
	*commandHandler = this;
	AddRef();
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CRoomEditorApp::OnDestroyUICommand (UINT32 commandId, UI_COMMANDTYPE typeID, IUICommandHandler* commandHandler)
{
	return S_OK;
}

// IRibbonHost

HRESULT WINAPI CRoomEditorApp::GetRibbonSettingsKey (__out_ecount(cchMaxKey) PSTR pszKeyName, INT cchMaxKey)
{
	return TStrCchCpy(pszKeyName, cchMaxKey, "\\Software\\Simbey\\InfiniteWolfenstein");
}

HRESULT WINAPI CRoomEditorApp::GetRibbonSettingsValue (__out_ecount(cchMaxValue) PSTR pszValueName, INT cchMaxValue)
{
	return TStrCchCpy(pszValueName, cchMaxValue, "RibbonSettings");
}

HRESULT WINAPI CRoomEditorApp::GetRibbonResource (__out HMODULE* phModule, __out_ecount(cchMaxResource) PWSTR pwzResource, INT cchMaxResource)
{
	*phModule = m_hInstance;
	return TStrCchCpy(pwzResource, cchMaxResource, L"APPLICATION_RIBBON");
}

UINT WINAPI CRoomEditorApp::TranslateGroupToImage (UINT nID)
{
	// Use the tool-generated MapGroupsToImages() function.
	return MapGroupsToImages(nID);
}

UINT WINAPI CRoomEditorApp::TranslateImageToLargeImage (UINT nID)
{
	// Use the tool-generated MapImagesToLargeImages() function.
	return MapImagesToLargeImages(nID);
}

// IUICommandHandler

HRESULT STDMETHODCALLTYPE CRoomEditorApp::Execute (UINT32 commandId, UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* currentValue, IUISimplePropertySet* commandExecutionProperties)
{
	HRESULT hr = S_OK;

	if(UI_EXECUTIONVERB_EXECUTE == verb)
	{
		switch(commandId)
		{
		case ID_NEW:
			SafeDelete(m_pRooms);
			RStrRelease(m_rstrFile); m_rstrFile = NULL;
			Check(CreateRooms(&m_pRooms));
			Check(UpdateAppTitle());
			Check(Invalidate(FALSE));
			Check(m_pRibbon->UpdateProperty(ID_SELECT_ROOM, &UI_PKEY_ItemsSource));
			Check(m_pRibbon->UpdateProperty(ID_ENABLE_ROTATION, &UI_PKEY_BooleanValue));
			Check(m_pRibbon->InvalidateEnabled());
			break;
		case ID_OPEN:
			CheckNoTrace(OpenRooms());
			break;
		case ID_SAVE:
			CheckNoTrace(SaveRooms());
			break;
		case ID_EXIT:
			PostMessage(m_hwnd, WM_CLOSE, 0, 0);
			break;
		case ID_SELECT_ROOM:
			if(UI_PKEY_SelectedItem == *key)
			{
				Check(m_pRooms->SelectRoom(currentValue->ulVal));
				Check(m_pRibbon->UpdateProperty(ID_SELECT_ROOM, &UI_PKEY_SelectedItem));
				Check(m_pRibbon->UpdateProperty(ID_ENABLE_ROTATION, &UI_PKEY_BooleanValue));
				Check(m_pRibbon->InvalidateEnabled());
				Check(UpdateAppTitle());
				Check(Invalidate(FALSE));
			}
			break;
		case ID_NEW_ROOM:
			{
				sysint idxRoom;
				Check(m_pRooms->AddRoom(&idxRoom));
				Check(m_pRooms->SelectRoom(idxRoom));
				Check(m_pRibbon->UpdateProperty(ID_SELECT_ROOM, &UI_PKEY_ItemsSource));
				Check(m_pRibbon->UpdateProperty(ID_ENABLE_ROTATION, &UI_PKEY_BooleanValue));
				Check(m_pRibbon->InvalidateEnabled());
				Check(UpdateAppTitle());
				Check(Invalidate(FALSE));
			}
			break;
		case ID_RENAME_ROOM:
			CheckNoTrace(RenameRoom());
			break;
		case ID_DELETE_ROOM:
			{
				sysint idxRoom;
				Check(m_pRooms->GetRoomIndex(&idxRoom));
				Check(m_pRooms->DeleteRoom(idxRoom));
				Check(m_pRooms->SelectRoom(-1));
				Check(m_pRibbon->UpdateProperty(ID_SELECT_ROOM, &UI_PKEY_ItemsSource));
				Check(m_pRibbon->UpdateProperty(ID_ENABLE_ROTATION, &UI_PKEY_BooleanValue));
				Check(m_pRibbon->InvalidateEnabled());
				Check(UpdateAppTitle());
				Check(Invalidate(FALSE));
			}
			break;
		case ID_CLONE_ROOM:
			Check(CloneRoom());
			break;
		case ID_PAINT_TYPE:
			if(UI_PKEY_SelectedItem == *key)
			{
				if(SUCCEEDED(SelectPaintType(currentValue->ulVal)))
					UpdatePaintType();
			}
			break;
		case ID_ENABLE_ROTATION:
			CheckIf(!m_pRooms->HasSelection(), E_FAIL);
			m_pRooms->m_pSelected->m_fEnableRotation = !m_pRooms->m_pSelected->m_fEnableRotation;
			Check(m_pRibbon->UpdateProperty(ID_ENABLE_ROTATION, &UI_PKEY_BooleanValue));
			break;
		}
	}

Cleanup:
	return hr;
}

HRESULT STDMETHODCALLTYPE CRoomEditorApp::UpdateProperty (UINT32 commandId, REFPROPERTYKEY key, const PROPVARIANT* currentValue, PROPVARIANT* newValue)
{
	HRESULT hr = E_NOTIMPL;

	if(UI_PKEY_LargeImage == key || UI_PKEY_SmallImage == key)
		hr = m_pRibbon->LoadImageForCommand(commandId, key, newValue);

	if(FAILED(hr))
	{
		if(ID_SELECT_ROOM == commandId)
		{
			hr = m_pRooms->UpdateSelectionData(m_pRibbon, key, currentValue, newValue);
			if(SUCCEEDED(hr) && UI_PKEY_ItemsSource == key)
				hr = m_pRibbon->UpdateProperty(ID_SELECT_ROOM, &UI_PKEY_SelectedItem);
		}
		else if(UI_PKEY_Enabled == key)
		{
			switch(commandId)
			{
			case ID_RENAME_ROOM:
			case ID_DELETE_ROOM:
			case ID_CLONE_ROOM:
			case ID_ENABLE_ROTATION:
				newValue->boolVal = m_pRooms->HasSelection() ? VARIANT_TRUE : VARIANT_FALSE;
				break;
			default:
				newValue->boolVal = VARIANT_TRUE;
				break;
			}
			newValue->vt = VT_BOOL;
			hr = S_OK;
		}
		else if(ID_PAINT_TYPE == commandId)
		{
			if(UI_PKEY_Label == key)
				hr = m_aTypes[m_idxSelected]->GetValue(key, newValue);
			else if(UI_PKEY_LargeImage == key || UI_PKEY_SmallImage == key)
				hr = m_aTypes[m_idxSelected]->GetValue(key, newValue);
			else if(UI_PKEY_SelectedItem == key)
			{
				newValue->vt = VT_UI4;
				newValue->ulVal = m_idxSelected;
				hr = S_OK;
			}
			else if(UI_PKEY_ItemsSource == key)
				hr = FillPaintTypes(currentValue);
		}
		else if(UI_PKEY_BooleanValue == key)
		{
			if(ID_ENABLE_ROTATION == commandId)
			{
				newValue->vt = VT_BOOL;
				newValue->boolVal = (m_pRooms->m_pSelected && m_pRooms->m_pSelected->m_fEnableRotation) ? VARIANT_TRUE : VARIANT_FALSE;
				hr = S_OK;
			}
		}
	}

	return hr;
}

// IGraphContainer

HRESULT WINAPI CRoomEditorApp::SetFocus (__in IGrapher* pGraphCtrl)
{
	::SetFocus(m_hwnd);
	return S_OK;
}

HRESULT WINAPI CRoomEditorApp::ScreenToWindowless (__in IGrapher* pGraphCtrl, __inout PPOINT ppt)
{
	if(ScreenToClient(m_hwnd, ppt))
		return S_OK;
	return E_FAIL;
}

HRESULT WINAPI CRoomEditorApp::InvalidateContainer (__in IGrapher* pGraphCtrl)
{
	return Invalidate(FALSE);
}

VOID WINAPI CRoomEditorApp::DrawDib24 (HDC hdcDest, LONG x, LONG y, HDC hdcSrc, const BYTE* pcDIB24, LONG xSize, LONG ySize, LONG lPitch)
{
	BitBlt(hdcDest, x, y, xSize, ySize, hdcSrc, 0, 0, SRCCOPY);
}

BOOL WINAPI CRoomEditorApp::CaptureMouse (__in IGrapher* pGraphCtrl, BOOL fCapture)
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

VOID CRoomEditorApp::onGraphPaint (IGrapher* lpGraph)
{
	m_pRooms->Paint(lpGraph);

	if(m_fInfoBalloon)
	{
		TStackRef<IJSONObject> srWall, srEntity;
		if(SUCCEEDED(m_pRooms->GetCellData(m_xInfo, m_zInfo, &srWall, &srEntity)))
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
			PaintBalloon(hdc, eDir, &rcClient, xClient, yClient, srEntity ? 160 : 80, 80, &rc);

			DrawInfoPart(hdc, srWall, rc.left + 8, rc.top, FALSE);
			if(srEntity)
				DrawInfoPart(hdc, srEntity, rc.left + 88, rc.top, TRUE);

			DeleteObject(SelectObject(hdc, hOldFont));
		}
	}
}

VOID CRoomEditorApp::onGraphMouseMove (DWORD dwKeys, FLOAT x, FLOAT y)
{
	if(m_fPainting)
		SetCellData(x, y);
	else
	{
		INT xInfo = static_cast<INT>(x / 16.0f);
		if(x < 0.0f)
			xInfo--;
		INT zInfo = static_cast<INT>(y / 16.0f);
		if(y < 0.0f)
			zInfo--;

		if(xInfo != m_xInfo || zInfo != m_zInfo)
		{
			TStackRef<IJSONObject> srWall, srEntity;

			if(m_fInfoBalloon)
			{
				m_fInfoBalloon = FALSE;
				InvalidateRect(m_hwnd, NULL, FALSE);
			}

			m_xInfo = xInfo;
			m_zInfo = zInfo;

			if(SUCCEEDED(m_pRooms->GetCellData(xInfo, zInfo, &srWall, &srEntity)))
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

VOID CRoomEditorApp::onGraphLBtnDown (DWORD dwKeys, FLOAT x, FLOAT y)
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

	if(SUCCEEDED(SetCellData(x, y)))
	{
		CaptureMouse(&m_Graph, TRUE);
		m_fPainting = TRUE;
	}
}

VOID CRoomEditorApp::onGraphLBtnUp (DWORD dwKeys, FLOAT x, FLOAT y)
{
	if(m_fPainting)
	{
		m_fPainting = FALSE;
		CaptureMouse(&m_Graph, FALSE);
	}
}

BOOL CRoomEditorApp::onGraphRBtnDown (DWORD dwKeys, FLOAT x, FLOAT y)
{
	return FALSE;
}

VOID CRoomEditorApp::onGraphRBtnUp (DWORD dwKeys, FLOAT x, FLOAT y)
{
}

VOID CRoomEditorApp::onGraphLBtnDbl (DWORD dwKeys, FLOAT x, FLOAT y)
{
}

VOID CRoomEditorApp::onGraphRBtnDbl (DWORD dwKeys, FLOAT x, FLOAT y)
{
}

VOID CRoomEditorApp::onGraphViewChanged (BOOL fZoomChanged)
{
}

BOOL CRoomEditorApp::onGraphKeyDown (WPARAM iKey)
{
	return FALSE;
}

BOOL CRoomEditorApp::onGraphKeyUp (WPARAM iKey)
{
	return FALSE;
}

BOOL CRoomEditorApp::onGraphChar (WPARAM iKey)
{
	return FALSE;
}

BOOL CRoomEditorApp::onGraphWheel (SHORT sDistance, FLOAT x, FLOAT y)
{
	return FALSE;
}

HRESULT CRoomEditorApp::onGraphGetAcc (IAccessible** lplpAccessible)
{
	return E_NOTIMPL;
}

VOID CRoomEditorApp::SizeWindow (VOID)
{
	m_Graph.Move(&m_rcClient);
}

VOID CRoomEditorApp::PaintWindow (HDC hdc, const RECT* prc)
{
	m_Graph.Paint(hdc);
}

HRESULT CRoomEditorApp::CreateRooms (__deref_out CRooms** ppRooms)
{
	HRESULT hr;
	CRooms* pRooms = __new CRooms;

	CheckAlloc(pRooms);
	Check(pRooms->Initialize());
	*ppRooms = pRooms;
	pRooms = NULL;

Cleanup:
	__delete pRooms;
	return hr;
}

HRESULT CRoomEditorApp::RenameRoom (VOID)
{
	HRESULT hr;
	CDialogHost host(m_hInstance);
	CRenameRoomDlg dlgRename(m_pRooms->m_pSelected);

	Check(host.Display(m_hwnd, &dlgRename));
	Check(UpdateAppTitle());

	Check(m_pRibbon->UpdateProperty(ID_SELECT_ROOM, &UI_PKEY_ItemsSource));

Cleanup:
	return hr;
}

HRESULT CRoomEditorApp::OpenRooms (VOID)
{
	HRESULT hr;
	CChooseFile choose;
	RSTRING rstrFile = NULL;
	CRooms* pRooms = NULL;

	Check(choose.Initialize());
	CheckIfIgnore(!choose.OpenSingleFile(m_hwnd, L"Rooms (*.json)\0*.json\0\0"), E_ABORT);

	PCWSTR pcwzFile = choose.GetFile(0);
	Check(RStrCreateW(TStrLenAssert(pcwzFile), pcwzFile, &rstrFile));
	Check(CreateRooms(&pRooms));
	Check(pRooms->Load(pcwzFile));

	SwapData(m_pRooms, pRooms);
	SwapData(m_rstrFile, rstrFile);

	Check(UpdateAppTitle());
	Check(Invalidate(FALSE));
	Check(m_pRibbon->UpdateProperty(ID_SELECT_ROOM, &UI_PKEY_ItemsSource));
	Check(m_pRibbon->UpdateProperty(ID_ENABLE_ROTATION, &UI_PKEY_BooleanValue));
	Check(m_pRibbon->InvalidateEnabled());

Cleanup:
	__delete pRooms;
	RStrRelease(rstrFile);
	return hr;
}

HRESULT CRoomEditorApp::SaveRooms (VOID)
{
	HRESULT hr;
	RSTRING rstrFile = NULL;

	if(NULL == m_rstrFile)
	{
		CChooseFile choose;
		Check(choose.Initialize());
		CheckIfIgnore(!choose.SaveFile(m_hwnd, L"Rooms (*.json)\0*.json\0\0"), E_ABORT);

		PCWSTR pcwzFile = choose.GetFile(0);
		Check(RStrCreateW(TStrLenAssert(pcwzFile), pcwzFile, &rstrFile));

		Check(m_pRooms->Save(pcwzFile));

		m_rstrFile = rstrFile;
		rstrFile = NULL;
	}
	else
		Check(m_pRooms->Save(RStrToWide(m_rstrFile)));

	Check(UpdateAppTitle());

Cleanup:
	RStrRelease(rstrFile);
	return hr;
}

HRESULT CRoomEditorApp::CloneRoom (VOID)
{
	HRESULT hr;
	TStackRef<IJSONValue> srvRoom, srv;
	TStackRef<IJSONObject> srRoom;
	RSTRING rstrName = NULL, rstrCloneName = NULL;
	sysint idxRoom;

	Check(m_pRooms->m_pSelected->Serialize(&srvRoom));
	Check(srvRoom->GetObject(&srRoom));
	Check(srRoom->FindNonNullValueW(L"name", &srv));
	Check(srv->GetString(&rstrName));

	Check(RStrFormatW(&rstrCloneName, L"%r Clone", rstrName));
	Check(m_pRooms->AddRoom(rstrCloneName, srRoom, &idxRoom));
	Check(m_pRooms->SelectRoom(idxRoom));

	Check(UpdateAppTitle());
	Check(m_pRibbon->UpdateProperty(ID_SELECT_ROOM, &UI_PKEY_ItemsSource));

Cleanup:
	RStrRelease(rstrCloneName);
	RStrRelease(rstrName);
	return hr;
}

HRESULT CRoomEditorApp::CreateSwatchItem (PCWSTR pcwzLabel, INT cchLabel, PCWSTR pcwzType, INT cchType, COLORREF crSwatch, BOOL fSquare)
{
	HRESULT hr;
	TStackRef<CSwatchItem> srSwatch;
	TStackRef<IJSONObject> srObject;
	TStackRef<IJSONValue> srv;

	Check(CSwatchItem::Create(m_pRibbon, &srSwatch));
	Check(srSwatch->SetItemText(pcwzLabel));
	srSwatch->SetSwatchColor(crSwatch);

	if(!fSquare)
		srSwatch->SetSwatchFunction(CSwatchItem::CircleSwatchImage);

	Check(JSONCreateObject(&srObject));
	Check(JSONCreateStringW(pcwzType, cchType, &srv));
	Check(srObject->AddValueW(L"type", srv));
	srv.Release();

	if(0 == TStrCmpAssert(pcwzType, L"entity"))
	{
		Check(JSONCreateStringW(pcwzLabel, cchLabel, &srv));
		Check(srObject->AddValueW(L"entity", srv));
		srv.Release();
	}

	if(crSwatch > INT_MAX)
		Check(JSONCreateLongInteger(crSwatch, &srv));
	else
		Check(JSONCreateInteger(crSwatch, &srv));
	Check(srObject->AddValueW(L"cr", srv));

	srSwatch->SetObject(srObject);

	Check(m_aTypes.Append(srSwatch));
	srSwatch.Detach();

Cleanup:
	return hr;
}

HRESULT CRoomEditorApp::CreateIconItem (PCWSTR pcwzLabel, INT cchLabel, PCWSTR pcwzType, INT cchType, UINT nIcon)
{
	HRESULT hr;
	TStackRef<CStringGalleryItem> srItem;
	TStackRef<IJSONObject> srObject;
	TStackRef<IJSONValue> srv;

	Check(CStringGalleryItem::Create(m_pRibbon, &srItem));
	Check(srItem->SetItemText(pcwzLabel));
	srItem->SetItemIcon(nIcon);

	Check(JSONCreateObject(&srObject));
	Check(JSONCreateStringW(pcwzType, cchType, &srv));
	Check(srObject->AddValueW(L"type", srv));
	srItem->SetObject(srObject);

	Check(m_aTypes.Append(srItem));
	srItem.Detach();

Cleanup:
	return hr;
}

HRESULT CRoomEditorApp::FillPaintTypes (const PROPVARIANT* pVar)
{
	HRESULT hr;
	IUICollection* pCollection = NULL;
	CSwatchItem* pItem = NULL;

	CheckIf(NULL == pVar || VT_UNKNOWN != pVar->vt, E_UNEXPECTED);
	Check(pVar->punkVal->QueryInterface(&pCollection));

	Check(pCollection->Clear());

	for(sysint i = 0; i < m_aTypes.Length(); i++)
		Check(pCollection->Add(static_cast<IUISimplePropertySet*>(m_aTypes[i])));

Cleanup:
	SafeRelease(pCollection);
	return hr;
}

HRESULT CRoomEditorApp::UpdatePaintType (VOID)
{
	HRESULT hr;

	Check(m_pRibbon->UpdateProperty(ID_PAINT_TYPE, &UI_PKEY_LargeImage));
	Check(m_pRibbon->UpdateProperty(ID_PAINT_TYPE, &UI_PKEY_SmallImage));
	Check(m_pRibbon->UpdateProperty(ID_PAINT_TYPE, &UI_PKEY_Label));

Cleanup:
	return hr;
}

HRESULT CRoomEditorApp::SelectPaintType (ULONG idxType)
{
	HRESULT hr;
	TStackRef<IJSONObject> srObject;
	TStackRef<IJSONValue> srv;
	RSTRING rstrType = NULL;

	Check(m_aTypes[idxType]->QueryService(IID_NULL, IID_PPV_ARGS(&srObject)));
	Check(srObject->FindNonNullValueW(L"type", &srv));
	Check(srv->GetString(&rstrType));

	if(0 == TStrCmpAssert(RStrToWide(rstrType), L"any.pick_wall"))
		hr = ShowPickWallDialog(&idxType);
	else if(0 == TStrCmpAssert(RStrToWide(rstrType), L"any.pick_entity"))
		hr = ShowPickEntityDialog(&idxType);

	if(E_ABORT == hr)
	{
		Check(m_pRibbon->UpdateProperty(ID_SELECT_ROOM, &UI_PKEY_SelectedItem));
		hr = E_ABORT;
	}
	else
	{
		Check(hr);
		m_idxSelected = idxType;
	}

Cleanup:
	RStrRelease(rstrType);
	return hr;
}

HRESULT CRoomEditorApp::SetCellData (FLOAT x, FLOAT z)
{
	HRESULT hr;
	TStackRef<IJSONObject> srObject;

	Check(m_aTypes[m_idxSelected]->QueryService(IID_NULL, IID_PPV_ARGS(&srObject)));
	Check(m_pRooms->SetCellData(x, z, srObject));
	Invalidate(FALSE);

Cleanup:
	return hr;
}

HRESULT CRoomEditorApp::ShowPickWallDialog (__out ULONG* pidxType)
{
	HRESULT hr;
	CDialogHost host(m_hInstance);
	CSelectSIFDlg dlgSelect(IDD_SELECT_SIF, IDC_ITEMS);
	RSTRING rstrSelectedNamespace = NULL;
	RSTRING rstrTitle = NULL;
	PACKAGE_DATA data;

	Check(dlgSelect.Initialize());

	for(sysint i = 0; i < m_mapPackages.Length(); i++)
	{
		RSTRING rstrNamespace;

		Check(m_mapPackages.GetKeyAndValue(i, &rstrNamespace, &data));
		if(data.psifWalls)
		{
			Check(RStrFormatW(&rstrTitle, L"Namespace: %r", rstrNamespace));
			Check(dlgSelect.AddSIF(rstrTitle, data.psifWalls));

			RStrRelease(rstrTitle); rstrTitle = NULL;
		}
	}

	Check(host.Display(m_hwnd, &dlgSelect));

	if(IDOK == host.GetReturnValue())
	{
		TStackRef<ISimbeyInterchangeFileLayer> srLayer;
		DWORD idSelection;
		COLORREF cr;
		PCWSTR pcwzNamespace;
		WCHAR wzType[MAX_PATH], *pwzPtr;
		INT cchPtr, cchName;

		Check(dlgSelect.GetSelection(&rstrTitle, &idSelection));

		pcwzNamespace = TStrChr(RStrToWide(rstrTitle), L':');
		CheckIf(NULL == pcwzNamespace, E_FAIL);
		pcwzNamespace++;
		if(*pcwzNamespace == L' ')
			pcwzNamespace++;

		Check(RStrCreateW(TStrLenAssert(pcwzNamespace), pcwzNamespace, &rstrSelectedNamespace));
		Check(m_mapPackages.Find(rstrSelectedNamespace, &data));
		Check(data.psifWalls->GetLayer(idSelection, &srLayer));

		Check(TStrCchCpyEx(wzType, ARRAYSIZE(wzType), pcwzNamespace, &pwzPtr, &cchPtr));
		*pwzPtr = L'.';
		pwzPtr++;
		cchPtr--;
		Check(srLayer->GetName(pwzPtr, cchPtr));

		cchName = TStrLenAssert(pwzPtr);
		pwzPtr += cchName;
		cchPtr -= cchName;

		Check(CalculateAveragePixelColor(srLayer, &cr));

		if(m_aTypes.Length() > m_cDefaultTypes)
		{
			TStackRef<CBaseRibbonItem> srItem;
			Check(m_aTypes.RemoveChecked(m_cDefaultTypes, &srItem));
		}

		Check(CreateSwatchItem(wzType, ARRAYSIZE(wzType) - cchPtr, wzType, ARRAYSIZE(wzType) - cchPtr, cr));
		Check(m_pRibbon->UpdateProperty(ID_PAINT_TYPE, &UI_PKEY_ItemsSource));
		*pidxType = m_cDefaultTypes;
	}
	else
		hr = E_ABORT;

Cleanup:
	RStrRelease(rstrSelectedNamespace);
	RStrRelease(rstrTitle);
	return hr;
}

HRESULT CRoomEditorApp::ShowPickEntityDialog (__out ULONG* pidxType)
{
	HRESULT hr;
	CDialogHost host(m_hInstance);
	CSelectSIFDlg dlgSelect(IDD_SELECT_SIF, IDC_ITEMS);
	RSTRING rstrSelectedNamespace = NULL;
	RSTRING rstrTitle = NULL, rstrName = NULL;
	PACKAGE_DATA data;

	Check(dlgSelect.Initialize());

	for(sysint i = 0; i < m_mapPackages.Length(); i++)
	{
		RSTRING rstrNamespace;

		Check(m_mapPackages.GetKeyAndValue(i, &rstrNamespace, &data));
		if(data.psifEntities)
		{
			Check(RStrFormatW(&rstrTitle, L"Namespace: %r", rstrNamespace));
			Check(dlgSelect.AddSIF(rstrTitle, data.psifEntities));

			RStrRelease(rstrTitle); rstrTitle = NULL;
		}
	}

	Check(host.Display(m_hwnd, &dlgSelect));

	if(IDOK == host.GetReturnValue())
	{
		TStackRef<ISimbeyInterchangeFileLayer> srLayer;
		TStackRef<IJSONObject> srEntity;
		TStackRef<IJSONValue> srv;
		DWORD idSelection;
		COLORREF cr;
		PCWSTR pcwzNamespace;
		WCHAR wzLabel[MAX_PATH], wzLayer[MAX_PATH], *pwzPtr;
		INT cchPtr;

		Check(dlgSelect.GetSelection(&rstrTitle, &idSelection));

		pcwzNamespace = TStrChr(RStrToWide(rstrTitle), L':');
		CheckIf(NULL == pcwzNamespace, E_FAIL);
		pcwzNamespace++;
		if(*pcwzNamespace == L' ')
			pcwzNamespace++;

		Check(RStrCreateW(TStrLenAssert(pcwzNamespace), pcwzNamespace, &rstrSelectedNamespace));
		Check(m_mapPackages.Find(rstrSelectedNamespace, &data));
		Check(data.psifEntities->GetLayer(idSelection, &srLayer));

		Check(srLayer->GetName(wzLayer, ARRAYSIZE(wzLayer)));
		Check(JSONFindArrayObject(data.pModels, RSTRING_CAST(L"entity"), RSTRING_CAST(wzLayer), &srEntity, NULL));
		Check(srEntity->FindNonNullValueW(L"name", &srv));
		Check(srv->GetString(&rstrName));

		Check(TStrCchCpyEx(wzLabel, ARRAYSIZE(wzLabel), pcwzNamespace, &pwzPtr, &cchPtr));
		*pwzPtr = L'.';
		pwzPtr++;
		cchPtr--;
		Check(TStrCchCpyEx(pwzPtr, cchPtr, RStrToWide(rstrName), &pwzPtr, &cchPtr));

		Check(CalculateAveragePixelColor(srLayer, &cr));

		if(m_aTypes.Length() > m_cDefaultTypes)
		{
			TStackRef<CBaseRibbonItem> srItem;
			Check(m_aTypes.RemoveChecked(m_cDefaultTypes, &srItem));
		}

		Check(CreateSwatchItem(wzLabel, ARRAYSIZE(wzLabel) - cchPtr, SLP(L"entity"), cr, FALSE));
		Check(m_pRibbon->UpdateProperty(ID_PAINT_TYPE, &UI_PKEY_ItemsSource));
		*pidxType = m_cDefaultTypes;
	}
	else
		hr = E_ABORT;

Cleanup:
	RStrRelease(rstrSelectedNamespace);
	RStrRelease(rstrName);
	RStrRelease(rstrTitle);
	return hr;
}

HRESULT CRoomEditorApp::LoadPackage (PCWSTR pcwzPackage, BOOL fRequireAll)
{
	HRESULT hr;
	PACKAGE_DATA data = {0};
	RSTRING rstrWallsNamespace = NULL;
	RSTRING rstrEntitiesNamespace = NULL;

	data.pPackage = __new CSIFPackage;
	CheckAlloc(data.pPackage);

	Check(data.pPackage->OpenPackage(pcwzPackage));

	hr = LoadPackageWalls(data.pPackage, &data.psifWalls, &rstrWallsNamespace);
	CheckIf(FAILED(hr) && fRequireAll, hr);

	hr = LoadPackageEntities(data.pPackage, &data.pModels, &data.psifEntities, &rstrEntitiesNamespace);
	CheckIf(FAILED(hr) && fRequireAll, hr);

	if(rstrWallsNamespace && rstrEntitiesNamespace)
	{
		INT nResult;
		Check(RStrCompareRStr(rstrWallsNamespace, rstrEntitiesNamespace, &nResult));
		CheckIf(0 != nResult, E_FAIL);
	}

	CheckIfIgnore(NULL == rstrWallsNamespace && NULL == rstrEntitiesNamespace, S_FALSE);
	Check(m_mapPackages.Add(rstrWallsNamespace ? rstrWallsNamespace : rstrEntitiesNamespace, data));

Cleanup:
	RStrRelease(rstrWallsNamespace);
	RStrRelease(rstrEntitiesNamespace);

	if(S_OK != hr)
		FreePackage(data);
	return hr;
}

HRESULT CRoomEditorApp::DrawInfoPart (HDC hdc, IJSONObject* pInfo, LONG x, LONG y, BOOL fEntity)
{
	HRESULT hr;
	TStackRef<ISimbeyInterchangeFileLayer> srLayer;
	TStackRef<IJSONValue> srv;
	PCWSTR pcwzType, pcwzPtr;
	RSTRING rstrType = NULL, rstrNamespace = NULL;
	PACKAGE_DATA* pData;
	RECT rcLabel = { x - 8, y + 64, x + 72, y + 80 };

	Check(pInfo->FindNonNullValueW(L"type", &srv));
	Check(srv->GetString(&rstrType));

	pcwzType = RStrToWide(rstrType);
	pcwzPtr = TStrChr(pcwzType, L'.');

	CheckIf(NULL == pcwzPtr, E_UNEXPECTED);
	if(0 == TStrCchCmpAssert(pcwzType, static_cast<INT>(pcwzPtr - pcwzType) + 1, L"any."))
	{
		COLORREF cr;
		RECT rc = { x, y, x + 64, y + 64 };

		srv.Release();
		Check(pInfo->FindNonNullValueW(L"cr", &srv));
		Check(srv->GetDWord(&cr));

		HBRUSH hbr = CreateSolidBrush(cr);
		FillRect(hdc, &rc, hbr);
		DeleteObject(hbr);
	}
	else
	{
		SIF_SURFACE sifSurface;

		Check(RStrCreateW(static_cast<INT>(pcwzPtr - pcwzType), pcwzType, &rstrNamespace));
		Check(m_mapPackages.FindPtr(rstrNamespace, &pData));
		pcwzPtr++;

		if(fEntity)
		{
			TStackRef<IJSONObject> srEntity;
			RSTRING rstrName;

			Check(RStrCreateW(TStrLenAssert(pcwzPtr), pcwzPtr, &rstrName));
			hr = JSONFindArrayObject(pData->pModels, RSTRING_CAST(L"name"), rstrName, &srEntity, NULL);
			RStrRelease(rstrName);
			Check(hr);

			srv.Release();
			Check(srEntity->FindNonNullValueW(L"entity", &srv));
			Check(srv->GetString(&rstrName));

			hr = pData->psifEntities->FindLayer(RStrToWide(rstrName), &srLayer, NULL);
			RStrRelease(rstrName);
			Check(hr);
		}
		else
			Check(pData->psifWalls->FindLayer(pcwzPtr, &srLayer, NULL));

		m_Graph.GetRawBuffer(sifSurface.pbSurface, sifSurface.xSize, sifSurface.ySize, sifSurface.lPitch);
		sifSurface.cBitsPerPixel = 24;

		Check(srLayer->DrawToDIB24(&sifSurface, x, y));
	}

	DrawText(hdc, pcwzType, RStrLen(rstrType), &rcLabel, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

Cleanup:
	RStrRelease(rstrNamespace);
	RStrRelease(rstrType);
	return hr;
}

HRESULT CRoomEditorApp::LoadPackageWalls (CSIFPackage* pPackage, __deref_out ISimbeyInterchangeFile** ppSIF, __out RSTRING* prstrNamespace)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	TStackRef<IJSONObject> srWalls;
	TStackRef<CSIFPackage> srDirectory;
	RSTRING rstrNamespace = NULL, rstrSIF = NULL;

	Check(pPackage->GetJSONData(SLP(L"\\walls\\walls.json"), &srv));
	Check(srv->GetObject(&srWalls));
	srv.Release();

	Check(srWalls->FindNonNullValueW(L"namespace", &srv));
	Check(srv->GetString(&rstrNamespace));
	srv.Release();

	Check(srWalls->FindNonNullValueW(L"sif", &srv));
	Check(srv->GetString(&rstrSIF));

	Check(pPackage->OpenDirectory(SLP(L"walls"), &srDirectory));
	Check(srDirectory->OpenSIF(RStrToWide(rstrSIF), ppSIF));

	*prstrNamespace = rstrNamespace;
	rstrNamespace = NULL;

Cleanup:
	RStrRelease(rstrSIF);
	RStrRelease(rstrNamespace);
	return hr;
}

HRESULT CRoomEditorApp::LoadPackageEntities (CSIFPackage* pPackage, __deref_out IJSONArray** ppModels, __deref_out ISimbeyInterchangeFile** ppSIF, __out RSTRING* prstrNamespace)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	TStackRef<IJSONObject> srEntities;
	TStackRef<CSIFPackage> srDirectory;
	RSTRING rstrNamespace = NULL, rstrSIF = NULL;

	Check(pPackage->GetJSONData(SLP(L"\\models\\models.json"), &srv));
	Check(srv->GetObject(&srEntities));
	srv.Release();

	Check(srEntities->FindNonNullValueW(L"namespace", &srv));
	Check(srv->GetString(&rstrNamespace));
	srv.Release();

	Check(srEntities->FindNonNullValueW(L"models", &srv));
	Check(srv->GetArray(ppModels));

	Check(pPackage->OpenDirectory(SLP(L"\\models"), &srDirectory));
	Check(srDirectory->OpenSIF(L"entities.sif", ppSIF));

	*prstrNamespace = rstrNamespace;
	rstrNamespace = NULL;

Cleanup:
	RStrRelease(rstrSIF);
	RStrRelease(rstrNamespace);
	return hr;
}

VOID CRoomEditorApp::FreePackage (PACKAGE_DATA& data)
{
	if(data.psifWalls)
	{
		data.psifWalls->Close();
		SafeRelease(data.psifWalls);
	}

	if(data.psifEntities)
	{
		data.psifEntities->Close();
		SafeRelease(data.psifEntities);
	}

	SafeRelease(data.pModels);
	SafeRelease(data.pPackage);
}

HRESULT CRoomEditorApp::CalculateAveragePixelColor (ISimbeyInterchangeFileLayer* pLayer, __out COLORREF* pcr)
{
	HRESULT hr;
	PBYTE pBits;
	DWORD cb, r = 0, g = 0, b = 0, cPixels = 0;

	Check(pLayer->GetBitsPtr(&pBits, &cb));

	for(DWORD i = 0; i < cb; i += sizeof(DWORD))
	{
		if(pBits[i + 3])
		{
			r += pBits[i];
			g += pBits[i + 1];
			b += pBits[i + 2];
			cPixels++;
		}
	}

	*pcr = RGB(r / cPixels, g / cPixels, b / cPixels);

Cleanup:
	return hr;
}

HRESULT CRoomEditorApp::UpdateAppTitle (VOID)
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

	if(m_pRooms->HasSelection())
		Check(RStrFormatW(&rstrTitle, L"%ls - %ls - %r", c_wzAppTitle, pcwzFile, m_pRooms->m_pSelected->m_rstrName));
	else
		Check(RStrFormatW(&rstrTitle, L"%ls - %ls", c_wzAppTitle, pcwzFile));
	CheckIfGetLastError(!SetWindowText(m_hwnd, RStrToWide(rstrTitle)));

Cleanup:
	RStrRelease(rstrTitle);
	return hr;
}

HINSTANCE CRoomEditorApp::GetInstance (VOID)
{
	return m_hInstance;
}

VOID CRoomEditorApp::OnFinalDestroy (HWND hwnd)
{
	if(m_pRibbon)
		m_pRibbon->Unload();
}

HRESULT CRoomEditorApp::FinalizeAndShow (DWORD dwStyle, INT nCmdShow)
{
	HRESULT hr;

	Check(m_pRibbon->Initialize(m_hwnd, this));
	Check(m_pRibbon->SetModes(1));
	Registry::LoadWindowPosition(m_hwnd, c_wzAppKey, L"WindowPlacement", &nCmdShow);
	Check(CBaseWindow::FinalizeAndShow(dwStyle, nCmdShow));

Cleanup:
	return hr;
}

BOOL CRoomEditorApp::DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
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
