#include <windows.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "Library\Core\StringCore.h"
#include "Library\GraphCtrl.h"
#include "NeuralAPI.h"
#include "GdiList.h"
#include "BaseContainer.h"
#include "NeuralNet.h"
#include "AccessibleNetDoc.h"
#include "NeuralDocument.h"
#include "Library\Window\DialogHost.h"
#include "Splitter.h"
#include "InputPadSize.h"
#include "InputPadProps.h"
#include "InputPad.h"

CInputPad::CInputPad ()
{
	m_cRef = 1;

	m_x = 0;
	m_y = 0;

	m_cSideLength = 0;
	m_lpbPattern = NULL;
	m_lpInput = NULL;
	m_lpCells = NULL;

	m_fSetCell = 1.0f;
	m_fClearCell = 0.0f;

	m_fSelected = FALSE;

	m_lpParent = NULL;
}

CInputPad::~CInputPad ()
{
	INT cTotal = m_cSideLength * m_cSideLength;
	for(INT i = 0; i < cTotal; i++)
	{
		if(m_lpInput[i].lpTarget)
			m_lpInput[i].lpTarget->Release();
	}

	if(m_lpParent)
		m_lpParent->Release();

	__delete_array m_lpbPattern;
	__delete_array m_lpInput;
	__delete_array m_lpCells;
}

