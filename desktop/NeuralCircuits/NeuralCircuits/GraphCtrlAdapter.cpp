#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "BaseContainer.h"
#include "GraphCtrlAdapter.h"

CGraphCtrlAdapter::CGraphCtrlAdapter () :
	m_cRef(1),
	m_pContainer(NULL),
	m_pCtrl(NULL)
{
}

CGraphCtrlAdapter::~CGraphCtrlAdapter ()
{
	if(m_pCtrl)
	{
		m_pCtrl->AttachContainer(NULL);
		__delete m_pCtrl;
	}
}

// IUnknown

HRESULT WINAPI CGraphCtrlAdapter::QueryInterface (REFIID iid, LPVOID* lplpvObject)
{
	UNREFERENCED_PARAMETER(iid);
	UNREFERENCED_PARAMETER(lplpvObject);

	return E_NOTIMPL;
}

ULONG WINAPI CGraphCtrlAdapter::AddRef (VOID)
{
	return (ULONG)InterlockedIncrement((LONG*)&m_cRef);
}

ULONG WINAPI CGraphCtrlAdapter::Release (VOID)
{
	ULONG c = (ULONG)InterlockedDecrement((LONG*)&m_cRef);
	if(c == 0)
		__delete this;
	return c;
}

// IWindowless

VOID WINAPI CGraphCtrlAdapter::AttachContainer (IBaseContainer* lpContainer)
{
	ReplaceInterface(m_pContainer, lpContainer);
	m_pCtrl->AttachContainer((m_pContainer) ? this : NULL);
}

VOID WINAPI CGraphCtrlAdapter::Paint (HDC hdc)
{
	return m_pCtrl->Paint(hdc);
}

VOID WINAPI CGraphCtrlAdapter::Move (LPRECT lpPosition)
{
	return m_pCtrl->Move(lpPosition);
}

VOID WINAPI CGraphCtrlAdapter::SizeObject (INT nWidth, INT nHeight)
{
	return m_pCtrl->SizeObject(nWidth, nHeight);
}

BOOL WINAPI CGraphCtrlAdapter::OnMessage (UINT msg, WPARAM wParam, LPARAM lParam, __out LRESULT& lResult)
{
	return m_pCtrl->OnMessage(msg, wParam, lParam, lResult);
}

HRESULT WINAPI CGraphCtrlAdapter::GetAccObject (IAccessible** lplpAccessible)
{
	return m_pCtrl->GetAccObject(lplpAccessible);
}

// IGraphContainer

HRESULT WINAPI CGraphCtrlAdapter::SetFocus (__in IGrapher* pGraphCtrl)
{
	UNREFERENCED_PARAMETER(pGraphCtrl);

	return m_pContainer->SetFocus(this);
}

HRESULT WINAPI CGraphCtrlAdapter::ScreenToWindowless (__in IGrapher* pGraphCtrl, __inout PPOINT ppt)
{
	UNREFERENCED_PARAMETER(pGraphCtrl);

	return m_pContainer->ScreenToWindowless(this, ppt);
}

HRESULT WINAPI CGraphCtrlAdapter::InvalidateContainer (__in IGrapher* pGraphCtrl)
{
	UNREFERENCED_PARAMETER(pGraphCtrl);

	return m_pContainer->InvalidateContainer();
}

VOID WINAPI CGraphCtrlAdapter::DrawDib24 (HDC hdcDest, LONG x, LONG y, HDC hdcSrc, const BYTE* pcDIB24, LONG xSize, LONG ySize, LONG lPitch)
{
	UNREFERENCED_PARAMETER(pcDIB24);
	UNREFERENCED_PARAMETER(lPitch);

	BitBlt(hdcDest, x, y, xSize, ySize, hdcSrc, 0, 0, SRCCOPY);
}

BOOL WINAPI CGraphCtrlAdapter::CaptureMouse (__in IGrapher* pGraphCtrl, BOOL fCapture)
{
	UNREFERENCED_PARAMETER(pGraphCtrl);

	return SUCCEEDED(m_pContainer->SetCapture(this, fCapture));
}

HRESULT CGraphCtrlAdapter::Initialize (VOID)
{
	HRESULT hr;

	Assert(NULL == m_pCtrl);

	m_pCtrl = __new CGraphCtrl;
	if(m_pCtrl)
		hr = S_OK;
	else
		hr = E_OUTOFMEMORY;

	return hr;
}

CGraphCtrl* CGraphCtrlAdapter:: GetCtrl (VOID)
{
	return m_pCtrl;
}