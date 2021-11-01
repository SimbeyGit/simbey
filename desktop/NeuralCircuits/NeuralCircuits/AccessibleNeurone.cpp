#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "NeuralAPI.h"
#include "Neurone.h"
#include "AccessibleNetDoc.h"
#include "AccessibleNeurone.h"

CAccessibleNeurone::CAccessibleNeurone (IAccessibleNetDoc* lpParent)
{
	m_cRef = 1;

	m_lpParent = lpParent;
	m_lpParent->AddRef();

	m_lpObject = NULL;

	m_lpConnectors = NULL;
	m_cList = 0;
}

CAccessibleNeurone::~CAccessibleNeurone ()
{
	m_lpParent->Release();
}

HRESULT CAccessibleNeurone::Initialize (INetDocObject* lpObject, INT nWidth, INT nHeight)
{
	m_lpObject = lpObject;
	m_lpObject->AddRef();

	m_lpObject->GetPosition(m_x,m_y);

	m_nWidth = nWidth;
	m_nHeight = nHeight;

	return S_OK;
}

HRESULT CAccessibleNeurone::InitConnectors (POINT* lpConnectors, INT cList)
{
	HRESULT hr = S_OK;

	Assert(m_lpConnectors == NULL);

	if(cList > 0)
	{
		m_lpConnectors = __new POINT[cList];
		if(m_lpConnectors)
		{
			CopyMemory(m_lpConnectors,lpConnectors,sizeof(POINT) * cList);
			m_cList = cList;
		}
		else
			hr = E_OUTOFMEMORY;
	}

	return hr;
}

HRESULT CAccessibleNeurone::UpdateConnectors (POINT* lpConnectors, INT cList)
{
	HRESULT hr;

	__delete_array m_lpConnectors;
	m_cList = 0;

	hr = InitConnectors(lpConnectors,cList);
	if(SUCCEEDED(hr))
		hr = m_lpParent->NotifyAccEvent(EVENT_OBJECT_LOCATIONCHANGE,m_lpObject);

	return hr;
}

HRESULT CAccessibleNeurone::Select (BOOL fSelect)
{
	return m_lpParent->NotifyAccEvent((fSelect) ? EVENT_OBJECT_SELECTIONADD : EVENT_OBJECT_SELECTIONREMOVE,m_lpObject);
}

HRESULT CAccessibleNeurone::MoveTo (INT x, INT y)
{
	m_x = x;
	m_y = y;

	return m_lpParent->NotifyAccEvent(EVENT_OBJECT_LOCATIONCHANGE,m_lpObject);
}

HRESULT CAccessibleNeurone::Delete (VOID)
{
	return m_lpParent->NotifyAccEvent(EVENT_OBJECT_REORDER,NULL);
}

VOID CAccessibleNeurone::GetObjectRect (LPNRECT lpRect)
{
	FLOAT fWidth = (FLOAT)m_nWidth;
	FLOAT fHeight = (FLOAT)m_nHeight;
	FLOAT x;

	lpRect->left = fWidth / -2.0f;
	lpRect->top = fHeight / 2.0f;
	lpRect->right = fWidth / 2.0f;
	lpRect->bottom = fHeight / -2.0f;

	// Only the x component needs to be checked.
	for(INT i = 0; i < m_cList; i++)
	{
		x = (FLOAT)m_lpConnectors[i].x;

		if(x - AXON_RADIUS < lpRect->left)
			lpRect->left = x - AXON_RADIUS;
		else if(x + AXON_RADIUS > lpRect->right)
			lpRect->right = x + AXON_RADIUS;
	}
}

// IUnknown

HRESULT WINAPI CAccessibleNeurone::QueryInterface (REFIID iid, LPVOID* lplpvObject)
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

ULONG WINAPI CAccessibleNeurone::AddRef (VOID)
{
	return (ULONG)InterlockedIncrement((LONG*)&m_cRef);
}

ULONG WINAPI CAccessibleNeurone::Release (VOID)
{
	ULONG c = (ULONG)InterlockedDecrement((LONG*)&m_cRef);
	if(c == 0)
		__delete this;
	return c;
}

// IDispatch

HRESULT WINAPI CAccessibleNeurone::GetTypeInfoCount (UINT* pctinfo)
{
	UNREFERENCED_PARAMETER(pctinfo);

	return S_FALSE;
}

HRESULT WINAPI CAccessibleNeurone::GetTypeInfo (UINT itinfo, LCID lcid, ITypeInfo** pptinfo)
{
	UNREFERENCED_PARAMETER(itinfo);
	UNREFERENCED_PARAMETER(lcid);
	UNREFERENCED_PARAMETER(pptinfo);

	return S_FALSE;
}