HRESULT WINAPI CInputPad::QueryInterface (REFIID iid, LPVOID* lplpvObject)
{
	HRESULT hr = E_INVALIDARG;
	if(lplpvObject)
	{
		if(iid == IID_IAccessible)
			*lplpvObject = (IAccessible*)(IDispatch*)this;
		else if(iid == IID_IDispatch)
			*lplpvObject = (IDispatch*)this;
		else if(__uuidof(INetDocObject) == iid)
			*lplpvObject = (INetDocObject*)this;
		else if(__uuidof(IOleCommandTarget) == iid)
			*lplpvObject = (IOleCommandTarget*)this;
		else if(iid == IID_IUnknown)
			*lplpvObject = (IUnknown*)(INetDocObject*)this;
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

ULONG WINAPI CInputPad::AddRef (VOID)
{
	return (ULONG)InterlockedIncrement((LONG*)&m_cRef);
}

ULONG WINAPI CInputPad::Release (VOID)
{
	ULONG c = (ULONG)InterlockedDecrement((LONG*)&m_cRef);
	if(c == 0)
		__delete this;
	return c;
}

// INetCycleProcessor

VOID CInputPad::SendPulses (VOID)
{
	INT c = m_cSideLength * m_cSideLength;
	BOOL fHasClearValue = 0.0f != m_fClearCell;
	for(INT n = 0; n < c; n++)
	{
		if(m_lpInput[n].lpTarget)
		{
			if(m_lpbPattern[n])
				m_lpInput[n].lpTarget->ReceiveValue(m_fSetCell,m_lpInput[n].iPin);
			else if(fHasClearValue)
				m_lpInput[n].lpTarget->ReceiveValue(m_fClearCell,m_lpInput[n].iPin);
		}
	}
}

VOID CInputPad::CheckThresholds (VOID)
{
}

// INetObject

HRESULT CInputPad::GetObjectClass (LPSTR lpszClass, INT cchMaxClass, __out INT* pcchClass)
{
	return TStrCchCpyLen(lpszClass, cchMaxClass, SLP("CInputPad"), pcchClass);
}

VOID CInputPad::GetPosition (INT& x, INT& y)
{
	x = m_x;
	y = m_y;
}

VOID CInputPad::ReceiveValue (FLOAT fValue, ULONG iPin)
{
	UNREFERENCED_PARAMETER(fValue);
	UNREFERENCED_PARAMETER(iPin);
}

ULONG CInputPad::GetInputPin (INT x, INT y)
{
	UNREFERENCED_PARAMETER(x);
	UNREFERENCED_PARAMETER(y);

	return 0xFFFFFFFF;
}

BOOL CInputPad::GetInputPinPosition (ULONG iPin, INT& x, INT& y)
{
	UNREFERENCED_PARAMETER(iPin);
	UNREFERENCED_PARAMETER(x);
	UNREFERENCED_PARAMETER(y);

	return FALSE;
}

HRESULT CInputPad::Load (INeuralFactory* pFactory, LPNLOADDATA lpLoadData, DWORD cLoadData, LPBYTE lpData, ULONG cbData)
{
	HRESULT hr = S_OK;

	INT cSideLength;
	LPVOID lpList[] = {&m_x, &m_y, &cSideLength, &m_fSelected};
	ULONG cbList[] = {sizeof(m_x), sizeof(m_y), sizeof(m_cSideLength), sizeof(m_fSelected)};
	INT c = sizeof(cbList) / sizeof(cbList[0]);

	for(INT i = 0; i < c; i++)
	{
		if(cbData < cbList[i])
		{
			hr = E_FAIL;
			break;
		}

		CopyMemory(lpList[i],lpData,cbList[i]);
		lpData += cbList[i];
		cbData -= cbList[i];
	}

	if(SUCCEEDED(hr))
		hr = SetSquareSize(cSideLength);

	if(SUCCEEDED(hr))
	{
		INT cSize = m_cSideLength * m_cSideLength;
		if(cbData < cSize * sizeof(BYTE))
			hr = E_FAIL;
		else
		{
			INT cConn;

			CopyMemory(m_lpbPattern,lpData,cSize * sizeof(BYTE));
			lpData += cSize * sizeof(BYTE);
			cbData -= cSize * sizeof(BYTE);

			if(cbData < sizeof(cConn))
				hr = E_FAIL;
			else
			{
				INT index;
				ULONG iPinSource, iPin;
				FLOAT fWeight;

				CopyMemory(&cConn,lpData,sizeof(cConn));
				lpData += sizeof(cConn);
				cbData -= sizeof(cConn);

				for(INT i = 0; i < cConn; i++)
				{
					if(cbData < sizeof(iPinSource) + sizeof(index) + sizeof(iPin) + sizeof(fWeight))
					{
						hr = E_FAIL;
						break;
					}

					CopyMemory(&iPinSource,lpData,sizeof(iPinSource));
					CopyMemory(&index,lpData + sizeof(iPinSource),sizeof(index));
					CopyMemory(&iPin,lpData + sizeof(iPinSource) + sizeof(index),sizeof(iPin));
					CopyMemory(&fWeight,lpData + sizeof(iPinSource) + sizeof(index) + sizeof(iPin),sizeof(fWeight));

					lpData += sizeof(iPinSource) + sizeof(index) + sizeof(iPin) + sizeof(fWeight);
					cbData -= sizeof(iPinSource) + sizeof(index) + sizeof(iPin) + sizeof(fWeight);

					if((DWORD)index >= cLoadData)
					{
						hr = E_FAIL;
						break;
					}

					hr = ConnectTo(iPinSource,lpLoadData[index].lpObject,iPin,fWeight);
					if(FAILED(hr))
						break;
				}
			}
		}
	}

	return hr;
}

HRESULT CInputPad::Save (INeuralNet* lpNet, ISequentialStream* lpStream)
{
	HRESULT hr = S_OK;
	ULONG cbWritten;

	INT cSize = m_cSideLength * m_cSideLength;
	LPVOID lpList[] = {&m_x, &m_y, &m_cSideLength, &m_fSelected, m_lpbPattern};
	ULONG cbList[] = {sizeof(m_x), sizeof(m_y), sizeof(m_cSideLength), sizeof(m_fSelected), cSize * sizeof(BYTE)};
	INT c = sizeof(cbList) / sizeof(cbList[0]);

	for(INT i = 0; i < c; i++)
	{
		hr = lpStream->Write(lpList[i],cbList[i],&cbWritten);
		if(FAILED(hr))
			break;
	}

	if(SUCCEEDED(hr))
	{
		INT i, cConn = 0;
		for(i = 0; i < cSize; i++)
		{
			if(m_lpInput[i].lpTarget)
				cConn++;
		}

		hr = lpStream->Write(&cConn,sizeof(cConn),&cbWritten);
		if(SUCCEEDED(hr) && cConn > 0)
		{
			INetDocObject* lpObject;
			INT index;
			ULONG iPinSource;

			for(i = 0; i < cSize; i++)
			{
				if(m_lpInput[i].lpTarget)
				{
					if(FAILED(m_lpInput[i].lpTarget->QueryInterface(&lpObject)))
					{
						hr = E_UNEXPECTED;
						break;
					}

					index = lpNet->GetObjectIndex(lpObject);
					lpObject->Release();
					if(index == -1)
					{
						hr = E_UNEXPECTED;
						break;
					}

					iPinSource = i + 1;
					hr = lpStream->Write(&iPinSource,sizeof(iPinSource),&cbWritten);
					if(FAILED(hr))
						break;

					hr = lpStream->Write(&index,sizeof(index),&cbWritten);
					if(FAILED(hr))
						break;

					hr = lpStream->Write(&m_lpInput[i].iPin,sizeof(ULONG),&cbWritten);
					if(FAILED(hr))
						break;

					hr = lpStream->Write(&m_lpInput[i].fWeight,sizeof(FLOAT),&cbWritten);
					if(FAILED(hr))
						break;
				}
			}
		}
	}

	return hr;
}

// INetDocObject

INT CInputPad::GetZOrder (VOID)
{
	return 2;
}

VOID CInputPad::DrawBackground (IGrapher* lpGraph)
{
	INT xPin, yPin, c = m_cSideLength * m_cSideLength;
	for(INT n = 0; n < c; n++)
	{
		if(m_lpInput[n].lpTarget)
		{
			lpGraph->MoveTo((FLOAT)m_lpCells[n].x,(FLOAT)m_lpCells[n].y,0.0f);
			m_lpInput[n].lpTarget->GetInputPinPosition(m_lpInput[n].iPin,xPin,yPin);
			lpGraph->LineTo((FLOAT)xPin,(FLOAT)yPin,0.0f);
		}
	}
}

VOID CInputPad::DrawForeground (IGrapher* lpGraph)
{
	LPBYTE lpBuffer;
	INT nWidth, nHeight;
	LONG lPitch;
	if(lpGraph->GetRawBuffer(lpBuffer,nWidth,nHeight,lPitch))
	{
		RECT rc;
		INT x, y, c = m_cSideLength * m_cSideLength;
		COLORREF crBackground;
		BYTE bAlpha;

		if(m_fSelected)
		{
			crBackground = RGB(255,200,200);
			bAlpha = 220;
		}
		else
		{
			crBackground = RGB(200,220,255);
			bAlpha = 128;
		}

		lpGraph->GraphToClientRect(&m_rcPos,&rc);
		if(DIBDrawing::AlphaFill24(lpBuffer,nWidth,nHeight,lPitch,&rc,crBackground,bAlpha))
		{
			for(INT n = 0; n < c; n++)
			{
				lpGraph->GraphToClient((FLOAT)(m_lpCells[n].x - 9),(FLOAT)(m_lpCells[n].y + 9),x,y);
				rc.left = x;
				rc.top = y;
				lpGraph->GraphToClient((FLOAT)(m_lpCells[n].x + 9),(FLOAT)(m_lpCells[n].y - 9),x,y);
				rc.right = x;
				rc.bottom = y;
				DIBDrawing::AlphaFill24(lpBuffer,nWidth,nHeight,lPitch,&rc,(m_lpbPattern[n]) ? RGB(64,64,64) : RGB(255,255,255),192);
			}
		}
	}
}

VOID CInputPad::SelectObject (BOOL fSelect)
{
	m_fSelected = fSelect;

	if(m_lpParent)
		m_lpParent->NotifyAccEvent((m_fSelected) ? EVENT_OBJECT_SELECTIONADD : EVENT_OBJECT_SELECTIONREMOVE,this);
}

VOID CInputPad::MoveObject (INT xDelta, INT yDelta)
{
	m_x += xDelta;
	m_y += yDelta;

	RecalculatePosition();
}

VOID CInputPad::ResizeObject (INT nHitTest, INT xResizePos, INT yResizePos, INT xDelta, INT yDelta)
{
	UNREFERENCED_PARAMETER(nHitTest);
	UNREFERENCED_PARAMETER(xResizePos);
	UNREFERENCED_PARAMETER(yResizePos);
	UNREFERENCED_PARAMETER(xDelta);
	UNREFERENCED_PARAMETER(yDelta);
}

INT CInputPad::HitTest (INT x, INT y)
{
	INT nHitTest = HITTEST_NONE;
	if(x >= m_rcPos.left && x <= m_rcPos.right)
	{
		if(y <= m_rcPos.top && y >= m_rcPos.bottom)
		{
			ULONG iPin = 0;

			nHitTest = HITTEST_SELECTABLE;

			if(GetDragSourcePin(x,y,iPin))
				nHitTest = HITTEST_DRAG_SOURCE_TOGGLE;

			nHitTest |= HITTEST_CONTEXT;
		}
	}
	return nHitTest;
}

BOOL CInputPad::GetDragSourcePin (INT x, INT y, ULONG& iPin)
{
	BOOL fSuccess = FALSE;
	INT c = m_cSideLength * m_cSideLength;
	for(INT n = 0; n < c; n++)
	{
		if(x >= m_lpCells[n].x - 9 && x <= m_lpCells[n].x + 9)
		{
			if(y <= m_lpCells[n].y + 9 && y >= m_lpCells[n].y - 9)
			{
				iPin = n + 1;
				fSuccess = TRUE;
				break;
			}
		}
	}
	return fSuccess;
}

BOOL CInputPad::GetDragSourcePoint (ULONG iPin, INT& x, INT& y)
{
	BOOL fSuccess = FALSE;
	if(iPin > 0 && iPin <= (ULONG)(m_cSideLength * m_cSideLength))
	{
		INT n = iPin - 1;
		x = m_lpCells[n].x;
		y = m_lpCells[n].y;
		fSuccess = TRUE;
	}
	return fSuccess;
}

BOOL CInputPad::HighlightPin (ULONG iPin, BOOL fHighlight)
{
	UNREFERENCED_PARAMETER(iPin);
	UNREFERENCED_PARAMETER(fHighlight);

	return FALSE;
}

BOOL CInputPad::HighlightConn (ULONG iPin, INT index)
{
	UNREFERENCED_PARAMETER(iPin);
	UNREFERENCED_PARAMETER(index);

	return FALSE;
}

VOID CInputPad::NotifyRemovalOf (INetDocObject* lpObject)
{
	INeurone* lpNeurone = NULL;
	if(SUCCEEDED(lpObject->QueryInterface(&lpNeurone)))
	{
		INT c = m_cSideLength * m_cSideLength;
		for(INT i = 0; i < c; i++)
		{
			if(m_lpInput[i].lpTarget == lpNeurone)
			{
				m_lpInput[i].lpTarget->Release();
				m_lpInput[i].lpTarget = NULL;
			}
		}
		lpNeurone->Release();
	}
}

HRESULT CInputPad::ConnectTo (ULONG iSourcePin, INetDocObject* lpTarget, ULONG iTargetPin, FLOAT fWeight)
{
	UNREFERENCED_PARAMETER(fWeight);

	HRESULT hr = E_FAIL;
	if(iSourcePin > 0 && iSourcePin <= (ULONG)(m_cSideLength * m_cSideLength))
	{
		INeurone* lpNeurone;
		hr = lpTarget->QueryInterface(&lpNeurone);
		if(SUCCEEDED(hr))
		{
			INT n = iSourcePin - 1;

			if(m_lpInput[n].lpTarget)
				m_lpInput[n].lpTarget->Release();

			m_lpInput[n].iPin = iTargetPin;
			m_lpInput[n].lpTarget = lpNeurone;
		}
	}
	return hr;
}

HRESULT CInputPad::ContextMenu (IBaseContainer* lpContainer, INT x, INT y)
{
	HRESULT hr;

	HMENU hMenu = LoadMenu(GetModuleHandle(NULL), MAKEINTRESOURCE(IDR_INPUT_PAD));
	if(hMenu)
	{
		HMENU hPopup = GetSubMenu(hMenu, 0);
		Assert(hPopup);
		hr = lpContainer->TrackPopupMenu(hPopup,x,y,this);
		DestroyMenu(hMenu);
	}
	else
		hr = HRESULT_FROM_WIN32(GetLastError());

	return hr;
}

BOOL CInputPad::Click (INT x, INT y)
{
	UNREFERENCED_PARAMETER(x);
	UNREFERENCED_PARAMETER(y);

	ULONG iPin;
	if(GetDragSourcePin(x,y,iPin))
	{
		INT n = iPin - 1;
		m_lpbPattern[n] = !m_lpbPattern[n];
	}

	return FALSE;
}

BOOL CInputPad::PressChar (CHAR ch)
{
	UNREFERENCED_PARAMETER(ch);

	return FALSE;
}

LONG CInputPad::GetAccState (VOID)
{
	LONG lAccState = STATE_SYSTEM_MULTISELECTABLE;
	if(m_fSelected)
		lAccState |= STATE_SYSTEM_SELECTED;
	return lAccState;
}

HRESULT CInputPad::GetAccObject (IAccessibleNetDoc* lpParent, IAccessible** lplpAccessible)
{
	if(m_lpParent)
		m_lpParent->Release();

	m_lpParent = lpParent;
	m_lpParent->AddRef();

	*lplpAccessible = this;
	(*lplpAccessible)->AddRef();

	return S_OK;
}

HRESULT CInputPad::UnloadAccessibility (VOID)
{
	HRESULT hr;

	if(m_lpParent)
	{
		m_lpParent->Release();
		m_lpParent = NULL;
		hr = S_OK;
	}
	else
		hr = S_FALSE;

	return hr;
}

HRESULT CInputPad::GetGraphInputCaptureObject (INeuralDocument* lpDocument, IGraphInputCapture** lplpCapture)
{
	UNREFERENCED_PARAMETER(lpDocument);
	UNREFERENCED_PARAMETER(lplpCapture);

	return E_NOTIMPL;
}

// IOleCommandTarget

HRESULT WINAPI CInputPad::QueryStatus (const GUID* lpguidCmdGroup, ULONG cCmds, OLECMD* lprgCmds, OLECMDTEXT* lpCmdText)
{
	UNREFERENCED_PARAMETER(lpguidCmdGroup);
	UNREFERENCED_PARAMETER(lpCmdText);

	for(ULONG i = 0; i < cCmds; i++)
	{
		switch(lprgCmds[i].cmdID)
		{
		case ID_INPUTPAD_SETSIZE:
		case ID_INPUTPAD_PROPERTIES:
		case ID_NEURONE_PROPERTIES:
			lprgCmds[i].cmdf = OLECMDF_SUPPORTED | OLECMDF_ENABLED;
			break;
		case ID_INPUTPAD_ADDSPLITTERS:
			if(CanAddSplitters())
				lprgCmds[i].cmdf = OLECMDF_SUPPORTED | OLECMDF_ENABLED;
			else
				lprgCmds[i].cmdf = OLECMDF_SUPPORTED;
			break;
		}
	}

	return S_OK;
}

HRESULT WINAPI CInputPad::Exec (const GUID* lpguidCmdGroup, DWORD nCmdID, DWORD nCmdExecOpt, VARIANTARG* lpvaIn, VARIANTARG* lpvaOut)
{
	UNREFERENCED_PARAMETER(lpguidCmdGroup);
	UNREFERENCED_PARAMETER(nCmdExecOpt);
	UNREFERENCED_PARAMETER(lpvaOut);

	HRESULT hr = OLECMDERR_E_NOTSUPPORTED;
	IBaseContainer* lpContainer = NULL;

	if(lpvaIn && VT_UNKNOWN == lpvaIn->vt && lpvaIn->punkVal)
		lpvaIn->punkVal->QueryInterface(IID_IBaseContainer,(LPVOID*)&lpContainer);

	switch(nCmdID)
	{
	case ID_INPUTPAD_SETSIZE:
		hr = CmdSetSize(lpContainer);
		break;
	case ID_INPUTPAD_ADDSPLITTERS:
		hr = CmdAddSplitters(lpContainer);
		break;
	case ID_INPUTPAD_PROPERTIES:
	case ID_NEURONE_PROPERTIES:
		hr = CmdProperties(lpContainer);
		break;
	}

	if(lpContainer)
		lpContainer->Release();

	return hr;
}

// IDispatch

HRESULT WINAPI CInputPad::GetTypeInfoCount (UINT* pctinfo)
{
	UNREFERENCED_PARAMETER(pctinfo);

	return S_FALSE;
}

HRESULT WINAPI CInputPad::GetTypeInfo (UINT itinfo, LCID lcid, ITypeInfo** pptinfo)
{
	UNREFERENCED_PARAMETER(itinfo);
	UNREFERENCED_PARAMETER(lcid);
	UNREFERENCED_PARAMETER(pptinfo);

	return S_FALSE;
}

HRESULT WINAPI CInputPad::GetIDsOfNames (REFIID iid, OLECHAR** rgszNames, UINT cNames, LCID lcid, DISPID* rgdispid)
{
	UNREFERENCED_PARAMETER(iid);
	UNREFERENCED_PARAMETER(rgszNames);
	UNREFERENCED_PARAMETER(cNames);
	UNREFERENCED_PARAMETER(lcid);
	UNREFERENCED_PARAMETER(rgdispid);

	return S_FALSE;
}

HRESULT WINAPI CInputPad::Invoke (DISPID dispidMember, REFIID iid, LCID lcid, WORD wFlags, DISPPARAMS* pdispParams, VARIANT* pvarResult, EXCEPINFO* pexcepInfo, UINT* puArgErr)
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

HRESULT WINAPI CInputPad::get_accParent (IDispatch** ppdispParent)
{
	*ppdispParent = m_lpParent;
	(*ppdispParent)->AddRef();

	return S_OK;
}

HRESULT WINAPI CInputPad::get_accChildCount (LONG* pChildCount)
{
	*pChildCount = m_cSideLength * m_cSideLength;
	return S_OK;
}

HRESULT WINAPI CInputPad::get_accChild (VARIANT varChild, IDispatch** ppdispChild)
{
	UNREFERENCED_PARAMETER(varChild);
	UNREFERENCED_PARAMETER(ppdispChild);

	return S_FALSE;
}

HRESULT WINAPI CInputPad::get_accName (VARIANT varChild, BSTR* pszName)
{
	HRESULT hr = S_FALSE;
	if(Child(varChild) == CHILDID_SELF)
		*pszName = SysAllocString(L"Input pad");
	else
	{
		WCHAR wzName[32];
		wsprintfW(wzName,L"Cell %d",varChild.llVal);
		*pszName = SysAllocString(wzName);
	}
	if(*pszName)
		hr = S_OK;
	else
		hr = E_OUTOFMEMORY;
	return hr;
}

HRESULT WINAPI CInputPad::get_accValue (VARIANT varChild, BSTR* pszValue)
{
	UNREFERENCED_PARAMETER(varChild);
	UNREFERENCED_PARAMETER(pszValue);

	return S_FALSE;
}

HRESULT WINAPI CInputPad::get_accDescription (VARIANT varChild, BSTR* pszDescription)
{
	UNREFERENCED_PARAMETER(varChild);
	UNREFERENCED_PARAMETER(pszDescription);

	return S_FALSE;
}

HRESULT WINAPI CInputPad::get_accRole (VARIANT varChild, VARIANT* pvarRole)
{
	pvarRole->vt = VT_I4;
	if(Child(varChild) == CHILDID_SELF)
		pvarRole->lVal = ROLE_SYSTEM_GRAPHIC;
	else
		pvarRole->lVal = ROLE_SYSTEM_CELL;
	return S_OK;
}

HRESULT WINAPI CInputPad::get_accState (VARIANT varChild, VARIANT* pvarState)
{
	pvarState->vt = VT_I4;
	if(Child(varChild) == CHILDID_SELF)
		pvarState->lVal = STATE_SYSTEM_MULTISELECTABLE | ((m_fSelected) ? STATE_SYSTEM_SELECTED : 0);
	else
		pvarState->lVal = (m_lpbPattern[varChild.lVal - 1]) ? STATE_SYSTEM_CHECKED : 0;
	return S_OK;
}

HRESULT WINAPI CInputPad::get_accHelp (VARIANT varChild, BSTR* pszHelp)
{
	UNREFERENCED_PARAMETER(varChild);
	UNREFERENCED_PARAMETER(pszHelp);

	return S_FALSE;
}

HRESULT WINAPI CInputPad::get_accHelpTopic (BSTR* pszHelpFile, VARIANT varChild, LONG* pidTopic)
{
	UNREFERENCED_PARAMETER(pszHelpFile);
	UNREFERENCED_PARAMETER(varChild);
	UNREFERENCED_PARAMETER(pidTopic);

	return S_FALSE;
}

HRESULT WINAPI CInputPad::get_accKeyboardShortcut (VARIANT varChild, BSTR* pszKeyboardShortcut)
{
	UNREFERENCED_PARAMETER(varChild);
	UNREFERENCED_PARAMETER(pszKeyboardShortcut);

	return S_FALSE;
}

HRESULT WINAPI CInputPad::get_accFocus (VARIANT* pvarFocusChild)
{
	pvarFocusChild->vt = VT_I4;
	pvarFocusChild->lVal = CHILDID_SELF;
	return S_OK;
}

HRESULT WINAPI CInputPad::get_accSelection (VARIANT* pvarSelectedChildren)
{
	UNREFERENCED_PARAMETER(pvarSelectedChildren);

	return S_FALSE;
}

HRESULT WINAPI CInputPad::get_accDefaultAction (VARIANT varChild, BSTR* pszDefaultAction)
{
	UNREFERENCED_PARAMETER(varChild);
	UNREFERENCED_PARAMETER(pszDefaultAction);

	return S_FALSE;
}

HRESULT WINAPI CInputPad::accSelect (LONG flagsSelect, VARIANT varChild)
{
	UNREFERENCED_PARAMETER(flagsSelect);
	UNREFERENCED_PARAMETER(varChild);

	return S_FALSE;
}

HRESULT WINAPI CInputPad::accLocation (LONG* pxLeft, LONG* pyTop, LONG* pcxWidth, LONG* pcyHeight, VARIANT varChild)
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
			m_lpParent->GraphToClient((FLOAT)m_rcPos.left,(FLOAT)m_rcPos.top,pxLeft,pyTop);
			*pcxWidth = (LONG)((m_rcPos.right - m_rcPos.left) * fScale);
			*pcyHeight = (LONG)((m_rcPos.top - m_rcPos.bottom) * fScale);
		}
		else
		{
			INT xCell, yCell;
			if(GetDragSourcePoint(varChild.lVal,xCell,yCell))	// The pins start with 1
			{
				xCell -= 9;
				yCell += 9;
				m_lpParent->GraphToClient((FLOAT)xCell,(FLOAT)yCell,pxLeft,pyTop);
				*pcxWidth = (LONG)(18.0f * fScale);
				*pcyHeight = (LONG)(18.0f * fScale);
			}
			else
				hr = E_FAIL;
		}

		*pxLeft += x;
		*pyTop += y;
	}
	return hr;
}

