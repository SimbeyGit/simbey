#include <math.h>
#include <windows.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "Library\Core\StringCore.h"
#include "Library\GraphCtrl.h"
#include "NeuralAPI.h"
#include "GdiList.h"
#include "BaseContainer.h"
#include "NeuralNet.h"
#include "AccessibleNeurone.h"
#include "Neurone.h"

CNeurone::CNeurone ()
{
	m_cRef = 1;

	m_x = 0;
	m_y = 0;

	m_dwState = NSTATE_NORMAL;
	m_iRadius = DEFAULT_RADIUS;
	m_iAxonPosition = 0;

	m_fThreshold = 0.5f;
	m_fAccumulator = 0.0f;
	m_fFinalValue = 0.0f;

	m_lpAccessible = NULL;
}

CNeurone::~CNeurone ()
{
	Assert(NULL == m_lpAccessible);
}

// IUnknown

HRESULT WINAPI CNeurone::QueryInterface (REFIID iid, LPVOID* lplpvObject)
{
	HRESULT hr = E_POINTER;

	if(lplpvObject)
	{
		if(iid == __uuidof(INeurone))
			*lplpvObject = (INeurone*)this;
		else if(iid == __uuidof(INetDocObject))
			*lplpvObject = (INetDocObject*)this;
		else if(iid == IID_IOleCommandTarget)
			*lplpvObject = (IOleCommandTarget*)this;
		else if(iid == IID_IUnknown)
			*lplpvObject = (IUnknown*)(INeurone*)this;
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

ULONG WINAPI CNeurone::AddRef (VOID)
{
	return (ULONG)InterlockedIncrement((LONG*)&m_cRef);
}

ULONG WINAPI CNeurone::Release (VOID)
{
	ULONG c = (ULONG)InterlockedDecrement((LONG*)&m_cRef);
	if(c == 0)
		__delete this;
	return c;
}

// INetCycleProcessor

VOID CNeurone::SendPulses (VOID)
{
	if(m_dwState & NSTATE_EXCITED)
	{
		m_dwState &= ~NSTATE_EXCITED;

		m_Connections.SendPulses();
	}
	m_fFinalValue = 0.0f;
}

VOID CNeurone::CheckThresholds (VOID)
{
	if(m_fAccumulator >= m_fThreshold)
		m_dwState |= NSTATE_EXCITED;
	m_fFinalValue = m_fAccumulator;
	m_fAccumulator = 0.0f;
}

// INetObject

HRESULT CNeurone::GetObjectClass (LPSTR lpszClass, INT cchMaxClass, __out INT* pcchClass)
{
	return TStrCchCpyLen(lpszClass, cchMaxClass, SLP("CNeurone"), pcchClass);
}

VOID CNeurone::GetPosition (INT& x, INT& y)
{
	x = m_x;
	y = m_y;
}

VOID CNeurone::ReceiveValue (FLOAT fValue, ULONG iPin)
{
	UNREFERENCED_PARAMETER(iPin);

	m_fAccumulator += fValue;
}

ULONG CNeurone::GetInputPin (INT x, INT y)
{
	UNREFERENCED_PARAMETER(x);
	UNREFERENCED_PARAMETER(y);

	return 0;
}

BOOL CNeurone::GetInputPinPosition (ULONG iPin, INT& x, INT& y)
{
	BOOL fSuccess = FALSE;
	if(iPin == 0)
	{
		x = m_x;
		y = m_y;
		fSuccess = TRUE;
	}
	return fSuccess;
}

HRESULT CNeurone::Load (INeuralFactory* pFactory, LPNLOADDATA lpLoadData, DWORD cLoadData, LPBYTE lpData, ULONG cbData)
{
	UNREFERENCED_PARAMETER(pFactory);

	HRESULT hr = S_OK;

	LPVOID lpList[] = {&m_x, &m_y, &m_dwState, &m_iRadius, &m_iAxonPosition, &m_fThreshold, &m_fFinalValue};
	ULONG cbList[] = {sizeof(m_x), sizeof(m_y), sizeof(m_dwState), sizeof(m_iRadius), sizeof(m_iAxonPosition), sizeof(m_fThreshold), sizeof(m_fFinalValue)};
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
		hr = CConnList::LoadConnections(this,0,lpLoadData,cLoadData,&lpData,&cbData);

	return hr;
}

HRESULT CNeurone::Save (INeuralNet* lpNet, ISequentialStream* lpStream)
{
	HRESULT hr = S_OK;
	ULONG cbWritten;

	LPVOID lpList[] = {&m_x, &m_y, &m_dwState, &m_iRadius, &m_iAxonPosition, &m_fThreshold, &m_fFinalValue};
	ULONG cbList[] = {sizeof(m_x), sizeof(m_y), sizeof(m_dwState), sizeof(m_iRadius), sizeof(m_iAxonPosition), sizeof(m_fThreshold), sizeof(m_fFinalValue)};

	// Don't save selection!
	DWORD dwRestoreState = m_dwState;
	m_dwState &= ~NSTATE_SELECTED;

	for(INT i = 0; i < ARRAYSIZE(cbList); i++)
	{
		hr = lpStream->Write(lpList[i],cbList[i],&cbWritten);
		if(FAILED(hr))
			break;
	}

	if(SUCCEEDED(hr))
		hr = m_Connections.SaveConnections(lpNet,lpStream);

	m_dwState = dwRestoreState;

	return hr;
}

// INeurone

VOID CNeurone::ResetNeurone (VOID)
{
	m_fAccumulator = 0.0f;
	m_fFinalValue = 0.0f;
	m_dwState &= ~NSTATE_EXCITED;
}

INT CNeurone::CountConnections (ULONG iSourcePin)
{
	UNREFERENCED_PARAMETER(iSourcePin);

	Assert(iSourcePin == 0);

	return m_Connections.Count();
}

VOID CNeurone::EnumConnections (ULONG iSourcePin, ENUMCONNLIST lpfnCallback, LPVOID lpParam)
{
	if(iSourcePin == 0)
		m_Connections.EnumConnections(this, lpfnCallback, lpParam);
}

BOOL CNeurone::ClearConnection (ULONG iSourcePin, INT index)
{
	BOOL fSuccess = FALSE;
	if(iSourcePin == 0)
		fSuccess = m_Connections.ClearConnection(index);
	return fSuccess;
}

BOOL CNeurone::SetConnectionWeight (ULONG iSourcePin, INT index, FLOAT fWeight)
{
	BOOL fSuccess = FALSE;
	if(iSourcePin == 0)
		fSuccess = m_Connections.SetConnectionWeight(index, fWeight);
	return fSuccess;
}

BOOL CNeurone::GetCurrentValue (FLOAT& fValue)
{
	fValue = m_fFinalValue;
	return TRUE;
}

VOID CNeurone::SetThreshold (FLOAT fThreshold)
{
	m_fThreshold = fThreshold;
}

FLOAT CNeurone::GetThreshold (VOID)
{
	return m_fThreshold;
}

VOID CNeurone::RunTrainer (DWORD dwTrainer, FLOAT fInput, ULONG iInputPin, FLOAT fOutput, ULONG iOutputPin)
{
	if(0 == iInputPin)
		m_Connections.RunTrainer(dwTrainer, fInput, fOutput, iOutputPin);
}

// INetDocObject

INT CNeurone::GetZOrder (VOID)
{
	return 1;
}

VOID CNeurone::DrawBackground (IGrapher* lpGraph)
{
	INT xAxon, yAxon;
	if(GetAxonPosition(xAxon,yAxon))
		m_Connections.Draw(lpGraph, xAxon, yAxon, NSTATE_EXCITED == (m_dwState & NSTATE_EXCITED));
}

VOID CNeurone::DrawForeground (IGrapher* lpGraph)
{
	INT iRadius = m_iRadius;
	INT xAxon, yAxon;

	if(m_dwState & NSTATE_SELECTED)
		lpGraph->SelectPen(GdiList::hpnSelected);
	else
		lpGraph->SelectPen(GdiList::hpnBorder);

	if(m_dwState & NSTATE_HIGHLIGHT)
		lpGraph->SelectBrush(GdiList::hbrTarget);
	else if(m_dwState & NSTATE_EXCITED)
		lpGraph->SelectBrush(GdiList::hbrExcited);
	else
		lpGraph->SelectBrush(GdiList::hbrRest);

	if(GetAxonPosition(xAxon,yAxon))
		lpGraph->Ellipse((FLOAT)(xAxon - AXON_RADIUS),(FLOAT)(yAxon - AXON_RADIUS),0.0f,(FLOAT)(xAxon + AXON_RADIUS),(FLOAT)(yAxon + AXON_RADIUS),0.0f);

	lpGraph->Ellipse((FLOAT)(m_x - iRadius),(FLOAT)(m_y - iRadius),0.0f,(FLOAT)(m_x + iRadius),(FLOAT)(m_y + iRadius),0.0f);
}

VOID CNeurone::SelectObject (BOOL fSelect)
{
	if (fSelect)
		m_dwState |= NSTATE_SELECTED;
	else
		m_dwState &= ~NSTATE_SELECTED;

	if(m_lpAccessible)
		m_lpAccessible->Select(fSelect);
}

VOID CNeurone::MoveObject (INT xDelta, INT yDelta)
{
	m_x += xDelta;
	m_y += yDelta;

	if(m_lpAccessible)
		m_lpAccessible->MoveTo(m_x,m_y);
}

VOID CNeurone::ResizeObject (INT nHitTest, INT xResizePos, INT yResizePos, INT xDelta, INT yDelta)
{
	UNREFERENCED_PARAMETER(nHitTest);
	UNREFERENCED_PARAMETER(xResizePos);
	UNREFERENCED_PARAMETER(yResizePos);
	UNREFERENCED_PARAMETER(xDelta);
	UNREFERENCED_PARAMETER(yDelta);
}

INT CNeurone::HitTest (INT x, INT y)
{
	INT nHitTest = HITTEST_NONE;
	FLOAT xDiff = (FLOAT)(m_x - x);
	FLOAT yDiff = (FLOAT)(m_y - y);
	FLOAT fDistance = sqrtf(xDiff * xDiff + yDiff * yDiff);
	if(fDistance <= (FLOAT)m_iRadius)
		nHitTest = HITTEST_DRAG_TARGET | HITTEST_SELECTABLE | HITTEST_CONTEXT;
	else
	{
		INT xAxon, yAxon;
		if(GetAxonPosition(xAxon,yAxon))
		{
			xDiff = (FLOAT)(xAxon - x);
			yDiff = (FLOAT)(yAxon - y);
			fDistance = (FLOAT)sqrtf(xDiff * xDiff + yDiff * yDiff);
			if(fDistance <= (FLOAT)AXON_RADIUS)
				nHitTest = HITTEST_DRAG_SOURCE | HITTEST_SELECTABLE;
		}
	}
	return nHitTest;
}

BOOL CNeurone::GetDragSourcePin (INT x, INT y, ULONG& iPin)
{
	BOOL fSuccess = FALSE;
	INT xAxon, yAxon;
	if(GetAxonPosition(xAxon,yAxon))
	{
		FLOAT xDiff = (FLOAT)(xAxon - x);
		FLOAT yDiff = (FLOAT)(yAxon - y);
		FLOAT fDistance = (FLOAT)sqrtf(xDiff * xDiff + yDiff * yDiff);
		if(fDistance <= (FLOAT)AXON_RADIUS)
		{
			iPin = 0;
			fSuccess = TRUE;
		}
	}
	return fSuccess;
}

BOOL CNeurone::GetDragSourcePoint (ULONG iPin, INT& x, INT& y)
{
	BOOL fSuccess = FALSE;
	if(iPin == 0)
		fSuccess = GetAxonPosition(x,y);
	return fSuccess;
}

BOOL CNeurone::HighlightPin (ULONG iPin, BOOL fHighlight)
{
	BOOL fSuccess = FALSE;
	if(iPin == 0)
	{
		if(fHighlight)
			m_dwState |= NSTATE_HIGHLIGHT;
		else
			m_dwState &= ~NSTATE_HIGHLIGHT;
		fSuccess = TRUE;
	}
	return fSuccess;
}

BOOL CNeurone::HighlightConn (ULONG iPin, INT index)
{
	return (-1 == index || 0 == iPin) ? m_Connections.Highlight(index) : FALSE;
}

VOID CNeurone::NotifyRemovalOf (INetDocObject* lpObject)
{
	INeurone* lpNeurone = NULL;
	if(SUCCEEDED(lpObject->QueryInterface(&lpNeurone)))
	{
		m_Connections.RemoveConnectionsTo(lpNeurone);
		lpNeurone->Release();
	}
}

// IOleCommandTarget

HRESULT WINAPI CNeurone::QueryStatus (const GUID* lpguidCmdGroup, ULONG cCmds, OLECMD* lprgCmds, OLECMDTEXT* lpCmdText)
{
	UNREFERENCED_PARAMETER(lpguidCmdGroup);
	UNREFERENCED_PARAMETER(lpCmdText);

	for(ULONG i = 0; i < cCmds; i++)
	{
		switch(lprgCmds[i].cmdID)
		{
		case ID_NEURONE_FIRE:
			lprgCmds[i].cmdf = OLECMDF_SUPPORTED | OLECMDF_ENABLED;
			break;
		case ID_NEURONE_ROTATE_AXON:
			{
				INT x, y;
				if(GetAxonPosition(x, y))
					lprgCmds[i].cmdf = OLECMDF_SUPPORTED | OLECMDF_ENABLED;
				else
					lprgCmds[i].cmdf = OLECMDF_SUPPORTED;
			}
			break;
		}
	}

	return S_OK;
}

HRESULT WINAPI CNeurone::Exec (const GUID* lpguidCmdGroup, DWORD nCmdID, DWORD nCmdExecOpt, VARIANTARG* lpvaIn, VARIANTARG* lpvaOut)
{
	UNREFERENCED_PARAMETER(lpguidCmdGroup);
	UNREFERENCED_PARAMETER(nCmdExecOpt);
	UNREFERENCED_PARAMETER(lpvaIn);
	UNREFERENCED_PARAMETER(lpvaOut);

	HRESULT hr = OLECMDERR_E_NOTSUPPORTED;

	switch(nCmdID)
	{
	case ID_NEURONE_FIRE:
		hr = CmdFire();
		break;
	case ID_NEURONE_ROTATE_AXON:
		if(8 == ++m_iAxonPosition)
			m_iAxonPosition = 0;
		hr = S_OK;
		break;
	}

	return hr;
}

HRESULT CNeurone::ConnectTo (ULONG iSourcePin, INetDocObject* lpTarget, ULONG iTargetPin, FLOAT fWeight)
{
	HRESULT hr = E_FAIL;
	if(iSourcePin == 0)
		hr = m_Connections.ConnectTo(lpTarget,iTargetPin,fWeight);
	return hr;
}

HRESULT CNeurone::ContextMenu (IBaseContainer* lpContainer, INT x, INT y)
{
	HRESULT hr;

	HMENU hMenu = LoadMenu(GetModuleHandle(NULL), MAKEINTRESOURCE(IDR_NEURONE));
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

BOOL CNeurone::Click (INT x, INT y)
{
	UNREFERENCED_PARAMETER(x);
	UNREFERENCED_PARAMETER(y);

	return FALSE;
}

BOOL CNeurone::PressChar (CHAR ch)
{
	UNREFERENCED_PARAMETER(ch);

	return FALSE;
}

LONG CNeurone::GetAccState (VOID)
{
	LONG lAccState = STATE_SYSTEM_MULTISELECTABLE;
	if(m_dwState & NSTATE_SELECTED)
		lAccState |= STATE_SYSTEM_SELECTED;
	return lAccState;
}

HRESULT CNeurone::GetAccObject (IAccessibleNetDoc* lpParent, IAccessible** lplpAccessible)
{
	HRESULT hr = E_OUTOFMEMORY;

	if(m_lpAccessible == NULL)
	{
		m_lpAccessible = __new CAccessibleNeurone(lpParent);
		if(m_lpAccessible)
		{
			hr = m_lpAccessible->Initialize(this,m_iRadius << 1,m_iRadius << 1);
			if(SUCCEEDED(hr))
			{
				INT xAxon, yAxon;
				if(GetAxonPosition(xAxon,yAxon))
				{
					POINT pt = {xAxon - m_x, yAxon - m_y};
					hr = m_lpAccessible->InitConnectors(&pt,1);
				}
			}
			if(FAILED(hr))
			{
				m_lpAccessible->Release();
				m_lpAccessible = NULL;
			}
		}
	}

	if(m_lpAccessible)
	{
		*lplpAccessible = m_lpAccessible;
		(*lplpAccessible)->AddRef();
		hr = S_OK;
	}

	return hr;
}

HRESULT CNeurone::UnloadAccessibility (VOID)
{
	HRESULT hr = S_FALSE;
	if(m_lpAccessible)
	{
		m_lpAccessible->Delete();
		m_lpAccessible->Release();
		m_lpAccessible = NULL;
		hr = S_OK;
	}
	return hr;
}

HRESULT CNeurone::GetGraphInputCaptureObject (INeuralDocument* lpDocument, IGraphInputCapture** lplpCapture)
{
	UNREFERENCED_PARAMETER(lpDocument);
	UNREFERENCED_PARAMETER(lplpCapture);

	return E_NOTIMPL;
}

//

BOOL CNeurone::GetAxonPosition (INT& x, INT& y)
{
	BOOL fSuccess = TRUE;
	INT iRadius = m_iRadius;
	switch(m_iAxonPosition)
	{
	case 0:
		x = m_x + iRadius + 4;
		y = m_y;
		break;
	case 1:
		x = m_x + iRadius + 1;
		y = m_y - iRadius - 1;
		break;
	case 2:
		x = m_x;
		y = m_y - iRadius - 4;
		break;
	case 3:
		x = m_x - iRadius - 1;
		y = m_y - iRadius - 1;
		break;
	case 4:
		x = m_x - iRadius - 4;
		y = m_y;
		break;
	case 5:
		x = m_x - iRadius - 1;
		y = m_y + iRadius + 1;
		break;
	case 6:
		x = m_x;
		y = m_y + iRadius + 4;
		break;
	case 7:
		x = m_x + iRadius + 1;
		y = m_y + iRadius + 1;
		break;
	default:
		fSuccess = FALSE;
		break;
	}
	return fSuccess;
}

HRESULT CNeurone::CmdFire (VOID)
{
	if(m_fAccumulator < m_fThreshold)
		m_fAccumulator = m_fThreshold;
	CheckThresholds();
	return S_OK;
}