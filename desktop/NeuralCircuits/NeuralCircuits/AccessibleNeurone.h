#pragma once

class CAccessibleNeurone : public IAccessibleNeurone
{
private:
	ULONG m_cRef;

protected:
	IAccessibleNetDoc* m_lpParent;
	INetDocObject* m_lpObject;

	INT m_x, m_y;
	INT m_nWidth;
	INT m_nHeight;

	POINT* m_lpConnectors;	// Offsets to m_x, m_y
	INT m_cList;

public:
	CAccessibleNeurone (IAccessibleNetDoc* lpParent);
	virtual ~CAccessibleNeurone ();

	// IUnknown
	HRESULT WINAPI QueryInterface (REFIID iid, LPVOID* lplpvObject);
	ULONG WINAPI AddRef (VOID);
	ULONG WINAPI Release (VOID);

	// IDispatch
	HRESULT WINAPI GetTypeInfoCount (UINT* pctinfo);
	HRESULT WINAPI GetTypeInfo (UINT itinfo, LCID lcid, ITypeInfo** pptinfo);
	HRESULT WINAPI GetIDsOfNames (REFIID iid, OLECHAR** rgszNames, UINT cNames, LCID lcid, DISPID* rgdispid);
	HRESULT WINAPI Invoke (DISPID dispidMember, REFIID iid, LCID lcid, WORD wFlags, DISPPARAMS* pdispParams, VARIANT* pvarResult, EXCEPINFO* pexcepInfo, UINT* puArgErr);

	// IAccessible
	HRESULT WINAPI get_accParent (IDispatch** ppdispParent);
	HRESULT WINAPI get_accChildCount (LONG* pChildCount);
	HRESULT WINAPI get_accChild (VARIANT varChild, IDispatch** ppdispChild);

	HRESULT WINAPI get_accName (VARIANT varChild, BSTR* pszName);
	HRESULT WINAPI get_accValue (VARIANT varChild, BSTR* pszValue);
	HRESULT WINAPI get_accDescription (VARIANT varChild, BSTR* pszDescription);
	HRESULT WINAPI get_accRole (VARIANT varChild, VARIANT* pvarRole);
	HRESULT WINAPI get_accState (VARIANT varChild, VARIANT* pvarState);
	HRESULT WINAPI get_accHelp (VARIANT varChild, BSTR* pszHelp);
	HRESULT WINAPI get_accHelpTopic (BSTR* pszHelpFile, VARIANT varChild, LONG* pidTopic);
	HRESULT WINAPI get_accKeyboardShortcut (VARIANT varChild, BSTR* pszKeyboardShortcut);
	HRESULT WINAPI get_accFocus (VARIANT* pvarFocusChild);
	HRESULT WINAPI get_accSelection (VARIANT* pvarSelectedChildren);
	HRESULT WINAPI get_accDefaultAction (VARIANT varChild, BSTR* pszDefaultAction);

	HRESULT WINAPI accSelect (LONG flagsSelect, VARIANT varChild);
	HRESULT WINAPI accLocation (LONG* pxLeft, LONG* pyTop, LONG* pcxWidth, LONG* pcyHeight, VARIANT varChild);
	HRESULT WINAPI accNavigate (LONG navDir, VARIANT varStart, VARIANT* pvarEndUpAt);
	HRESULT WINAPI accHitTest (LONG xLeft, LONG yTop, VARIANT* pvarChildAtPoint);
	HRESULT WINAPI accDoDefaultAction (VARIANT varChild);

	HRESULT WINAPI put_accName (VARIANT varChild, BSTR bstrName);
	HRESULT WINAPI put_accValue (VARIANT varChild, BSTR bstrValue);

	// IAccessibleNeurone
	HRESULT Initialize (INetDocObject* lpObject, INT nWidth, INT nHeight);
	HRESULT InitConnectors (POINT* lpConnectors, INT cList);

	HRESULT UpdateConnectors (POINT* lpConnectors, INT cList);
	HRESULT Select (BOOL fSelect);
	HRESULT MoveTo (INT x, INT y);
	HRESULT Delete (VOID);

	VOID GetObjectRect (LPNRECT lpRect);

protected:
	inline LONG Child (const VARIANT& v)
	{
		if(v.vt == VT_I4)
			return v.lVal;
		return -1;
	};
};