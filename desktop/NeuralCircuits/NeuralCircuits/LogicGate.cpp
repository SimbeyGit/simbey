#include <math.h>
#include <windows.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "Library\Core\StringCore.h"
#include "Library\GraphCtrl.h"
#include "NeuralAPI.h"
#include "GdiList.h"
#include "BaseContainer.h"
#include "LogicGate.h"

CLogicGate::CLogicGate ()
{
	m_cRef = 1;

	m_x = 0;
	m_y = 0;

	m_dwState = 0;
	m_nHighlightPin = 0xFFFFFFFF;

	m_fThreshold = 0.5f;

	m_ptInput = NULL;
}

CLogicGate::~CLogicGate ()
{
	__delete_array m_ptInput;
	__delete_array m_pfAccumulators;
}

HRESULT CLogicGate::Initialize (VOID)
{
	HRESULT hr;
	ULONG cPins = GetInputPinCount();

	m_ptInput = __new POINT[cPins];
	if(m_ptInput)
	{
		LONG y = (cPins - 1) * (AXON_RADIUS + 1);
		for(ULONG i = 0; i < cPins; i++)
		{
			m_ptInput[i].x = -DEFAULT_RADIUS - AXON_RADIUS - 2;
			m_ptInput[i].y = y;
			y -= AXON_RADIUS * 2 + 2;
		}
		hr = S_OK;
	}
	else
		hr = E_OUTOFMEMORY;

	if(SUCCEEDED(hr))
	{
		ULONG cPins = GetInputPinCount();
		m_pfAccumulators = __new FLOAT[GetInputPinCount()];
		if(m_pfAccumulators)
			ZeroMemory(m_pfAccumulators, sizeof(FLOAT) * cPins);
		else
			hr = E_OUTOFMEMORY;
	}

	return hr;
}

// IUnknown