HRESULT WINAPI CInputPad::accNavigate (LONG navDir, VARIANT varStart, VARIANT* pvarEndUpAt)
{
	UNREFERENCED_PARAMETER(navDir);
	UNREFERENCED_PARAMETER(varStart);
	UNREFERENCED_PARAMETER(pvarEndUpAt);

	return S_FALSE;
}

HRESULT WINAPI CInputPad::accHitTest (LONG xLeft, LONG yTop, VARIANT* pvarChildAtPoint)
{
	HRESULT hr = S_FALSE;
	POINT ptScreen = {xLeft, yTop};
	FLOAT fx, fy;
	if(SUCCEEDED(m_lpParent->ScreenToGraph(ptScreen,fx,fy)))
	{
		ULONG iPin;

		pvarChildAtPoint->vt = VT_I4;
		pvarChildAtPoint->lVal = CHILDID_SELF;

		if(GetDragSourcePin((INT)fx,(INT)fy,iPin))
			pvarChildAtPoint->lVal = iPin;			// The pins start with 1

		hr = S_OK;
	}
	return hr;
}

HRESULT WINAPI CInputPad::accDoDefaultAction (VARIANT varChild)
{
	UNREFERENCED_PARAMETER(varChild);

	return S_FALSE;
}

HRESULT WINAPI CInputPad::put_accName (VARIANT varChild, BSTR bstrName)
{
	UNREFERENCED_PARAMETER(varChild);
	UNREFERENCED_PARAMETER(bstrName);

	return S_FALSE;
}

