#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\EnumVariant.h"
#include "Library\GraphCtrl.h"
#include "NeuralAPI.h"
#include "NeuralNet.h"
#include "NeuralDocument.h"
#include "AccessibleNetDoc.h"

CAccessibleNetDoc::CAccessibleNetDoc (CNeuralDocument* lpDoc, CGraphCtrlAdapter* pGraphAdapter)
{
	m_cRef = 1;

	m_lpDoc = lpDoc;
	m_lpDoc->AddRef();

	m_pGraphAdapter = pGraphAdapter;
	m_pGraphAdapter->AddRef();
	m_lpGraph = m_pGraphAdapter->GetCtrl();

	m_lpNet = NULL;

	m_hOleAcc = NULL;
	lpfnAccessibleObjectFromWindow = NULL;
}

CAccessibleNetDoc::~CAccessibleNetDoc ()
{
	if(m_hOleAcc)
		FreeLibrary(m_hOleAcc);

	Assert(m_lpDoc == NULL);
	Assert(m_lpGraph == NULL);
}

HRESULT CAccessibleNetDoc::Initialize (VOID)
{
	HRESULT hr = S_OK;
	if(m_hOleAcc == NULL)
	{
		m_hOleAcc = LoadLibrary("OLEACC.DLL");
		if(m_hOleAcc)
		{
			lpfnAccessibleObjectFromWindow = (HRESULT(WINAPI*)(HWND,DWORD,REFIID,LPVOID))GetProcAddress(m_hOleAcc,"AccessibleObjectFromWindow");

			if(lpfnAccessibleObjectFromWindow == NULL)
				hr = E_FAIL;
			else
				hr = NotifyAccEvent(EVENT_OBJECT_CREATE,NULL);
		}
		else
			hr = E_FAIL;
	}
	return hr;
}

VOID CAccessibleNetDoc::AttachNet (INeuralNet* lpNet)
{
	if(m_lpNet)
		m_lpNet->Release();
	m_lpNet = lpNet;
	if(m_lpNet)
		m_lpNet->AddRef();
}

VOID CAccessibleNetDoc::ReleaseDocument (VOID)
{
	NotifyAccEvent(EVENT_OBJECT_DESTROY,NULL);

	if(m_lpDoc)
	{
		m_lpDoc->Release();
		m_lpDoc = NULL;
	}

	if(m_pGraphAdapter)
	{
		m_pGraphAdapter->Release();
		m_pGraphAdapter = NULL;

		m_lpGraph = NULL;
	}
}

// IUnknown

HRESULT WINAPI CAccessibleNetDoc::QueryInterface (REFIID iid, LPVOID* lplpvObject)
{
	HRESULT hr = E_INVALIDARG;
	if(lplpvObject)
	{
		if(iid == IID_IAccessible)
			*lplpvObject = (IAccessible*)this;
		else if(iid == IID_IDispatch)
			*lplpvObject = (IDispatch*)this;
		else if(iid == IID_IUnknown)
			*lplpvObject = (IUnknown*)this;
		else
		{
			hr = E_NOINTERFACE;
			goto exit;
		}
		AddRef();
		hr = S_OK;
	}
exit:
	return hr;
}

ULONG WINAPI CAccessibleNetDoc::AddRef (VOID)
{
	return (ULONG)InterlockedIncrement((LONG*)&m_cRef);
}

ULONG WINAPI CAccessibleNetDoc::Release (VOID)
{
	ULONG c = (ULONG)InterlockedDecrement((LONG*)&m_cRef);
	if(c == 0)
		__delete this;
	return c;
}

// IDispatch

HRESULT WINAPI CAccessibleNetDoc::GetTypeInfoCount (UINT* pctinfo)
{
	UNREFERENCED_PARAMETER(pctinfo);

	return S_FALSE;
}

HRESULT WINAPI CAccessibleNetDoc::GetTypeInfo (UINT itinfo, LCID lcid, ITypeInfo** pptinfo)
{
	UNREFERENCED_PARAMETER(itinfo);
	UNREFERENCED_PARAMETER(lcid);
	UNREFERENCED_PARAMETER(pptinfo);

	return S_FALSE;
}