HRESULT WINAPI CLogicGate::QueryInterface (REFIID iid, LPVOID* lplpvObject)
{
	HRESULT hr = E_INVALIDARG;
	if(lplpvObject)
	{
		if(iid == __uuidof(INeurone))
			*lplpvObject = (INeurone*)this;
		else if(iid == __uuidof(IOleCommandTarget))
			*lplpvObject = (IOleCommandTarget*)this;
		else if(iid == __uuidof(INetDocObject))
			*lplpvObject = (INetDocObject*)this;
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

ULONG WINAPI CLogicGate::AddRef (VOID)
{
	return (ULONG)InterlockedIncrement((LONG*)&m_cRef);
}

ULONG WINAPI CLogicGate::Release (VOID)
{
	ULONG c = (ULONG)InterlockedDecrement((LONG*)&m_cRef);
	if(c == 0)
		__delete this;
	return c;
}

// INetCycleProcessor

VOID CLogicGate::SendPulses (VOID)
{
	if(m_dwState & NSTATE_EXCITED)
	{
		m_dwState &= ~NSTATE_EXCITED;

        m_Connections.SendPulses();
	}
}

// INetObject

VOID CLogicGate::GetPosition (INT& x, INT& y)
{
	x = m_x;
	y = m_y;
}

VOID CLogicGate::ReceiveValue (FLOAT fValue, ULONG iPin)
{
	Assert(iPin - 1 < GetInputPinCount());

	m_pfAccumulators[iPin - 1] += fValue;
}

ULONG CLogicGate::GetInputPin (INT x, INT y)
{
	ULONG cInputPins = GetInputPinCount();
	for(ULONG i = 0; i < cInputPins; i++)
	{
		INT xDiff = (m_x + m_ptInput[i].x) - x;
		INT yDiff = (m_y + m_ptInput[i].y) - y;
		FLOAT fDistance = sqrtf((FLOAT)(xDiff * xDiff + yDiff * yDiff));
		if(fDistance <= AXON_RADIUS)
		{
			return i + 1;
		}
	}
	return 0;
}

BOOL CLogicGate::GetInputPinPosition (ULONG iPin, INT& x, INT& y)
{
	BOOL fSuccess = FALSE;
	if(--iPin <= GetInputPinCount())
	{
		x = m_x + m_ptInput[iPin].x;
		y = m_y + m_ptInput[iPin].y;
		fSuccess = TRUE;
	}
	return fSuccess;
}

HRESULT CLogicGate::Load (INeuralFactory* pFactory, LPNLOADDATA lpLoadData, DWORD cLoadData, LPBYTE lpData, ULONG cbData)
{
	UNREFERENCED_PARAMETER(pFactory);

	HRESULT hr = S_OK;

	LPVOID lpList[] = {&m_x, &m_y, &m_dwState, &m_fThreshold};
	ULONG cbList[] = {sizeof(m_x), sizeof(m_y), sizeof(m_dwState), sizeof(m_fThreshold)};
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

HRESULT CLogicGate::Save (INeuralNet* lpNet, ISequentialStream* lpStream)
{
	HRESULT hr = S_OK;
	ULONG cbWritten;

	LPVOID lpList[] = {&m_x, &m_y, &m_dwState, &m_fThreshold};
	ULONG cbList[] = {sizeof(m_x), sizeof(m_y), sizeof(m_dwState), sizeof(m_fThreshold)};

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

VOID CLogicGate::ResetNeurone (VOID)
{
	ZeroMemory(m_pfAccumulators, sizeof(FLOAT) * GetInputPinCount());
}

INT CLogicGate::CountConnections (ULONG iSourcePin)
{
	UNREFERENCED_PARAMETER(iSourcePin);

	Assert(iSourcePin == 0);

	return m_Connections.Count();
}

VOID CLogicGate::EnumConnections (ULONG iSourcePin, ENUMCONNLIST lpfnCallback, LPVOID lpParam)
{
	if(iSourcePin == 0)
		m_Connections.EnumConnections(this, lpfnCallback, lpParam);
}

BOOL CLogicGate::ClearConnection (ULONG iSourcePin, INT index)
{
	BOOL fSuccess = FALSE;
	if(iSourcePin == 0)
		fSuccess = m_Connections.ClearConnection(index);
	return fSuccess;
}

BOOL CLogicGate::SetConnectionWeight (ULONG iSourcePin, INT index, FLOAT fWeight)
{
	BOOL fSuccess = FALSE;
	if(iSourcePin == 0)
		fSuccess = m_Connections.SetConnectionWeight(index, fWeight);
	return fSuccess;
}

BOOL CLogicGate::GetCurrentValue (FLOAT& fValue)
{
	fValue = NSTATE_EXCITED == (m_dwState & NSTATE_EXCITED) ? m_fThreshold : 0.0f;
	return TRUE;
}

VOID CLogicGate::SetThreshold (FLOAT fThreshold)
{
	m_fThreshold = fThreshold;
}

FLOAT CLogicGate::GetThreshold (VOID)
{
	return m_fThreshold;
}

VOID CLogicGate::RunTrainer (DWORD dwTrainer, FLOAT fInput, ULONG iInputPin, FLOAT fOutput, ULONG iOutputPin)
{
	UNREFERENCED_PARAMETER(dwTrainer);
	UNREFERENCED_PARAMETER(fInput);
	UNREFERENCED_PARAMETER(iInputPin);
	UNREFERENCED_PARAMETER(fOutput);
	UNREFERENCED_PARAMETER(iOutputPin);
}

// INetDocObject

INT CLogicGate::GetZOrder (VOID)
{
	return 1;
}

VOID CLogicGate::DrawBackground (IGrapher* lpGraph)
{
	INT xAxon, yAxon;
	GetAxonPosition(xAxon, yAxon);
	m_Connections.Draw(lpGraph, xAxon, yAxon, NSTATE_EXCITED == (m_dwState & NSTATE_EXCITED));
}

VOID CLogicGate::DrawForeground (IGrapher* lpGraph)
{
	ULONG cPins = GetInputPinCount();
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

	for(ULONG i = 0; i < cPins; i++)
	{
		INT xPin = m_x + m_ptInput[i].x;
		INT yPin = m_y + m_ptInput[i].y;
		if(i == m_nHighlightPin)
		{
			HBRUSH hbrDef = lpGraph->SelectBrush(GdiList::hbrTarget);
			lpGraph->Ellipse((FLOAT)(xPin - AXON_RADIUS),(FLOAT)(yPin - AXON_RADIUS),0.0f,(FLOAT)(xPin + AXON_RADIUS),(FLOAT)(yPin + AXON_RADIUS),0.0f);
			lpGraph->SelectBrush(hbrDef);
		}
		else
			lpGraph->Ellipse((FLOAT)(xPin - AXON_RADIUS),(FLOAT)(yPin - AXON_RADIUS),0.0f,(FLOAT)(xPin + AXON_RADIUS),(FLOAT)(yPin + AXON_RADIUS),0.0f);
	}

	GetAxonPosition(xAxon,yAxon);
	lpGraph->Ellipse((FLOAT)(xAxon - AXON_RADIUS),(FLOAT)(yAxon - AXON_RADIUS),0.0f,(FLOAT)(xAxon + AXON_RADIUS),(FLOAT)(yAxon + AXON_RADIUS),0.0f);
}

VOID CLogicGate::SelectObject (BOOL fSelect)
{
	if (fSelect)
		m_dwState |= NSTATE_SELECTED;
	else
		m_dwState &= ~NSTATE_SELECTED;

	//if(m_lpAccessible)
	//	m_lpAccessible->Select(fSelect);
}

VOID CLogicGate::MoveObject (INT xDelta, INT yDelta)
{
	m_x += xDelta;
	m_y += yDelta;

	//if(m_lpAccessible)
	//	m_lpAccessible->MoveTo(m_x,m_y);
}

VOID CLogicGate::ResizeObject (INT nHitTest, INT xResizePos, INT yResizePos, INT xDelta, INT yDelta)
{
	UNREFERENCED_PARAMETER(nHitTest);
	UNREFERENCED_PARAMETER(xResizePos);
	UNREFERENCED_PARAMETER(yResizePos);
	UNREFERENCED_PARAMETER(xDelta);
	UNREFERENCED_PARAMETER(yDelta);
}

INT CLogicGate::HitTest (INT x, INT y)
{
	INT xAxon, yAxon;
	GetAxonPosition(xAxon, yAxon);
	if(x >= m_x + m_ptInput[0].x - AXON_RADIUS && x <= xAxon + AXON_RADIUS)
	{
		if(y <= m_y + m_ptInput[0].y + AXON_RADIUS && y >= m_y + m_ptInput[GetInputPinCount() - 1].y - AXON_RADIUS)
		{
			INT xRel = x - m_x;
			INT yRel = y - m_y;
			FLOAT xDiff, yDiff, fDistance;
			ULONG cPins = GetInputPinCount();

			for(ULONG i = 0; i < cPins; i++)
			{
				xDiff = (FLOAT)(m_ptInput[i].x - xRel);
				yDiff = (FLOAT)(m_ptInput[i].y - yRel);
				fDistance = sqrtf(xDiff * xDiff + yDiff * yDiff);
				if(fDistance <= (FLOAT)AXON_RADIUS)
					return HITTEST_DRAG_TARGET;
			}

			xDiff = (FLOAT)(x - xAxon);
			yDiff = (FLOAT)(y - yAxon);
			fDistance = sqrtf(xDiff * xDiff + yDiff * yDiff);
			if(fDistance < (FLOAT)AXON_RADIUS)
				return HITTEST_DRAG_SOURCE | HITTEST_CONTEXT;

			if(PointOverGate(x, y))
   				return HITTEST_SELECTABLE | HITTEST_CONTEXT;
		}
	}
	return HITTEST_NONE;
}

BOOL CLogicGate::GetDragSourcePin (INT x, INT y, ULONG& iPin)
{
	BOOL fSuccess = FALSE;
	INT xAxon, yAxon;
	GetAxonPosition(xAxon, yAxon);
	FLOAT xDiff = (FLOAT)(xAxon - x);
	FLOAT yDiff = (FLOAT)(yAxon - y);
	if(sqrtf(xDiff * xDiff + yDiff + yDiff) <= (FLOAT)AXON_RADIUS)
	{
		iPin = 0;
		fSuccess = TRUE;
	}
	return fSuccess;
}

BOOL CLogicGate::GetDragSourcePoint (ULONG iPin, INT& x, INT& y)
{
	BOOL fSuccess = FALSE;
	if(0 == iPin)
	{
		GetAxonPosition(x, y);
		fSuccess = TRUE;
	}
	return fSuccess;
}

BOOL CLogicGate::HighlightPin (ULONG iPin, BOOL fHighlight)
{
	BOOL fSuccess = FALSE;
	if(fHighlight)
	{
		if(iPin - 1 < GetInputPinCount())
		{
			m_nHighlightPin = iPin - 1;
			fSuccess = TRUE;
		}
	}
	else
	{
		m_nHighlightPin = 0xFFFFFFFF;
		fSuccess = TRUE;
	}
	return fSuccess;
}

BOOL CLogicGate::HighlightConn (ULONG iPin, INT index)
{
	return (-1 == index || 0 == iPin) ? m_Connections.Highlight(index) : FALSE;
}

VOID CLogicGate::NotifyRemovalOf (INetDocObject* lpObject)
{
	INeurone* lpNeurone = NULL;
	if(SUCCEEDED(lpObject->QueryInterface(&lpNeurone)))
	{
		m_Connections.RemoveConnectionsTo(lpNeurone);
		lpNeurone->Release();
	}
}

HRESULT CLogicGate::ConnectTo (ULONG iSourcePin, INetDocObject* lpTarget, ULONG iTargetPin, FLOAT fWeight)
{
	HRESULT hr = E_FAIL;
	if(iSourcePin == 0)
		hr = m_Connections.ConnectTo(lpTarget,iTargetPin,fWeight);
	return hr;
}

HRESULT CLogicGate::ContextMenu (IBaseContainer* lpContainer, INT x, INT y)
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

BOOL CLogicGate::Click (INT x, INT y)
{
	UNREFERENCED_PARAMETER(x);
	UNREFERENCED_PARAMETER(y);

	return FALSE;
}

BOOL CLogicGate::PressChar (CHAR ch)
{
	UNREFERENCED_PARAMETER(ch);

	return FALSE;
}

LONG CLogicGate::GetAccState (VOID)
{
	LONG lAccState = STATE_SYSTEM_MULTISELECTABLE;
	if(m_dwState & NSTATE_SELECTED)
		lAccState |= STATE_SYSTEM_SELECTED;
	return lAccState;
}

HRESULT CLogicGate::GetAccObject (IAccessibleNetDoc* lpParent, IAccessible** lplpAccessible)
{
	return E_NOTIMPL;
}

HRESULT CLogicGate::UnloadAccessibility (VOID)
{
	return S_FALSE;
}

HRESULT CLogicGate::GetGraphInputCaptureObject (INeuralDocument* lpDocument, IGraphInputCapture** lplpCapture)
{
	UNREFERENCED_PARAMETER(lpDocument);
	UNREFERENCED_PARAMETER(lplpCapture);

	return E_NOTIMPL;
}

// IOleCommandTarget

HRESULT WINAPI CLogicGate::QueryStatus (const GUID* lpguidCmdGroup, ULONG cCmds, OLECMD* lprgCmds, OLECMDTEXT* lpCmdText)
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
		}
	}

	return S_OK;
}

HRESULT WINAPI CLogicGate::Exec (const GUID* lpguidCmdGroup, DWORD nCmdID, DWORD nCmdExecOpt, VARIANTARG* lpvaIn, VARIANTARG* lpvaOut)
{
	UNREFERENCED_PARAMETER(lpguidCmdGroup);
	UNREFERENCED_PARAMETER(nCmdExecOpt);
	UNREFERENCED_PARAMETER(lpvaIn);
	UNREFERENCED_PARAMETER(lpvaOut);

	HRESULT hr = OLECMDERR_E_NOTSUPPORTED;

	switch(nCmdID)
	{
	case ID_NEURONE_FIRE:
		m_dwState |= NSTATE_EXCITED;
		hr = S_OK;
		break;
	}

	return hr;
}

BOOL CLogicGate::PointOverGate (INT x, INT y)
{
	FLOAT xDiff = (FLOAT)(m_x - x);
	FLOAT yDiff = (FLOAT)(m_y - y);
	FLOAT fDistance = sqrtf(xDiff * xDiff + yDiff * yDiff);
	return fDistance <= (FLOAT)DEFAULT_RADIUS;
}

VOID CLogicGate::GetAxonPosition (INT& x, INT& y)
{
	x = m_x + DEFAULT_RADIUS + AXON_RADIUS + 1;
	y = m_y;
}