HRESULT WINAPI CAccessibleNeurone::GetIDsOfNames (REFIID iid, OLECHAR** rgszNames, UINT cNames, LCID lcid, DISPID* rgdispid)
{
	UNREFERENCED_PARAMETER(iid);
	UNREFERENCED_PARAMETER(rgszNames);
	UNREFERENCED_PARAMETER(cNames);
	UNREFERENCED_PARAMETER(lcid);
	UNREFERENCED_PARAMETER(rgdispid);

	return S_FALSE;
}

HRESULT WINAPI CAccessibleNeurone::Invoke (DISPID dispidMember, REFIID iid, LCID lcid, WORD wFlags, DISPPARAMS* pdispParams, VARIANT* pvarResult, EXCEPINFO* pexcepInfo, UINT* puArgErr)
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

HRESULT WINAPI CAccessibleNeurone::get_accParent (IDispatch** ppdispParent)
{
	*ppdispParent = m_lpParent;
	(*ppdispParent)->AddRef();

	return S_OK;
}

HRESULT WINAPI CAccessibleNeurone::get_accChildCount (LONG* pChildCount)
{
	*pChildCount = m_cList;
	return S_OK;
}

HRESULT WINAPI CAccessibleNeurone::get_accChild (VARIANT varChild, IDispatch** ppdispChild)
{
	UNREFERENCED_PARAMETER(varChild);
	UNREFERENCED_PARAMETER(ppdispChild);

	return S_FALSE;
}

HRESULT WINAPI CAccessibleNeurone::get_accName (VARIANT varChild, BSTR* pszName)
{
	HRESULT hr = S_FALSE;
	if(Child(varChild) == CHILDID_SELF)
		*pszName = SysAllocString(L"Neurone");
	else
		*pszName = SysAllocString(L"Node");
	if(*pszName)
		hr = S_OK;
	else
		hr = E_OUTOFMEMORY;
	return hr;
}

HRESULT WINAPI CAccessibleNeurone::get_accValue (VARIANT varChild, BSTR* pszValue)
{
	UNREFERENCED_PARAMETER(varChild);
	UNREFERENCED_PARAMETER(pszValue);

	return S_FALSE;
}

HRESULT WINAPI CAccessibleNeurone::get_accDescription (VARIANT varChild, BSTR* pszDescription)
{
	UNREFERENCED_PARAMETER(varChild);
	UNREFERENCED_PARAMETER(pszDescription);

	return S_FALSE;
}

HRESULT WINAPI CAccessibleNeurone::get_accRole (VARIANT varChild, VARIANT* pvarRole)
{
	pvarRole->vt = VT_I4;
	if(Child(varChild) == CHILDID_SELF)
		pvarRole->lVal = ROLE_SYSTEM_GRAPHIC;
	else
		pvarRole->lVal = ROLE_SYSTEM_DIAL;
	return S_OK;
}

HRESULT WINAPI CAccessibleNeurone::get_accState (VARIANT varChild, VARIANT* pvarState)
{
	pvarState->vt = VT_I4;
	if(Child(varChild) == CHILDID_SELF)
		pvarState->lVal = m_lpObject->GetAccState();
	else
		pvarState->lVal = 0;
	return S_OK;
}

HRESULT WINAPI CAccessibleNeurone::get_accHelp (VARIANT varChild, BSTR* pszHelp)
{
	UNREFERENCED_PARAMETER(varChild);
	UNREFERENCED_PARAMETER(pszHelp);

	return S_FALSE;
}

HRESULT WINAPI CAccessibleNeurone::get_accHelpTopic (BSTR* pszHelpFile, VARIANT varChild, LONG* pidTopic)
{
	UNREFERENCED_PARAMETER(pszHelpFile);
	UNREFERENCED_PARAMETER(varChild);
	UNREFERENCED_PARAMETER(pidTopic);

	return S_FALSE;
}

HRESULT WINAPI CAccessibleNeurone::get_accKeyboardShortcut (VARIANT varChild, BSTR* pszKeyboardShortcut)
{
	UNREFERENCED_PARAMETER(varChild);
	UNREFERENCED_PARAMETER(pszKeyboardShortcut);

	return S_FALSE;
}

HRESULT WINAPI CAccessibleNeurone::get_accFocus (VARIANT* pvarFocusChild)
{
	pvarFocusChild->vt = VT_I4;
	pvarFocusChild->lVal = CHILDID_SELF;
	return S_OK;
}

