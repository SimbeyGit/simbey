#include <math.h>
#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Core\StringCore.h"
#include "Library\GraphCtrl.h"
#include "NeuralAPI.h"
#include "GdiList.h"
#include "AccessibleNeurone.h"
#include "NeuralChip.h"

CNeuralChip::CNeuralChip ()
{
	m_cRef = 1;

	m_x = 0;
	m_y = 0;

	m_dwState = NSTATE_NORMAL;

	ZeroMemory(&m_rcPos,sizeof(RECT));

	m_lpNet = NULL;

	m_lpInput = NULL;
	m_cInputPins = 0;
	m_nHighlightPin = 0;

	m_lpOutput = NULL;
	m_cOutputPins = 0;
	m_nHighlightPinConn = 0;
	m_iHighlightIndex = -1;

	m_lpAccessible = NULL;
}

CNeuralChip::~CNeuralChip ()
{
	Assert(NULL == m_lpAccessible);

	Assert(m_lpNet == NULL);
	Assert(m_lpInput == NULL);
	Assert(m_lpOutput == NULL);
}

// IUnknown

HRESULT WINAPI CNeuralChip::QueryInterface (REFIID iid, LPVOID* lplpvObject)
{
	HRESULT hr = E_INVALIDARG;
	if(lplpvObject)
	{
		if(iid == __uuidof(INeurone))
			*lplpvObject = (INeurone*)this;
		else if(iid == __uuidof(INeuralChip))
			*lplpvObject = (INeuralChip*)this;
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

ULONG WINAPI CNeuralChip::AddRef (VOID)
{
	return (ULONG)InterlockedIncrement((LONG*)&m_cRef);
}

ULONG WINAPI CNeuralChip::Release (VOID)
{
	ULONG c = (ULONG)InterlockedDecrement((LONG*)&m_cRef);
	if(c == 0)
		__delete this;
	return c;
}

// INetCycleProcessor

VOID CNeuralChip::SendPulses (VOID)
{
	for(INT n = 0; n < m_cOutputPins; n++)
		m_lpOutput[n].fHighlight = FALSE;

	m_lpNet->SendPulses();
}

VOID CNeuralChip::CheckThresholds (VOID)
{
	m_lpNet->CheckThresholds();
}

// INetObject

HRESULT CNeuralChip::GetObjectClass (LPSTR lpszClass, INT cchMaxClass, __out INT* pcchClass)
{
	return TStrCchCpyLen(lpszClass, cchMaxClass, SLP("CNeuralChip"), pcchClass);
}

VOID CNeuralChip::GetPosition (INT& x, INT& y)
{
	x = m_x;
	y = m_y;
}

VOID CNeuralChip::ReceiveValue (FLOAT fValue, ULONG iPin)
{
	UNREFERENCED_PARAMETER(fValue);
	UNREFERENCED_PARAMETER(iPin);

	INT n = -1;
	if(FindPin(iPin,m_lpInput,m_cInputPins,&n))
		m_lpInput[n].lpNeurone->ReceiveValue(fValue,iPin);
}

ULONG CNeuralChip::GetInputPin (INT x, INT y)
{
	ULONG iPin = 0xFFFFFFFF;
	if(x < m_x)	// Inputs are always on the left side
	{
		INT n = -1;
		if(FindPinByPos(x,y,m_lpInput,m_cInputPins,&n))
			iPin = m_lpInput[n].iPin;
	}
	return iPin;
}

BOOL CNeuralChip::GetInputPinPosition (ULONG iPin, INT& x, INT& y)
{
	INT n = -1;
	BOOL fSuccess = FindPin(iPin,m_lpInput,m_cInputPins,&n);
	if(fSuccess)
	{
		x = m_lpInput[n].ptPin.x;
		y = m_lpInput[n].ptPin.y;
	}
	return fSuccess;
}

HRESULT CNeuralChip::Load (INeuralFactory* pFactory, LPNLOADDATA lpLoadData, DWORD cLoadData, LPBYTE lpData, ULONG cbData)
{
	HRESULT hr = E_UNEXPECTED;
	if(m_lpNet == NULL)
	{
		m_lpNet = __new CNeuralNet(pFactory);
		if(m_lpNet)
		{
			hr = m_lpNet->Load(lpLoadData->lpLinks, &lpData, &cbData);

			if(SUCCEEDED(hr))
			{
				LPVOID lpList[] = {&m_x, &m_y, &m_dwState};
				ULONG cbList[] = {sizeof(m_x), sizeof(m_y), sizeof(m_dwState)};
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
			}

			if(SUCCEEDED(hr))
				hr = LoadPins(m_lpNet->GetObjects());

			if(SUCCEEDED(hr))
			{
				if(cbData >= sizeof(INT))
				{
					INT c;
					CopyMemory(&c,lpData,sizeof(INT));
					lpData += sizeof(INT);
					cbData -= sizeof(INT);

					if(c == m_cOutputPins)
					{
						for(INT i = 0; i < c; i++)
						{
							hr = CConnList::LoadConnections(this,m_lpOutput[i].iPin,lpLoadData,cLoadData,&lpData,&cbData);
							if(FAILED(hr))
								break;
						}
					}
					else
						hr = E_FAIL;
				}
				else
					hr = E_FAIL;
			}

			if(SUCCEEDED(hr))
				RecalculatePosition();
			else
				Reset();
		}
		else
			hr = E_OUTOFMEMORY;
	}
	return hr;
}

HRESULT CNeuralChip::Save (INeuralNet* lpNet, ISequentialStream* lpStream)
{
	HRESULT hr = m_lpNet->Save(lpStream);
	if(SUCCEEDED(hr))
	{
		ULONG cbWritten;
		LPVOID lpList[] = {&m_x, &m_y, &m_dwState};
		ULONG cbList[] = {sizeof(m_x), sizeof(m_y), sizeof(m_dwState)};
		INT c = sizeof(cbList) / sizeof(cbList[0]);

		for(INT i = 0; i < c; i++)
		{
			hr = lpStream->Write(lpList[i],cbList[i],&cbWritten);
			if(FAILED(hr))
				break;
		}

		if(SUCCEEDED(hr))
			hr = lpStream->Write(&m_cOutputPins,sizeof(INT),&cbWritten);

		if(SUCCEEDED(hr))
		{
			for(INT i = 0; i < m_cOutputPins; i++)
			{
				hr = CConnList::SaveConnections(lpNet,lpStream,m_lpOutput[i].lpConn,m_lpOutput[i].cConn);
				if(FAILED(hr))
					break;
			}
		}
	}
	return hr;
}

// INeurone

VOID CNeuralChip::ResetNeurone (VOID)
{
}

INT CNeuralChip::CountConnections (ULONG iSourcePin)
{
	INT cConn = 0, n = -1;
	if(FindPin(iSourcePin,m_lpOutput,m_cOutputPins,&n))
		cConn = m_lpOutput[n].cConn;
	return cConn;
}

VOID CNeuralChip::EnumConnections (ULONG iSourcePin, ENUMCONNLIST lpfnCallback, LPVOID lpParam)
{
	INT n = -1;
	if(FindPin(iSourcePin,m_lpOutput,m_cOutputPins,&n))
	{
		LPCLIST lpConn = m_lpOutput[n].lpConn;
		INT cConn = m_lpOutput[n].cConn;

		if(cConn > 0)
		{
			LPCLIST lpNew = __new CLIST[cConn];
			if(lpNew)
			{
				INT i;
				CopyMemory(lpNew,lpConn,sizeof(CLIST) * cConn);
				for(i = 0; i < cConn; i++)
					lpfnCallback(i,lpNew[i].lpTarget,lpNew[i].lpTarget,lpNew[i].iPin,lpNew[i].fWeight,lpParam);
				__delete_array lpNew;
			}
		}
	}
}

BOOL CNeuralChip::ClearConnection (ULONG iSourcePin, INT index)
{
	BOOL fSuccess = FALSE;
	INT n = -1;
	if(FindPin(iSourcePin,m_lpOutput,m_cOutputPins,&n))
		fSuccess = CConnList::ClearConnection(&m_lpOutput[n].lpConn,&m_lpOutput[n].cConn,index);
	return fSuccess;
}

BOOL CNeuralChip::SetConnectionWeight (ULONG iSourcePin, INT index, FLOAT fWeight)
{
	BOOL fSuccess = FALSE;
	INT n = -1;
	if(FindPin(iSourcePin,m_lpOutput,m_cOutputPins,&n))
	{
		LPCLIST lpConn = m_lpOutput[n].lpConn;
		INT cConn = m_lpOutput[n].cConn;

		if(index >= 0 && index < cConn)
		{
			lpConn[index].fWeight = fWeight;
			fSuccess = TRUE;
		}
	}
	return fSuccess;
}

BOOL CNeuralChip::GetCurrentValue (FLOAT& fValue)
{
	UNREFERENCED_PARAMETER(fValue);

	return FALSE;
}

VOID CNeuralChip::SetThreshold (FLOAT fThreshold)
{
	UNREFERENCED_PARAMETER(fThreshold);
}

FLOAT CNeuralChip::GetThreshold (VOID)
{
	return 0.0f;
}

VOID CNeuralChip::RunTrainer (DWORD dwTrainer, FLOAT fInput, ULONG iInputPin, FLOAT fOutput, ULONG iOutputPin)
{
	UNREFERENCED_PARAMETER(dwTrainer);
	UNREFERENCED_PARAMETER(fInput);
	UNREFERENCED_PARAMETER(iInputPin);
	UNREFERENCED_PARAMETER(fOutput);
	UNREFERENCED_PARAMETER(iOutputPin);
}

// INeuralChip

INeuralNet* CNeuralChip::GetEmbeddedNet (VOID)
{
	return m_lpNet;
}

VOID CNeuralChip::ReceiveOutputValue (FLOAT fValue, ULONG iPin)
{
	INT n = -1;
	if(FindPin(iPin,m_lpOutput,m_cOutputPins,&n))
	{
		LPCLIST lpList = m_lpOutput[n].lpConn;
		INT c = m_lpOutput[n].cConn;

		for(INT i = 0; i < c; i++)
			lpList[i].lpTarget->ReceiveValue(fValue * lpList[i].fWeight,lpList[i].iPin);

		m_lpOutput[n].fHighlight = TRUE;
	}
}

VOID CNeuralChip::UnloadEmbeddedNet (VOID)
{
	Reset();
}

// INetDocObject

INT CNeuralChip::GetZOrder (VOID)
{
	return 1;
}

VOID CNeuralChip::DrawBackground (IGrapher* lpGraph)
{
	LPCLIST lpConn;
	INT cConn, xPin, yPin;
	FLOAT fxAxon, fyAxon;

	for(INT n = 0; n < m_cOutputPins; n++)
	{
		lpConn = m_lpOutput[n].lpConn;
		cConn = m_lpOutput[n].cConn;
		fxAxon = (FLOAT)m_lpOutput[n].ptPin.x;
		fyAxon = (FLOAT)m_lpOutput[n].ptPin.y;

		if(m_lpOutput[n].iPin == m_nHighlightPinConn)
		{
			HPEN hpnDef = NULL;
			if(m_lpOutput[n].fHighlight)
				hpnDef = lpGraph->SelectPen(GdiList::hpnLiveConn);

			for(INT i = 0; i < cConn; i++)
			{
				if(m_iHighlightIndex == i)
				{
					HPEN hpnDef = lpGraph->SelectPen(GdiList::hpnHighlight);

					lpGraph->MoveTo(fxAxon,fyAxon,0.0f);
					lpConn[i].lpTarget->GetInputPinPosition(lpConn[i].iPin,xPin,yPin);
					lpGraph->LineTo((FLOAT)xPin,(FLOAT)yPin,0.0f);

					lpGraph->SelectPen(hpnDef);
				}
				else
				{
					lpGraph->MoveTo(fxAxon,fyAxon,0.0f);
					lpConn[i].lpTarget->GetInputPinPosition(lpConn[i].iPin,xPin,yPin);
					lpGraph->LineTo((FLOAT)xPin,(FLOAT)yPin,0.0f);
				}
			}

			if(hpnDef)
				lpGraph->SelectPen(hpnDef);
		}
		else if(m_lpOutput[n].fHighlight)
		{
			HPEN hpnDef = lpGraph->SelectPen(GdiList::hpnLiveConn);

			for(INT i = 0; i < cConn; i++)
			{
				lpGraph->MoveTo(fxAxon,fyAxon,0.0f);
				lpConn[i].lpTarget->GetInputPinPosition(lpConn[i].iPin,xPin,yPin);
				lpGraph->LineTo((FLOAT)xPin,(FLOAT)yPin,0.0f);
			}

			lpGraph->SelectPen(hpnDef);
		}
		else
		{
			for(INT i = 0; i < cConn; i++)
			{
				lpGraph->MoveTo(fxAxon,fyAxon,0.0f);
				lpConn[i].lpTarget->GetInputPinPosition(lpConn[i].iPin,xPin,yPin);
				lpGraph->LineTo((FLOAT)xPin,(FLOAT)yPin,0.0f);
			}
		}
	}
}

VOID CNeuralChip::DrawForeground (IGrapher* lpGraph)
{
	if(m_dwState & NSTATE_SELECTED)
		lpGraph->SelectPen(GdiList::hpnSelected);
	else
		lpGraph->SelectPen(GdiList::hpnBorder);

	lpGraph->SelectBrush(GdiList::hbrRest);

	lpGraph->RoundRect((FLOAT)(m_rcPos.left + DEFAULT_RADIUS),(FLOAT)m_rcPos.top,0.0f,(FLOAT)(m_rcPos.right - DEFAULT_RADIUS),(FLOAT)m_rcPos.bottom,0.0f,DEFAULT_RADIUS * 2.0f,(m_rcPos.top - m_rcPos.bottom) / 2.0f);

	for(INT i = 0; i < m_cInputPins; i++)
	{
		if(m_nHighlightPin == m_lpInput[i].iPin)
		{
			HBRUSH hbrDef = lpGraph->SelectBrush(GdiList::hbrTarget);
			lpGraph->Ellipse((FLOAT)(m_lpInput[i].ptPin.x - AXON_RADIUS),(FLOAT)(m_lpInput[i].ptPin.y - AXON_RADIUS),0.0f,(FLOAT)(m_lpInput[i].ptPin.x + AXON_RADIUS),(FLOAT)(m_lpInput[i].ptPin.y + AXON_RADIUS),0.0f);
			lpGraph->SelectBrush(hbrDef);
		}
		else
			lpGraph->Ellipse((FLOAT)(m_lpInput[i].ptPin.x - AXON_RADIUS),(FLOAT)(m_lpInput[i].ptPin.y - AXON_RADIUS),0.0f,(FLOAT)(m_lpInput[i].ptPin.x + AXON_RADIUS),(FLOAT)(m_lpInput[i].ptPin.y + AXON_RADIUS),0.0f);
	}

	for(INT i = 0; i < m_cOutputPins; i++)
		lpGraph->Ellipse((FLOAT)(m_lpOutput[i].ptPin.x - AXON_RADIUS),(FLOAT)(m_lpOutput[i].ptPin.y - AXON_RADIUS),0.0f,(FLOAT)(m_lpOutput[i].ptPin.x + AXON_RADIUS),(FLOAT)(m_lpOutput[i].ptPin.y + AXON_RADIUS),0.0f);
}

VOID CNeuralChip::SelectObject (BOOL fSelect)
{
	if (fSelect)
		m_dwState |= NSTATE_SELECTED;
	else
		m_dwState &= ~NSTATE_SELECTED;

	if(m_lpAccessible)
		m_lpAccessible->Select(fSelect);
}

VOID CNeuralChip::MoveObject (INT xDelta, INT yDelta)
{
	m_x += xDelta;
	m_y += yDelta;

	RecalculatePosition();

	if(m_lpAccessible)
		m_lpAccessible->MoveTo(m_x,m_y);
}

VOID CNeuralChip::ResizeObject (INT nHitTest, INT xResizePos, INT yResizePos, INT xDelta, INT yDelta)
{
	UNREFERENCED_PARAMETER(nHitTest);
	UNREFERENCED_PARAMETER(xResizePos);
	UNREFERENCED_PARAMETER(yResizePos);
	UNREFERENCED_PARAMETER(xDelta);
	UNREFERENCED_PARAMETER(yDelta);
}

INT CNeuralChip::HitTest (INT x, INT y)
{
	INT nHitTest = HITTEST_NONE;
	if(x >= m_rcPos.left && x < m_rcPos.right)
	{
		if(y <= m_rcPos.top && y > m_rcPos.bottom)
		{
			if(x >= m_x - DEFAULT_RADIUS && x < m_x + DEFAULT_RADIUS)
				nHitTest = HITTEST_SELECTABLE;
			else if(GetInputPin(x,y) != 0xFFFFFFFF)
				nHitTest = HITTEST_DRAG_TARGET;
			else
			{
				ULONG iPin;
				
				if(GetDragSourcePin(x,y,iPin))
					nHitTest = HITTEST_DRAG_SOURCE;
			}
		}
	}
	return nHitTest;
}

BOOL CNeuralChip::GetDragSourcePin (INT x, INT y, ULONG& iPin)
{
	BOOL fSuccess = FALSE;
	if(x > m_x)	// Outputs are always on the right side
	{
		INT n = -1;
		if(FindPinByPos(x,y,m_lpOutput,m_cOutputPins,&n))
		{
			iPin = m_lpOutput[n].iPin;
			fSuccess = TRUE;
		}
	}
	return fSuccess;
}

BOOL CNeuralChip::GetDragSourcePoint (ULONG iPin, INT& x, INT& y)
{
	BOOL fSuccess = FALSE;
	INT n = -1;
	if(FindPin(iPin,m_lpOutput,m_cOutputPins,&n))
	{
		x = m_lpOutput[n].ptPin.x;
		y = m_lpOutput[n].ptPin.y;
		fSuccess = TRUE;
	}
	return fSuccess;
}

BOOL CNeuralChip::HighlightPin (ULONG iPin, BOOL fHighlight)
{
	BOOL fSuccess = FALSE;
	if(fHighlight)
	{
		INT n = -1;
		if(FindPin(iPin,m_lpInput,m_cInputPins,&n))
		{
			m_nHighlightPin = iPin;
			fSuccess = TRUE;
		}
	}
	else
	{
		m_nHighlightPin = 0;
		fSuccess = TRUE;
	}
	return fSuccess;
}

BOOL CNeuralChip::HighlightConn (ULONG iPin, INT index)
{
	BOOL fSuccess = FALSE;
	if(index == -1)
	{
		m_nHighlightPinConn = 0;
		fSuccess = TRUE;
	}
	else
	{
		INT n = -1;
		if(FindPin(iPin,m_lpOutput,m_cOutputPins,&n))
		{
			if(index >= 0 && index < m_lpOutput[n].cConn)
			{
				m_nHighlightPinConn = iPin;
				m_iHighlightIndex = index;
				fSuccess = TRUE;
			}
		}
	}
	return fSuccess;
}

VOID CNeuralChip::NotifyRemovalOf (INetDocObject* lpObject)
{
	INeurone* lpNeurone = NULL;
	if(SUCCEEDED(lpObject->QueryInterface(&lpNeurone)))
	{
		LPCLIST lpConn;
		INT cConn;

		for(INT n = 0; n < m_cOutputPins; n++)
		{
			lpConn = m_lpOutput[n].lpConn;
			cConn = m_lpOutput[n].cConn;

			for(INT i = 0; i < cConn; i++)
			{
				if(lpConn[i].lpTarget == lpNeurone)
				{
					if(CConnList::ClearConnection(&m_lpOutput[n].lpConn,&m_lpOutput[n].cConn,i))
					{
						lpConn = m_lpOutput[n].lpConn;
						cConn = m_lpOutput[n].cConn;
						i--;
					}
				}
			}
		}
		lpNeurone->Release();
	}
}

HRESULT CNeuralChip::ConnectTo (ULONG iSourcePin, INetDocObject* lpTarget, ULONG iTargetPin, FLOAT fWeight)
{
	HRESULT hr = E_FAIL;
	INT n = -1;
	if(FindPin(iSourcePin,m_lpOutput,m_cOutputPins,&n))
		hr = CConnList::ConnectTo(&m_lpOutput[n].lpConn,&m_lpOutput[n].cConn,lpTarget,iTargetPin,fWeight);
	return hr;
}

HRESULT CNeuralChip::ContextMenu (IBaseContainer* lpContainer, INT x, INT y)
{
	UNREFERENCED_PARAMETER(lpContainer);
	UNREFERENCED_PARAMETER(x);
	UNREFERENCED_PARAMETER(y);

	return E_NOTIMPL;
}

BOOL CNeuralChip::Click (INT x, INT y)
{
	UNREFERENCED_PARAMETER(x);
	UNREFERENCED_PARAMETER(y);

	return FALSE;
}

BOOL CNeuralChip::PressChar (CHAR ch)
{
	UNREFERENCED_PARAMETER(ch);

	return FALSE;
}

LONG CNeuralChip::GetAccState (VOID)
{
	LONG lAccState = STATE_SYSTEM_MULTISELECTABLE;
	if(m_dwState & NSTATE_SELECTED)
		lAccState |= STATE_SYSTEM_SELECTED;
	return lAccState;
}

HRESULT CNeuralChip::GetAccObject (IAccessibleNetDoc* lpParent, IAccessible** lplpAccessible)
{
	HRESULT hr = E_OUTOFMEMORY;

	if(m_lpAccessible == NULL)
	{
		m_lpAccessible = __new CAccessibleNeurone(lpParent);
		if(m_lpAccessible)
		{
			hr = m_lpAccessible->Initialize(this,AXON_RADIUS << 1,m_rcPos.top - m_rcPos.bottom);
			if(SUCCEEDED(hr))
			{
				INT cPins = m_cInputPins + m_cOutputPins;
				POINT* lpPins = __new POINT[cPins];
				if(lpPins)
				{
					INT i;

					for(i = 0; i < m_cInputPins; i++)
					{
						lpPins[i].x = m_lpInput[i].ptPin.x - m_x;
						lpPins[i].y = m_lpInput[i].ptPin.y - m_y;
					}

					for(i = 0; i < m_cOutputPins; i++)
					{
						lpPins[m_cInputPins + i].x = m_lpOutput[i].ptPin.x - m_x;
						lpPins[m_cInputPins + i].y = m_lpOutput[i].ptPin.y - m_y;
					}

					hr = m_lpAccessible->InitConnectors(lpPins,cPins);

					__delete_array lpPins;
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

HRESULT CNeuralChip::UnloadAccessibility (VOID)
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

HRESULT CNeuralChip::GetGraphInputCaptureObject (INeuralDocument* lpDocument, IGraphInputCapture** lplpCapture)
{
	UNREFERENCED_PARAMETER(lpDocument);
	UNREFERENCED_PARAMETER(lplpCapture);

	return E_NOTIMPL;
}

HRESULT CNeuralChip::LoadFromFile (INeuralFactory* pFactory, INeuralLinks* lpLinks, PCSTR pcszFile, INT cchFile)
{
	HRESULT hr = E_UNEXPECTED;
	if(m_lpNet == NULL)
	{
		m_lpNet = __new CNeuralNet(pFactory);
		if(m_lpNet)
		{
			hr = CNeuralNet::LoadFromFile(m_lpNet, lpLinks, pcszFile, cchFile, TRUE);

			if(SUCCEEDED(hr))
				hr = LoadPins(m_lpNet->GetObjects());

			if(SUCCEEDED(hr))
				RecalculatePosition();
			else
				Reset();
		}
		else
			hr = E_OUTOFMEMORY;
	}
	return hr;
}

HRESULT CNeuralChip::LoadPins (LPNLIST lpObjects)
{
	HRESULT hr = HRESULT_FROM_WIN32(ERROR_CONNECTION_UNAVAIL);
	LPNLIST lpList = lpObjects;
	IIONeurone* lpIO;
	INT cInput = 0, cOutput = 0;
	while(lpList)
	{
		if(SUCCEEDED(lpList->lpObject->QueryInterface(&lpIO)))
		{
			lpIO->AttachParentChip(this);

			switch(lpIO->GetIOType())
			{
			case INPUT_NEURONE:
				cInput++;
				break;
			case OUTPUT_NEURONE:
				cOutput++;
				break;
			}
			lpIO->Release();
		}
		lpList = lpList->Next;
	}
	if(cInput > 0 || cOutput > 0)
	{
		hr = S_OK;

		if(cInput > 0)
		{
			m_lpInput = __new CHIP_PIN[cInput];
			if(m_lpInput)
				ZeroMemory(m_lpInput,sizeof(CHIP_PIN) * cInput);
			else
				hr = E_OUTOFMEMORY;
		}

		if(hr == S_OK && cOutput > 0)
		{
			m_lpOutput = __new CHIP_PIN[cOutput];
			if(m_lpOutput)
				ZeroMemory(m_lpOutput,sizeof(CHIP_PIN) * cOutput);
			else
				hr = E_OUTOFMEMORY;
		}

		if(hr == S_OK)
		{
			lpList = lpObjects;
			while(lpList)
			{
				if(SUCCEEDED(lpList->lpObject->QueryInterface(&lpIO)))
				{
					LPCHIP_PIN lpSet = NULL;
					INT cSet = 0, n = -1, *lpcList = NULL;
					ULONG iPin = lpIO->GetPin();

					switch(lpIO->GetIOType())
					{
					case INPUT_NEURONE:
						lpSet = m_lpInput;
						cSet = cInput;
						lpcList = &m_cInputPins;
						break;
					case OUTPUT_NEURONE:
						lpSet = m_lpOutput;
						cSet = cOutput;
						lpcList = &m_cOutputPins;
						break;
					}

					lpIO->Release();

					if(lpSet && FindPin(iPin,lpSet,*lpcList,&n) == FALSE)
					{
						Assert(n >= 0 && n < cSet);

						hr = lpList->lpObject->QueryInterface(&lpSet[n].lpNeurone);
						if(FAILED(hr))
							break;

						MoveMemory(lpSet + n + 1,lpSet + n,sizeof(CHIP_PIN) * (*lpcList - n));
						lpSet[n].iPin = iPin;
						(*lpcList)++;
					}
					else
					{
						hr = E_FAIL;
						break;
					}
				}
				lpList = lpList->Next;
			}
		}
	}
	return hr;
}

VOID CNeuralChip::ResetChipPins (LPCHIP_PIN& pPins, INT& cPins)
{
	if(pPins)
	{
		for(INT i = 0; i < cPins; i++)
		{
			IIONeurone* lpIO;
			if(SUCCEEDED(pPins[i].lpNeurone->QueryInterface(&lpIO)))
			{
				lpIO->AttachParentChip(NULL);
				lpIO->Release();
			}
			pPins[i].lpNeurone->Release();

			if(0 < pPins[i].cConn)
			{
				LPCLIST pConnections = pPins[i].lpConn;
				INT cConnections = pPins[i].cConn;

				for(INT n = 0; n < cConnections; n++)
					pConnections[n].lpTarget->Release();

				__delete_array pConnections;
			}
		}
		__delete_array pPins;
		pPins = NULL;
		cPins = 0;
	}
}

VOID CNeuralChip::Reset (VOID)
{
	ResetChipPins(m_lpInput, m_cInputPins);
	ResetChipPins(m_lpOutput, m_cOutputPins);

	SafeRelease(m_lpNet);
}

VOID CNeuralChip::RecalculatePosition (VOID)
{
	INT cSidePins = max(m_cInputPins,m_cOutputPins), x, y;
	INT nHeight;

	if(cSidePins < 3) cSidePins = 3;

	m_rcPos.left = m_x - DEFAULT_RADIUS - (AXON_RADIUS * 2 + 1);
	m_rcPos.right = m_x + DEFAULT_RADIUS + (AXON_RADIUS * 2 + 1);
	m_rcPos.top = m_y + ((cSidePins * (AXON_RADIUS * 2 + 2)) >> 1);
	m_rcPos.bottom = m_y - ((cSidePins * (AXON_RADIUS * 2 + 2)) >> 1);

	nHeight = m_rcPos.top - m_rcPos.bottom;

	x = m_rcPos.left + AXON_RADIUS;
	y = m_rcPos.top - ((nHeight - m_cInputPins * (AXON_RADIUS * 2 + 2)) >> 1);
	y -= AXON_RADIUS + 1;
	for(INT i = 0; i < m_cInputPins; i++)
	{
		m_lpInput[i].ptPin.x = x;
		m_lpInput[i].ptPin.y = y;
		y -= AXON_RADIUS * 2 + 2;
	}

	x = m_rcPos.right - AXON_RADIUS;
	y = m_rcPos.top - ((nHeight - m_cOutputPins * (AXON_RADIUS * 2 + 2)) >> 1);
	y -= AXON_RADIUS + 1;
	for(INT i = 0; i < m_cOutputPins; i++)
	{
		m_lpOutput[i].ptPin.x = x;
		m_lpOutput[i].ptPin.y = y;
		y -= AXON_RADIUS * 2 + 2;
	}
}

BOOL CNeuralChip::FindPinByPos (INT x, INT y, LPCHIP_PIN lpPins, INT cPins, INT* lpnIndex)
{
	BOOL fSuccess = FALSE;
	INT xDiff, yDiff;
	FLOAT fDistance;

	for(INT i = 0; i < cPins; i++)
	{
		xDiff = lpPins[i].ptPin.x - x;
		yDiff = lpPins[i].ptPin.y - y;
		fDistance = sqrtf((FLOAT)(xDiff * xDiff + yDiff * yDiff));
		if(fDistance <= AXON_RADIUS)
		{
			*lpnIndex = i;
			fSuccess = TRUE;
			break;
		}
	}
	return fSuccess;
}

BOOL CNeuralChip::FindPin (ULONG iPin, LPCHIP_PIN lpPins, INT cPins, INT* lpnIndex)
{
	INT iLeft = 0, iRight = cPins - 1, iMiddle;
	while(iLeft <= iRight)
	{
		iMiddle = ((unsigned)(iLeft + iRight)) >> 1;
		if(iPin > lpPins[iMiddle].iPin)
			iLeft = iMiddle + 1;
		else if(iPin < lpPins[iMiddle].iPin)
			iRight = iMiddle - 1;
		else
		{
			*lpnIndex = iMiddle;	// The item has been found at this position
			return TRUE;
		}
	}
	*lpnIndex = iLeft;				// This is the point where the item should be inserted
	return FALSE;
}