HRESULT WINAPI CInputPad::put_accValue (VARIANT varChild, BSTR bstrValue)
{
	UNREFERENCED_PARAMETER(varChild);
	UNREFERENCED_PARAMETER(bstrValue);

	return S_FALSE;
}

//

INT CInputPad::GetSquareSize (VOID)
{
	return m_cSideLength;
}

HRESULT CInputPad::SetSquareSize (INT cLength)
{
	HRESULT hr = E_INVALIDARG;
	if(cLength >= 3 && cLength <= 16)
	{
		INT cSize = cLength * cLength;
		LPBYTE lpbPattern = __new BYTE[cSize];
		LPCLIST lpInput = __new CLIST[cSize];
		POINT* lpCells = __new POINT[cSize];
		if(lpbPattern && lpInput && lpCells)
		{
			ZeroMemory(lpbPattern,sizeof(BYTE) * cSize);
			ZeroMemory(lpInput,sizeof(CLIST) * cSize);

			if(m_lpInput)
			{
				INT cPrevSize = m_cSideLength * m_cSideLength;
				INT cMinSize = min(cSize,cPrevSize);
				CopyMemory(lpInput,m_lpInput,sizeof(CLIST) * cMinSize);
				if(cPrevSize > cMinSize)
				{
					for(INT i = cMinSize; i < cPrevSize; i++)
					{
						if(m_lpInput[i].lpTarget)
							m_lpInput[i].lpTarget->Release();
					}
				}
			}

			__delete_array m_lpbPattern;
			m_lpbPattern = lpbPattern;

			__delete_array m_lpInput;
			m_lpInput = lpInput;

			__delete_array m_lpCells;
			m_lpCells = lpCells;

			m_cSideLength = cLength;
			RecalculatePosition();

			hr = S_OK;
		}
		else
		{
			__delete_array lpbPattern;
			__delete_array lpInput;
			__delete_array lpCells;
			hr = E_OUTOFMEMORY;
		}
	}
	return hr;
}