HRESULT WINAPI CAccessibleNetDoc::GetIDsOfNames (REFIID iid, OLECHAR** rgszNames, UINT cNames, LCID lcid, DISPID* rgdispid)
{
	UNREFERENCED_PARAMETER(iid);
	UNREFERENCED_PARAMETER(rgszNames);
	UNREFERENCED_PARAMETER(cNames);
	UNREFERENCED_PARAMETER(lcid);
	UNREFERENCED_PARAMETER(rgdispid);

	return S_FALSE;
}

HRESULT WINAPI CAccessibleNetDoc::Invoke (DISPID dispidMember, REFIID iid, LCID lcid, WORD wFlags, DISPPARAMS* pdispParams, VARIANT* pvarResult, EXCEPINFO* pexcepInfo, UINT* puArgErr)
{
	UNREFERENCED_PARAMETER(dispidMember);
	UNREFERENCED_PARAMETER(iid);
	UNREFERENCED_PARAMETER(lcid);
	UNREFERENCED_PARAMETER(wFlags);
	UNREFERENCED_PARAMETER(pdispParams);
	UNREFERENCED_PARAMETER(pvarResult);
	UNREFERENCED_PARAMETER(pexcepInfo);
	UNREFERENCED_PARAMETER(puArgErr);

	return S_FALSE;
}

// IAccessible

HRESULT WINAPI CAccessibleNetDoc::get_accParent (IDispatch** ppdispParent)
{
	HWND hwnd;
	HRESULT hr = m_lpDoc->GetHwnd(&hwnd);
	if(SUCCEEDED(hr))
		hr = lpfnAccessibleObjectFromWindow(hwnd,OBJID_WINDOW,IID_IDispatch,(LPVOID*)ppdispParent);
	return hr;
}

HRESULT WINAPI CAccessibleNetDoc::get_accChildCount (LONG* pChildCount)
{
	Assert(m_lpNet);

	*pChildCount = m_lpNet->GetObjectCount();
	return S_OK;
}

HRESULT WINAPI CAccessibleNetDoc::get_accChild (VARIANT varChild, IDispatch** ppdispChild)
{
	HRESULT hr = S_FALSE;
	INetDocObject* lpObject = m_lpNet->GetObject(Child(varChild) - 1);
	if(lpObject)
	{
		IAccessible* lpAccessible;
		if(SUCCEEDED(lpObject->GetAccObject(this,&lpAccessible)))
		{
			hr = lpAccessible->QueryInterface(IID_IDispatch,(LPVOID*)ppdispChild);
			lpAccessible->Release();
		}
	}
	return hr;
}

HRESULT WINAPI CAccessibleNetDoc::get_accName (VARIANT varChild, BSTR* pszName)
{
	HRESULT hr = S_FALSE;
	if(Child(varChild) == CHILDID_SELF)
	{
		*pszName = SysAllocString(L"Neural Document");
		if(*pszName)
			hr = S_OK;
		else
			hr = E_OUTOFMEMORY;
	}
	return hr;
}

HRESULT WINAPI CAccessibleNetDoc::get_accValue (VARIANT varChild, BSTR* pszValue)
{
	UNREFERENCED_PARAMETER(varChild);
	UNREFERENCED_PARAMETER(pszValue);

	return S_FALSE;
}

HRESULT WINAPI CAccessibleNetDoc::get_accDescription (VARIANT varChild, BSTR* pszDescription)
{
	UNREFERENCED_PARAMETER(varChild);
	UNREFERENCED_PARAMETER(pszDescription);

	return S_FALSE;
}

HRESULT WINAPI CAccessibleNetDoc::get_accRole (VARIANT varChild, VARIANT* pvarRole)
{
	HRESULT hr = S_FALSE;
	if(Child(varChild) == CHILDID_SELF)
	{
		pvarRole->vt = VT_I4;
		pvarRole->lVal = ROLE_SYSTEM_CHART;
		hr = S_OK;
	}
	return hr;
}

HRESULT WINAPI CAccessibleNetDoc::get_accState (VARIANT varChild, VARIANT* pvarState)
{
	HRESULT hr = S_FALSE;
	if(Child(varChild) == CHILDID_SELF)
	{
		pvarState->vt = VT_I4;
		pvarState->lVal = 0;	// STATE_SYSTEM_NORMAL
		hr = S_OK;
	}
	return hr;
}

HRESULT WINAPI CAccessibleNetDoc::get_accHelp (VARIANT varChild, BSTR* pszHelp)
{
	UNREFERENCED_PARAMETER(varChild);
	UNREFERENCED_PARAMETER(pszHelp);

	return S_FALSE;
}

