#pragma once

#include <OleAcc.h>
#include "Neurone.h"

class CInputPad : public INetDocObject, public IOleCommandTarget, public IAccessible
{
private:
	volatile ULONG m_cRef;

protected:
	INT m_x;
	INT m_y;

	RECT m_rcPos;
	INT m_cSideLength;
	LPBYTE m_lpbPattern;
	LPCLIST m_lpInput;
	POINT* m_lpCells;

	FLOAT m_fSetCell;
	FLOAT m_fClearCell;

	BOOL m_fSelected;

	IAccessibleNetDoc* m_lpParent;

public:
	CInputPad ();
	~CInputPad ();

	// IUnknown
	HRESULT WINAPI QueryInterface (REFIID iid, LPVOID* lplpvObject);
	ULONG WINAPI AddRef (VOID);
	ULONG WINAPI Release (VOID);

	// INetCycleProcessor
	virtual VOID SendPulses (VOID);
	virtual VOID CheckThresholds (VOID);

	// INetObject
	virtual HRESULT GetObjectClass (LPSTR lpszClass, INT cchMaxClass, __out INT* pcchClass);
	virtual VOID GetPosition (INT& x, INT& y);
	virtual VOID ReceiveValue (FLOAT fValue, ULONG iPin);
	virtual ULONG GetInputPin (INT x, INT y);
	virtual BOOL GetInputPinPosition (ULONG iPin, INT& x, INT& y);
	virtual HRESULT Load (INeuralFactory* pFactory, LPNLOADDATA lpLoadData, DWORD cLoadData, LPBYTE lpData, ULONG cbData);
	virtual HRESULT Save (INeuralNet* lpNet, ISequentialStream* lpStream);

	// INetDocObject
	virtual INT GetZOrder (VOID);
	virtual VOID DrawBackground (IGrapher* lpGraph);
	virtual VOID DrawForeground (IGrapher* lpGraph);
	virtual VOID SelectObject (BOOL fSelect);
	virtual VOID MoveObject (INT xDelta, INT yDelta);
	virtual VOID ResizeObject (INT nHitTest, INT xResizePos, INT yResizePos, INT xDelta, INT yDelta);
	virtual INT HitTest (INT x, INT y);
	virtual BOOL GetDragSourcePin (INT x, INT y, ULONG& iPin);
	virtual BOOL GetDragSourcePoint (ULONG iPin, INT& x, INT& y);
	virtual BOOL HighlightPin (ULONG iPin, BOOL fHighlight);
	virtual BOOL HighlightConn (ULONG iPin, INT index);
	virtual VOID NotifyRemovalOf (INetDocObject* lpObject);
	virtual HRESULT ConnectTo (ULONG iSourcePin, INetDocObject* lpTarget, ULONG iTargetPin, FLOAT fWeight);
	virtual HRESULT ContextMenu (IBaseContainer* lpContainer, INT x, INT y);
	virtual BOOL Click (INT x, INT y);
	virtual BOOL PressChar (CHAR ch);
	virtual LONG GetAccState (VOID);
	virtual HRESULT GetAccObject (IAccessibleNetDoc* lpParent, IAccessible** lplpAccessible);
	virtual HRESULT UnloadAccessibility (VOID);
	virtual HRESULT GetGraphInputCaptureObject (INeuralDocument* lpDocument, IGraphInputCapture** lplpCapture);

	// IOleCommandTarget
	virtual HRESULT WINAPI QueryStatus (const GUID* lpguidCmdGroup, ULONG cCmds, OLECMD* lprgCmds, OLECMDTEXT* lpCmdText);
	virtual HRESULT WINAPI Exec (const GUID* lpguidCmdGroup, DWORD nCmdID, DWORD nCmdExecOpt, VARIANTARG* lpvaIn, VARIANTARG* lpvaOut);

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

	INT GetSquareSize (VOID);
	HRESULT SetSquareSize (INT cLength);

	VOID GetCellValues (FLOAT& fSetCell, FLOAT& fClearCell);
	VOID SetCellValues (FLOAT fSetCell, FLOAT fClearCell);

protected:
	HRESULT CmdSetSize (IBaseContainer* lpContainer);
	HRESULT CmdAddSplitters (IBaseContainer* lpContainer);
	HRESULT CmdProperties (IBaseContainer* lpContainer);

	VOID RecalculatePosition (VOID);
	BOOL CanAddSplitters (VOID);

	inline LONG Child (const VARIANT& v)
	{
		if(v.vt == VT_I4)
			return v.lVal;
		return -1;
	};
};