VOID CInputPad::GetCellValues (FLOAT& fSetCell, FLOAT& fClearCell)
{
	fSetCell = m_fSetCell;
	fClearCell = m_fClearCell;
}

VOID CInputPad::SetCellValues (FLOAT fSetCell, FLOAT fClearCell)
{
	m_fSetCell = fSetCell;
	m_fClearCell = fClearCell;
}

HRESULT CInputPad::CmdSetSize (IBaseContainer* lpContainer)
{
	HRESULT hr;
	HWND hwnd;

	if(lpContainer)
	{
		HINSTANCE hInstance;

		hr = lpContainer->GetHwnd(&hwnd);
		hInstance = (HINSTANCE)GetWindowLong(hwnd,GWL_HINSTANCE);

		if(hwnd && SUCCEEDED(hr))
		{
			CInputPadSize Size(this);
			CDialogHost Dialog(hInstance);

			hr = Dialog.Display(hwnd,&Size);
			if(SUCCEEDED(hr) && Dialog.GetReturnValue())
			{
				INeuralDocument* lpDocument;
				if(SUCCEEDED(lpContainer->QueryInterface(&lpDocument)))
				{
					lpDocument->SetChangesMade();
					lpDocument->Release();
				}
			}
		}
	}
	else
		hr = E_INVALIDARG;

	return hr;
}

