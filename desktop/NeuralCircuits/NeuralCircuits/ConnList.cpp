#include <Windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\GraphCtrl.h"
#include "NeuralAPI.h"
#include "GdiList.h"
#include "NeuralNet.h"
#include "ConnList.h"

CConnList::CConnList ()
{
	m_iHighlightIndex = -1;

	m_cConn = 0;
	m_lpConn = NULL;
}

CConnList::~CConnList ()
{
	if(m_lpConn)
	{
		for(INT i = 0; i < m_cConn; i++)
			m_lpConn[i].lpTarget->Release();
		__delete_array m_lpConn;
	}
}

INT CConnList::Count (VOID)
{
	return m_cConn;
}

VOID CConnList::EnumConnections (INeurone* pSource, ENUMCONNLIST lpfnCallback, LPVOID lpParam)
{
	if(m_cConn > 0)
	{
		LPCLIST lpNew = __new CLIST[m_cConn];
		if(lpNew)
		{
			INT i;
			CopyMemory(lpNew,m_lpConn,sizeof(CLIST) * m_cConn);
			for(i = 0; i < m_cConn; i++)
				lpfnCallback(i,pSource,lpNew[i].lpTarget,lpNew[i].iPin,lpNew[i].fWeight,lpParam);
			__delete_array lpNew;
		}
	}
}

VOID CConnList::SendPulses (VOID)
{
	for(INT i = 0; i < m_cConn; i++)
		m_lpConn[i].lpTarget->ReceiveValue(m_lpConn[i].fWeight, m_lpConn[i].iPin);
}

VOID CConnList::SendPulses (FLOAT fMultiplier)
{
	for(INT i = 0; i < m_cConn; i++)
		m_lpConn[i].lpTarget->ReceiveValue(fMultiplier * m_lpConn[i].fWeight, m_lpConn[i].iPin);
}

HRESULT CConnList::ConnectTo (INetDocObject* lpTarget, ULONG iTargetPin, FLOAT fWeight)
{
	return ConnectTo(&m_lpConn, &m_cConn, lpTarget, iTargetPin, fWeight);
}

BOOL CConnList::ClearConnection (INT index)
{
	return ClearConnection(&m_lpConn, &m_cConn, index);
}

BOOL CConnList::SetConnectionWeight (INT index, FLOAT fWeight)
{
	BOOL fSuccess = FALSE;
	if(index >= 0 && index < m_cConn)
	{
		m_lpConn[index].fWeight = fWeight;
		fSuccess = TRUE;
	}
	return fSuccess;
}

HRESULT CConnList::SaveConnections (INeuralNet* lpNet, ISequentialStream* lpStream)
{
	return SaveConnections(lpNet, lpStream, m_lpConn, m_cConn);
}

BOOL CConnList::Highlight (INT index)
{
	BOOL fSuccess = FALSE;
	if(index == -1)
	{
		m_iHighlightIndex = -1;
		fSuccess = TRUE;
	}
    else if(index >= 0 && index < m_cConn)
	{
		m_iHighlightIndex = index;
		fSuccess = TRUE;
	}
	return fSuccess;
}

VOID CConnList::RemoveConnectionsTo (INeurone* lpNeurone)
{
	for(INT i = 0; i < m_cConn; i++)
	{
		if(m_lpConn[i].lpTarget == lpNeurone)
		{
			ClearConnection(&m_lpConn, &m_cConn, i);
			i--;
		}
	}
}

VOID CConnList::RunTrainer (DWORD dwTrainer, FLOAT fInput, FLOAT fOutput, ULONG iOutputPin)
{
	UNREFERENCED_PARAMETER(fInput);
	UNREFERENCED_PARAMETER(fOutput);
	UNREFERENCED_PARAMETER(iOutputPin);

	if(TRAIN_RANDOMIZE_WEIGHTS == dwTrainer)
	{
		FLOAT r;
		for(INT i = 0; i < m_cConn; i++)
		{
			r = (FLOAT)(rand() % 1000) / 1000;
			m_lpConn[i].fWeight = r;
		}
	}
}