HRESULT WINAPI CAccessibleNetDoc::get_accHelpTopic (BSTR* pszHelpFile, VARIANT varChild, LONG* pidTopic)
{
	UNREFERENCED_PARAMETER(pszHelpFile);
	UNREFERENCED_PARAMETER(varChild);
	UNREFERENCED_PARAMETER(pidTopic);

	return S_FALSE;
}

HRESULT WINAPI CAccessibleNetDoc::get_accKeyboardShortcut (VARIANT varChild, BSTR* pszKeyboardShortcut)
{
	UNREFERENCED_PARAMETER(varChild);
	UNREFERENCED_PARAMETER(pszKeyboardShortcut);

	return S_FALSE;
}

HRESULT WINAPI CAccessibleNetDoc::get_accFocus (VARIANT* pvarFocusChild)
{
	pvarFocusChild->vt = VT_I4;
	pvarFocusChild->lVal = CHILDID_SELF;
	return S_OK;
}

HRESULT WINAPI CAccessibleNetDoc::get_accSelection (VARIANT* pvarSelectedChildren)
{
	HRESULT hr = E_OUTOFMEMORY;
	CEnumVariant* lpEnum = __new CEnumVariant;
	if(lpEnum)
	{
		VARIANT* lpvDispatchList;
		ULONG cList;
		hr = m_lpDoc->GetSelection(&lpvDispatchList,&cList);
		if(SUCCEEDED(hr))
		{
			lpEnum->AttachList(lpvDispatchList,cList);
			pvarSelectedChildren->vt = VT_UNKNOWN;
			pvarSelectedChildren->punkVal = lpEnum;
		}
		else
			lpEnum->Release();
	}
	return hr;
}

HRESULT WINAPI CAccessibleNetDoc::get_accDefaultAction (VARIANT varChild, BSTR* pszDefaultAction)
{
	UNREFERENCED_PARAMETER(varChild);
	UNREFERENCED_PARAMETER(pszDefaultAction);

	return S_FALSE;
}

HRESULT WINAPI CAccessibleNetDoc::accSelect (LONG flagsSelect, VARIANT varChild)
{
	HRESULT hr = S_FALSE;
	LONG iChild = Child(varChild);
	if(iChild != CHILDID_SELF)
	{
		INetDocObject* lpObject = m_lpNet->GetObject(iChild - 1);
		if(flagsSelect & SELFLAG_ADDSELECTION)
			hr = m_lpDoc->AddToSelection(lpObject);
		else if(flagsSelect & SELFLAG_REMOVESELECTION)
			hr = (m_lpDoc->RemoveSelection(lpObject)) ? S_OK : S_FALSE;
	}
	return hr;
}

HRESULT WINAPI CAccessibleNetDoc::accLocation (LONG* pxLeft, LONG* pyTop, LONG* pcxWidth, LONG* pcyHeight, VARIANT varChild)
{
	HRESULT hr = S_FALSE;
	LONG lChild = Child(varChild);
	if(lChild == CHILDID_SELF)
	{
		HWND hwnd;
		RECT rect;
		hr = m_lpDoc->GetHwnd(&hwnd);
		if(SUCCEEDED(hr) && GetClientRect(hwnd,&rect))
		{
			ClientToScreen(hwnd,(LPPOINT)&rect);
			ClientToScreen(hwnd,(LPPOINT)&rect + 1);
			*pxLeft = rect.left;
			*pyTop = rect.top;
			*pcxWidth = rect.right - rect.left;
			*pcyHeight = rect.bottom - rect.top;
			hr = S_OK;
		}
	}
	else
	{
		INetDocObject* lpObject = m_lpNet->GetObject(lChild - 1);
		if(lpObject)
		{
			IAccessible* lpAccessible;
			hr = lpObject->GetAccObject(this,&lpAccessible);
			if(SUCCEEDED(hr))
			{
				VARIANT varSelf;
				varSelf.vt = VT_I4;
				varSelf.lVal = CHILDID_SELF;
				hr = lpAccessible->accLocation(pxLeft,pyTop,pcxWidth,pcyHeight,varSelf);
				lpAccessible->Release();
			}
		}
	}
	return hr;
}