HRESULT CInputPad::CmdAddSplitters (IBaseContainer* lpContainer)
{
	HRESULT hr;

	if(lpContainer)
	{
		INeuralDocument* lpDocument;

		hr = lpContainer->QueryInterface(&lpDocument);
		if(SUCCEEDED(hr))
		{
			CSplitter* lpSplitter;
			FLOAT yOffset;

			for(INT x = 0; x < m_cSideLength; x++)
			{
				for(INT y = 0; y < m_cSideLength; y++)
				{
					lpSplitter = __new CSplitter;
					if(NULL == lpSplitter)
					{
						hr = E_OUTOFMEMORY;
						goto Cleanup;
					}
					yOffset = ((FLOAT)y - (FLOAT)m_cSideLength / 2.0f) * 20.0f;
					if(x & 1)
						yOffset += 10.0f;
					lpSplitter->MoveObject(m_rcPos.right + 20 + x * 20,(m_y - (INT)yOffset) - 10);
					hr = lpDocument->InsertObject(lpSplitter);
					if(SUCCEEDED(hr))
					{
						hr = ConnectTo((x + y * m_cSideLength) + 1,lpSplitter,0,1.0f);
						if(SUCCEEDED(hr))
							lpDocument->RemoveSelection(lpSplitter);
					}
					lpSplitter->Release();
					if(FAILED(hr))
						goto Cleanup;
				}
			}

			lpDocument->Release();
		}
	}
	else
		hr = E_INVALIDARG;

Cleanup:
	return hr;
}