HRESULT WINAPI CAccessibleNeurone::get_accSelection (VARIANT* pvarSelectedChildren)
{
	UNREFERENCED_PARAMETER(pvarSelectedChildren);

	return S_FALSE;
}

HRESULT WINAPI CAccessibleNeurone::get_accDefaultAction (VARIANT varChild, BSTR* pszDefaultAction)
{
	UNREFERENCED_PARAMETER(varChild);
	UNREFERENCED_PARAMETER(pszDefaultAction);

	return S_FALSE;
}

HRESULT WINAPI CAccessibleNeurone::accSelect (LONG flagsSelect, VARIANT varChild)
{
	UNREFERENCED_PARAMETER(flagsSelect);
	UNREFERENCED_PARAMETER(varChild);

	return S_FALSE;
}

HRESULT WINAPI CAccessibleNeurone::accLocation (LONG* pxLeft, LONG* pyTop, LONG* pcxWidth, LONG* pcyHeight, VARIANT varChild)
{
	HRESULT hr;
	VARIANT varParent;
	LONG x, y, nWidth, nHeight;
	varParent.vt = VT_I4;
	varParent.lVal = CHILDID_SELF;
	hr = m_lpParent->accLocation(&x,&y,&nWidth,&nHeight,varParent);
	if(SUCCEEDED(hr))
	{
		FLOAT fScale = m_lpParent->GetGraphScale();

		if(Child(varChild) == CHILDID_SELF)
		{
			NRECT rc;
			GetObjectRect(&rc);
			m_lpParent->GraphToClient((FLOAT)m_x + rc.left,(FLOAT)m_y + rc.top,pxLeft,pyTop);
			*pcxWidth = (LONG)((rc.right - rc.left) * fScale);
			*pcyHeight = (LONG)((rc.top - rc.bottom) * fScale);
		}
		else if(varChild.lVal <= m_cList)
		{
			POINT* lppt = &m_lpConnectors[varChild.lVal - 1];
			m_lpParent->GraphToClient((FLOAT)(m_x + lppt->x) - AXON_RADIUS,(FLOAT)(m_y + lppt->y) + AXON_RADIUS,pxLeft,pyTop);
			*pcxWidth = (LONG)((FLOAT)(AXON_RADIUS * 2) * fScale);
			*pcyHeight = *pcxWidth;;
		}
		else
			hr = E_FAIL;

		*pxLeft += x;
		*pyTop += y;
	}
	return hr;
}

HRESULT WINAPI CAccessibleNeurone::accNavigate (LONG navDir, VARIANT varStart, VARIANT* pvarEndUpAt)
{
	UNREFERENCED_PARAMETER(navDir);
	UNREFERENCED_PARAMETER(varStart);
	UNREFERENCED_PARAMETER(pvarEndUpAt);

	return S_FALSE;
}

HRESULT WINAPI CAccessibleNeurone::accHitTest (LONG xLeft, LONG yTop, VARIANT* pvarChildAtPoint)
{
	HRESULT hr = S_FALSE;
	POINT ptScreen = {xLeft, yTop};
	FLOAT fx, fy;
	if(SUCCEEDED(m_lpParent->ScreenToGraph(ptScreen,fx,fy)))
	{
		POINT pt = {(LONG)(fx - m_x), (LONG)(fy - m_y)};

		pvarChildAtPoint->vt = VT_I4;
		pvarChildAtPoint->lVal = CHILDID_SELF;

		for(INT i = 0; i < m_cList; i++)
		{
			if(pt.y <= m_lpConnectors[i].y + AXON_RADIUS && pt.y > m_lpConnectors[i].y - AXON_RADIUS)
			{
				if(pt.x >= m_lpConnectors[i].x - AXON_RADIUS && pt.x < m_lpConnectors[i].x + AXON_RADIUS)
				{
					pvarChildAtPoint->lVal = i + 1;
					break;
				}
			}
		}

		hr = S_OK;
	}
	return hr;
}

HRESULT WINAPI CAccessibleNeurone::accDoDefaultAction (VARIANT varChild)
{
	UNREFERENCED_PARAMETER(varChild);

	return S_FALSE;
}

HRESULT WINAPI CAccessibleNeurone::put_accName (VARIANT varChild, BSTR bstrName)
{
	UNREFERENCED_PARAMETER(varChild);
	UNREFERENCED_PARAMETER(bstrName);

	return S_FALSE;
}

HRESULT WINAPI CAccessibleNeurone::put_accValue (VARIANT varChild, BSTR bstrValue)
{
	UNREFERENCED_PARAMETER(varChild);
	UNREFERENCED_PARAMETER(bstrValue);

	return S_FALSE;
}