VOID CConnList::Draw (IGrapher* lpGraph, INT xAxon, INT yAxon, BOOL fExcited)
{
	INT xPin, yPin;
	FLOAT fxAxon = (FLOAT)xAxon;
	FLOAT fyAxon = (FLOAT)yAxon;
	if(m_iHighlightIndex >= 0)
	{
		HPEN hpnDef = NULL;
		if(fExcited)
			hpnDef = lpGraph->SelectPen(GdiList::hpnLiveConn);

		for(INT i = 0; i < m_cConn; i++)
		{
			if(m_iHighlightIndex == i)
			{
				HPEN hpnDef = lpGraph->SelectPen(GdiList::hpnHighlight);

				lpGraph->MoveTo(fxAxon,fyAxon,0.0f);
				m_lpConn[i].lpTarget->GetInputPinPosition(m_lpConn[i].iPin,xPin,yPin);
				lpGraph->LineTo((FLOAT)xPin,(FLOAT)yPin,0.0f);

				lpGraph->SelectPen(hpnDef);
			}
			else
			{
				lpGraph->MoveTo(fxAxon,fyAxon,0.0f);
				m_lpConn[i].lpTarget->GetInputPinPosition(m_lpConn[i].iPin,xPin,yPin);
				lpGraph->LineTo((FLOAT)xPin,(FLOAT)yPin,0.0f);
			}
		}

		if(hpnDef)
			lpGraph->SelectPen(hpnDef);
	}
	else if(fExcited)
	{
		HPEN hpnDef = lpGraph->SelectPen(GdiList::hpnLiveConn);

		for(INT i = 0; i < m_cConn; i++)
		{
			lpGraph->MoveTo(fxAxon,fyAxon,0.0f);
			m_lpConn[i].lpTarget->GetInputPinPosition(m_lpConn[i].iPin,xPin,yPin);
			lpGraph->LineTo((FLOAT)xPin,(FLOAT)yPin,0.0f);
		}

		lpGraph->SelectPen(hpnDef);
	}
	else
	{
		for(INT i = 0; i < m_cConn; i++)
		{
			lpGraph->MoveTo(fxAxon,fyAxon,0.0f);
			m_lpConn[i].lpTarget->GetInputPinPosition(m_lpConn[i].iPin,xPin,yPin);
			lpGraph->LineTo((FLOAT)xPin,(FLOAT)yPin,0.0f);
		}
	}
}

HRESULT CConnList::LoadConnections (INetDocObject* lpSource, ULONG iSourcePin, LPNLOADDATA lpLoadData, DWORD cLoadData, LPBYTE* lplpData, ULONG* lpcbData)
{
	HRESULT hr = S_OK;
	LPBYTE lpData = *lplpData;
	ULONG cbData = *lpcbData;
	if(cbData >= sizeof(INT))
	{
		INT cLoadConn;

		CopyMemory(&cLoadConn,lpData,sizeof(INT));
		lpData += sizeof(INT);
		cbData -= sizeof(INT);

		if(cLoadConn > 0)
		{
			INT index;
			ULONG iPin;
			FLOAT fWeight;

			for(INT i = 0; i < cLoadConn; i++)
			{
				if(cbData < sizeof(index) + sizeof(iPin) + sizeof(fWeight))
				{
					hr = E_FAIL;
					break;
				}

				CopyMemory(&index,lpData,sizeof(index));
				CopyMemory(&iPin,lpData + sizeof(index),sizeof(iPin));
				CopyMemory(&fWeight,lpData + sizeof(index) + sizeof(iPin),sizeof(fWeight));

				lpData += sizeof(index) + sizeof(iPin) + sizeof(fWeight);
				cbData -= sizeof(index) + sizeof(iPin) + sizeof(fWeight);

				if((DWORD)index >= cLoadData)
				{
					hr = E_FAIL;
					break;
				}

				hr = lpSource->ConnectTo(iSourcePin,lpLoadData[index].lpObject,iPin,fWeight);
				if(FAILED(hr))
					break;
			}
		}

		*lplpData = lpData;
		*lpcbData = cbData;
	}
	else
		hr = E_FAIL;
	return hr;
}