HRESULT CInputPad::CmdProperties (IBaseContainer* lpContainer)
{
	HRESULT hr;

	if(lpContainer)
	{
		HWND hwnd;
		HINSTANCE hInstance;

		hr = lpContainer->GetHwnd(&hwnd);
		hInstance = (HINSTANCE)GetWindowLong(hwnd,GWL_HINSTANCE);

		if(hwnd && SUCCEEDED(hr))
		{
			CInputPadProps Props(this);
			CDialogHost Dialog(hInstance);

			hr = Dialog.Display(hwnd,&Props);
			if(SUCCEEDED(hr) && Dialog.GetReturnValue())
			{
				INeuralDocument* lpDocument;
				if(SUCCEEDED(lpContainer->QueryInterface(&lpDocument)))
				{
					lpDocument->SetChangesMade();
					lpDocument->Release();
				}
			}
		}
	}
	else
		hr = E_INVALIDARG;

	return hr;
}

VOID CInputPad::RecalculatePosition (VOID)
{
	INT nSize = m_cSideLength * 20 + 4;
	INT n = 0, xPos, yPos;

	m_rcPos.left = m_x - nSize / 2;
	m_rcPos.right = m_rcPos.left + nSize;
	m_rcPos.top = m_y + nSize / 2;
	m_rcPos.bottom = m_rcPos.top - nSize;

	yPos = m_rcPos.top - 2;
	for(INT y = 0; y < m_cSideLength; y++)
	{
		xPos = m_rcPos.left + 2;
		for(INT x = 0; x < m_cSideLength; x++)
		{
			m_lpCells[n].x = xPos + 10;
			m_lpCells[n].y = yPos - 10;
			xPos += 20;
			n++;
		}
		yPos -= 20;
	}

	if(m_lpParent)
		m_lpParent->NotifyAccEvent(EVENT_OBJECT_LOCATIONCHANGE,this);
}

BOOL CInputPad::CanAddSplitters (VOID)
{
	INT cTotal = m_cSideLength * m_cSideLength;
	for(INT i = 0; i < cTotal; i++)
	{
		if(m_lpInput[i].lpTarget)
			return FALSE;
	}
	return TRUE;
}