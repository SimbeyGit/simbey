#include <windows.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "Library\Core\MemoryStream.h"
#include "Library\Util\Formatting.h"
#include "Library\Util\FileStream.h"
#include "Library\Util\StreamHelpers.h"
#include "Library\DPI.h"
#include "Published\JSON.h"
#include "Ribbon.h"
#include "RibbonMappings.h"
#include "RibbonWindow.h"

const WCHAR c_wzAppClassName[] = L"RibbonWindowCls";
const WCHAR c_wzAppTitle[] = L"Ribbon Window";

CRibbonWindow::CRibbonWindow (HINSTANCE hInstance) :
	m_hInstance(hInstance),
	m_pRibbon(NULL),
	m_fActive(FALSE),
	m_pGalleryImages(NULL)
{
}

CRibbonWindow::~CRibbonWindow ()
{
	SafeRelease(m_pRibbon);
}

HRESULT CRibbonWindow::Register (HINSTANCE hInstance)
{
	WNDCLASSEX wnd = {0};

	wnd.cbSize = sizeof(WNDCLASSEX);
	wnd.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
	wnd.cbClsExtra = 0;
	wnd.cbWndExtra = 0;
	wnd.hInstance = hInstance;
	wnd.hIcon = LoadIcon(hInstance, MAKEINTRESOURCEW(IDI_MAIN));
	wnd.hCursor = LoadCursor(NULL, IDC_ARROW);
	wnd.hbrBackground = NULL;
	wnd.lpszMenuName = NULL;
	wnd.lpszClassName = c_wzAppClassName;

	return RegisterClass(&wnd, NULL);
}

HRESULT CRibbonWindow::Unregister (HINSTANCE hInstance)
{
	return UnregisterClass(c_wzAppClassName, hInstance);
}

HRESULT CRibbonWindow::Initialize (INT nWidth, INT nHeight, INT nCmdShow)
{
	HRESULT hr;
	RECT rect = { 0, 0, nWidth, nHeight };
	ISimbeyInterchangeFile* pSIF = NULL;

	Check(CSIFRibbon::Create(DPI::Scale, &m_pRibbon));
	Check(sifLoadResource(m_hInstance, L"SIF16_144", &m_pGalleryImages));

	CheckIfGetLastError(!AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE));
	Check(Create(WS_EX_APPWINDOW | WS_EX_WINDOWEDGE, WS_OVERLAPPEDWINDOW,
		c_wzAppClassName, c_wzAppTitle, CW_USEDEFAULT, CW_USEDEFAULT,
		rect.right - rect.left, rect.bottom - rect.top, NULL, nCmdShow));

Cleanup:
	if(FAILED(hr) && m_hwnd)
		Destroy();
	return hr;
}

// IRibbonHost

HRESULT WINAPI CRibbonWindow::GetRibbonSettingsKey (__out_ecount(cchMaxKey) PSTR pszKeyName, INT cchMaxKey)
{
	return TStrCchCpy(pszKeyName, cchMaxKey, "Software\\Simbey\\RibbonWindow");
}

HRESULT WINAPI CRibbonWindow::GetRibbonSettingsValue (__out_ecount(cchMaxValue) PSTR pszValueName, INT cchMaxValue)
{
	return TStrCchCpy(pszValueName, cchMaxValue, "RibbonSettings");
}

HRESULT WINAPI CRibbonWindow::GetRibbonResource (__out HMODULE* phModule, __out_ecount(cchMaxResource) PWSTR pwzResource, INT cchMaxResource)
{
	*phModule = GetModuleHandle(NULL);
	return TStrCchCpy(pwzResource, cchMaxResource, L"APPLICATION_RIBBON");
}

UINT32 WINAPI CRibbonWindow::TranslateGroupToImage (UINT32 nID)
{
	// Use the tool-generated MapGroupsToImages() function.
	return MapGroupsToImages(nID);
}

UINT32 WINAPI CRibbonWindow::TranslateImageToLargeImage (UINT32 nID)
{
	// Use the tool-generated MapImagesToLargeImages() function.
	return MapImagesToLargeImages(nID);
}

// IUIApplication

HRESULT STDMETHODCALLTYPE CRibbonWindow::OnViewChanged (UINT32 viewId, UI_VIEWTYPE typeID, IUnknown* view, UI_VIEWVERB verb, INT32 uReasonCode)
{
	HRESULT hr = S_OK;

	switch(verb)
	{
	case UI_VIEWVERB_CREATE:
		break;
	case UI_VIEWVERB_DESTROY:
		if(m_pRibbon)
			m_pRibbon->SaveSettings();
		break;
	case UI_VIEWVERB_SIZE:
		InvalidateRect(m_hwnd, NULL, FALSE);
		break;
	}

	return hr;
}