HRESULT CConnList::SaveConnections (INeuralNet* lpNet, ISequentialStream* lpStream, LPCLIST lpConn, INT cConn)
{
	ULONG cbWritten;
	HRESULT hr = lpStream->Write(&cConn,sizeof(INT),&cbWritten);
	if(SUCCEEDED(hr) && cConn > 0)
	{
		INetDocObject* lpObject;
		INT index;

		for(INT i = 0; i < cConn; i++)
		{
			if(FAILED(lpConn[i].lpTarget->QueryInterface(&lpObject)))
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

			hr = lpStream->Write(&index,sizeof(index),&cbWritten);
			if(FAILED(hr))
				break;

			hr = lpStream->Write(&lpConn[i].iPin,sizeof(ULONG),&cbWritten);
			if(FAILED(hr))
				break;

			hr = lpStream->Write(&lpConn[i].fWeight,sizeof(FLOAT),&cbWritten);
			if(FAILED(hr))
				break;
		}
	}
	return hr;
}

HRESULT CConnList::ConnectTo (LPCLIST* lplpList, INT* lpcList, INetDocObject* lpTarget, ULONG iTargetPin, FLOAT fWeight)
{
	INeurone* lpNeurone = NULL;
	HRESULT hr = lpTarget->QueryInterface(&lpNeurone);
	if(SUCCEEDED(hr))
	{
		LPCLIST lpConn = *lplpList;
		INT cConn = *lpcList;

		for(INT i = 0; i < cConn; i++)
		{
			if(lpConn[i].lpTarget == lpNeurone && lpConn[i].iPin == iTargetPin)
			{
				hr = S_FALSE;
				break;
			}
		}

		if(S_OK == hr)
		{
			LPCLIST lpNew = __new CLIST[cConn + 1];
			if(lpNew)
			{
				CopyMemory(lpNew,lpConn,cConn * sizeof(CLIST));

				lpNew[cConn].lpTarget = lpNeurone;
				lpNew[cConn].lpTarget->AddRef();
				lpNew[cConn].iPin = iTargetPin;
				lpNew[cConn].fWeight = fWeight;
				__delete_array lpConn;
				*lplpList = lpNew;
				*lpcList = cConn + 1;
			}
			else
				hr = E_OUTOFMEMORY;
		}
		lpNeurone->Release();
	}
	return hr;
}

BOOL CConnList::ClearConnection (LPCLIST* lplpList, INT* lpcList, INT index)
{
	BOOL fSuccess = FALSE;
	if(index >= 0 && index < *lpcList)
	{
		LPCLIST lpConn = *lplpList;
		INT cConn = *lpcList;

		if(cConn - 1 > 0)
		{
			LPCLIST lpNew = __new CLIST[cConn - 1];
			if(lpNew)
			{
				lpConn[index].lpTarget->Release();
				CopyMemory(lpNew,lpConn,index * sizeof(CLIST));
				CopyMemory(lpNew + index,lpConn + index + 1,(cConn - (index + 1)) * sizeof(CLIST));
				__delete_array lpConn;
				*lplpList = lpNew;
				*lpcList = cConn - 1;
				fSuccess = TRUE;
			}
		}
		else
		{
			lpConn[index].lpTarget->Release();
			__delete_array lpConn;
			*lplpList = NULL;
			*lpcList = 0;
			fSuccess = TRUE;
		}
	}
	return fSuccess;
}