HRESULT WINAPI CAccessibleNetDoc::accNavigate (LONG navDir, VARIANT varStart, VARIANT* pvarEndUpAt)
{
	HRESULT hr = S_OK;
	LONG iStart = Child(varStart);

	pvarEndUpAt->vt = VT_EMPTY;

	if(navDir == NAVDIR_FIRSTCHILD)
	{
		pvarEndUpAt->vt = VT_I4;
		pvarEndUpAt->lVal = 1;
	}
	else if(navDir == NAVDIR_LASTCHILD)
	{
		pvarEndUpAt->vt = VT_I4;
		pvarEndUpAt->lVal = m_lpNet->GetObjectCount();
	}
	else if(iStart != CHILDID_SELF)
	{
		if(navDir == NAVDIR_NEXT)
		{
			if(iStart < m_lpNet->GetObjectCount())
			{
				pvarEndUpAt->vt = VT_I4;
				pvarEndUpAt->lVal = iStart + 1;
			}
			else
				hr = S_FALSE;
		}
		else if(navDir == NAVDIR_PREVIOUS)
		{
			if(iStart > 1)
			{
				pvarEndUpAt->vt = VT_I4;
				pvarEndUpAt->lVal = iStart - 1;
			}
			else
				hr = S_FALSE;
		}
		else
			hr = S_FALSE;
	}

	return hr;
}

HRESULT WINAPI CAccessibleNetDoc::accHitTest (LONG xLeft, LONG yTop, VARIANT* pvarChildAtPoint)
{
	HRESULT hr = S_FALSE;
	POINT ptScreen = {xLeft, yTop};
	FLOAT fx, fy;
	if(SUCCEEDED(ScreenToGraph(ptScreen, fx, fy)))
	{
		INetDocObject* lpObject;
		lpObject = m_lpNet->FindObject((INT)fx,(INT)fy);
		if(lpObject)
			pvarChildAtPoint->lVal = m_lpNet->GetObjectIndex(lpObject) + 1;
		else
			pvarChildAtPoint->lVal = CHILDID_SELF;
		pvarChildAtPoint->vt = VT_I4;
		hr = S_OK;
	}
	return hr;
}

HRESULT WINAPI CAccessibleNetDoc::accDoDefaultAction (VARIANT varChild)
{
	UNREFERENCED_PARAMETER(varChild);

	return S_FALSE;
}

HRESULT WINAPI CAccessibleNetDoc::put_accName (VARIANT varChild, BSTR bstrName)
{
	UNREFERENCED_PARAMETER(varChild);
	UNREFERENCED_PARAMETER(bstrName);

	return S_FALSE;
}

HRESULT WINAPI CAccessibleNetDoc::put_accValue (VARIANT varChild, BSTR bstrValue)
{
	UNREFERENCED_PARAMETER(varChild);
	UNREFERENCED_PARAMETER(bstrValue);

	return S_FALSE;
}

HRESULT CAccessibleNetDoc::NotifyAccEvent (DWORD dwEvent, INetDocObject* lpObject)
{
	HRESULT hr = S_FALSE;
	if(m_lpDoc)
	{
		HWND hwnd;
		hr = m_lpDoc->GetHwnd(&hwnd);
		if(SUCCEEDED(hr))
		{
			LONG idChild = CHILDID_SELF;
			if(lpObject)
			{
				idChild = m_lpNet->GetObjectIndex(lpObject);
				if(idChild != -1)
					idChild++;
			}
			if(idChild != -1)
				::NotifyWinEvent(dwEvent,hwnd,OBJID_CLIENT,idChild);
			else
				hr = S_FALSE;
		}
	}
	return hr;
}

VOID CAccessibleNetDoc::GraphToClient (FLOAT gx, FLOAT gy, LONG* px, LONG* py)
{
	INT xClient, yClient;
	Assert(px && py);
	m_lpGraph->GraphToClient(gx,gy,xClient,yClient);
	*px = xClient;
	*py = yClient;
}

FLOAT CAccessibleNetDoc::GetGraphScale (VOID)
{
	return m_lpGraph->GetScale();
}

HRESULT CAccessibleNetDoc::ScreenToGraph (POINT& ptScreen, FLOAT& fxGraph, FLOAT& fyGraph)
{
	POINT pt = ptScreen;
	HRESULT hr = m_lpDoc->ScreenToWindowless(NULL,&pt);
	if(SUCCEEDED(hr))
		m_lpGraph->ClientToGraph(pt.x,pt.y,fxGraph,fyGraph);
	return hr;
}