HRESULT STDMETHODCALLTYPE CRibbonWindow::OnCreateUICommand (UINT32 commandId, UI_COMMANDTYPE typeID, IUICommandHandler** commandHandler)
{
	HRESULT hr;

	*commandHandler = this;
	AddRef();
	hr = S_OK;

//Cleanup:
	return hr;
}

HRESULT STDMETHODCALLTYPE CRibbonWindow::OnDestroyUICommand (UINT32 commandId, UI_COMMANDTYPE typeID, IUICommandHandler* commandHandler)
{
	return S_OK;
}

// IUICommandHandler

HRESULT STDMETHODCALLTYPE CRibbonWindow::Execute (UINT32 commandId, UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* currentValue, IUISimplePropertySet* commandExecutionProperties)
{
	HRESULT hr;

	switch(commandId)
	{
	case ID_EXIT:
		PostMessage(m_hwnd, WM_CLOSE, 0, 0);
		hr = S_OK;
		break;
	case ID_GENERIC:
		MessageBox(m_hwnd, L"Generic Button", L"Generic Title", MB_OK);
		hr = S_OK;
		break;
	case ID_GALLERY:
		MessageBox(m_hwnd, L"Generic Gallery", L"Generic Title", MB_OK);
		hr = S_OK;
		break;
	case ID_DROPDOWN:
		MessageBox(m_hwnd, L"Generic DropDown", L"Generic Title", MB_OK);
		hr = S_OK;
		break;
	default:
		hr = E_NOTIMPL;
		break;
	}

	return hr;
}

HRESULT STDMETHODCALLTYPE CRibbonWindow::UpdateProperty (UINT32 commandId, REFPROPERTYKEY key, const PROPVARIANT* currentValue, PROPVARIANT* newValue)
{
	HRESULT hr;

	if(UI_PKEY_LargeImage == key || UI_PKEY_SmallImage == key)
	{
		hr = m_pRibbon->LoadImageForCommand(commandId, key, newValue);
	}
	else if(UI_PKEY_ItemsSource == key)
	{
		TStackRef<IUICollection> srItems;
		TStackRef<CSIFRibbonItem> srItem;

		Check(currentValue->punkVal->QueryInterface(&srItems));

		srItems->Clear();

		Check(CSIFRibbonItem::Create(m_pRibbon, m_pGalleryImages, &srItem));
		srItem->SetItemIcon(ID_GENERIC);
		Check(srItem->SetItemText(L"Generic"));
		Check(srItems->Add(srItem.StaticCast<IUISimplePropertySet>()));
	}
	else
		hr = E_NOTIMPL;

Cleanup:
	return hr;
}

HINSTANCE CRibbonWindow::GetInstance (VOID)
{
	return m_hInstance;
}

VOID CRibbonWindow::OnFinalDestroy (HWND hwnd)
{
	if(m_pRibbon)
		m_pRibbon->Unload();

	if(m_pGalleryImages)
	{
		m_pGalleryImages->Close();
		SafeRelease(m_pGalleryImages);
	}

	PostQuitMessage(0);
}

HRESULT CRibbonWindow::FinalizeAndShow (DWORD dwStyle, INT nCmdShow)
{
	HRESULT hr;

	Check(m_pRibbon->Initialize(m_hwnd, this));
	Check(m_pRibbon->SetModes(1));
	Check(__super::FinalizeAndShow(dwStyle, nCmdShow));

Cleanup:
	return hr;
}

BOOL CRibbonWindow::OnCreate (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	return FALSE;
}

BOOL CRibbonWindow::OnPaint (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	PAINTSTRUCT ps;
	UINT32 nHeight = m_pRibbon->GetHeight();
	HDC hdc = BeginPaint(m_hwnd, &ps);

	if(ps.rcPaint.top < nHeight)
		ps.rcPaint.top = nHeight;
	FillRect(hdc, &ps.rcPaint, (HBRUSH)GetStockObject(WHITE_BRUSH));

	EndPaint(m_hwnd, &ps);
	lResult = TRUE;
	return TRUE;
}

BOOL CRibbonWindow::OnEraseBackground (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	lResult = TRUE;
	return TRUE;
}

BOOL CRibbonWindow::OnSize (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	return FALSE;
}

BOOL CRibbonWindow::OnClose (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	return FALSE;
}

BOOL CRibbonWindow::OnActivate (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	m_fActive = (WA_INACTIVE != LOWORD(wParam));
	return FALSE;
}

BOOL CRibbonWindow::OnDestroy (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	return FALSE;
}
