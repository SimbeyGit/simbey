#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Core\StringCore.h"
#include "NeuralAPI.h"
#include "NeuralLinks.h"
#include "BaseIONeurone.h"

CBaseIONeurone::CBaseIONeurone ()
{
	m_iPin = 0;
	m_lpParent = NULL;
	m_lpLink = NULL;
}

CBaseIONeurone::~CBaseIONeurone ()
{
	Assert(NULL == m_lpLink);

	if(m_lpParent)
		m_lpParent->Release();
}

// IUnknown

HRESULT WINAPI CBaseIONeurone::QueryInterface (REFIID iid, LPVOID* lplpvObject)
{
	HRESULT hr = E_INVALIDARG;
	if(lplpvObject)
	{
		if(iid == __uuidof(IIONeurone))
			*lplpvObject = (IIONeurone*)this;
		else
		{
			hr = CNeurone::QueryInterface(iid,lplpvObject);
			goto exit;
		}
		AddRef();
		hr = S_OK;
	}
exit:
	return hr;
}

ULONG WINAPI CBaseIONeurone::AddRef (VOID)
{
	return CNeurone::AddRef();
}

ULONG WINAPI CBaseIONeurone::Release (VOID)
{
	return CNeurone::Release();
}

// INetCycleProcessor

VOID CBaseIONeurone::SendPulses (VOID)
{
}

VOID CBaseIONeurone::CheckThresholds (VOID)
{
}

// INetObject

HRESULT CBaseIONeurone::Load (INeuralFactory* pFactory, LPNLOADDATA lpLoadData, DWORD cLoadData, LPBYTE lpData, ULONG cbData)
{
	HRESULT hr = E_FAIL;

	if(cbData > sizeof(ULONG))
	{
		CopyMemory(&m_iPin,lpData,sizeof(ULONG));
		cbData -= sizeof(ULONG);
		lpData += sizeof(ULONG);
		hr = S_OK;
	}

	if(SUCCEEDED(hr))
	{
		if(cbData > sizeof(BYTE))
		{
			BYTE cchLink;

			CopyMemory(&cchLink, lpData, sizeof(BYTE));
			cbData -= sizeof(BYTE);
			lpData += sizeof(BYTE);

			if(0 < cchLink)
			{
				if(cbData > cchLink)
				{
					CHAR szLink[256];
					CopyMemory(szLink, lpData, cchLink * sizeof(BYTE));
					szLink[cchLink] = '\0';

					hr = SetLinkName(lpLoadData->lpLinks, szLink);
					if(SUCCEEDED(hr))
					{
						lpData += cchLink;
						cbData -= cchLink;
					}
				}
				else
					hr = E_FAIL;
			}
		}
		else
			hr = E_FAIL;
	}

	if(SUCCEEDED(hr))
		hr = CNeurone::Load(pFactory,lpLoadData,cLoadData,lpData,cbData);

	return hr;
}

HRESULT CBaseIONeurone::Save (INeuralNet* lpNet, ISequentialStream* lpStream)
{
	ULONG cbWritten;
	HRESULT hr = lpStream->Write(&m_iPin,sizeof(ULONG),&cbWritten);
	if(SUCCEEDED(hr))
	{
		BYTE cchName = 0;
		if(m_lpLink)
		{
			CHAR szName[256];
			hr = m_lpLink->GetLinkName(szName, ARRAYSIZE(szName));
			if(SUCCEEDED(hr))
			{
				cchName = (BYTE)lstrlen(szName);
				hr = lpStream->Write(&cchName, sizeof(BYTE), &cbWritten);
				if(SUCCEEDED(hr) && 0 < cchName)
					hr = lpStream->Write(szName, cchName * sizeof(BYTE), &cbWritten);
			}
		}
		else
			hr = lpStream->Write(&cchName, sizeof(BYTE), &cbWritten);
	}
	if(SUCCEEDED(hr))
		hr = CNeurone::Save(lpNet,lpStream);
	return hr;
}

// INetDocObject

VOID CBaseIONeurone::NotifyRemovalOf (INetDocObject* lpObject)
{
	if(this == lpObject && m_lpLink)
	{
		m_lpLink->Unregister(this);
		SafeRelease(m_lpLink);
	}

	__super::NotifyRemovalOf(lpObject);
}

// IIONeurone

VOID CBaseIONeurone::AttachParentChip (INeuralChip* lpParent)
{
	ReplaceInterface(m_lpParent, lpParent);
}

BOOL CBaseIONeurone::HasParentChip (VOID)
{
	return (NULL != m_lpParent);
}

HRESULT CBaseIONeurone::GetLinkName (LPSTR lpszName, INT cchMaxName)
{
	HRESULT hr;

	if(m_lpLink)
		hr = m_lpLink->GetLinkName(lpszName, cchMaxName);
	else
		hr = TStrCchCpy(lpszName, cchMaxName, DEFAULT_SOURCE_IO_NAME);

	return hr;
}

HRESULT CBaseIONeurone::SetLinkName (INeuralLinks* lpLinks, LPCSTR lpcszName)
{
	HRESULT hr;

	if(lpcszName)
	{
		INeuralLink* lpLink;
		hr = lpLinks->GetNeuralLink(lpcszName, &lpLink);
		if(SUCCEEDED(hr))
			hr = lpLink->Register(this);
		if(SUCCEEDED(hr))
		{
			if(m_lpLink)
				m_lpLink->Unregister(this);
			ReplaceInterface(m_lpLink, lpLink);
		}
		SafeRelease(lpLink);
	}
	else
	{
		if(m_lpLink)
		{
			m_lpLink->Unregister(this);
			m_lpLink->Release();
			m_lpLink = NULL;
		}
		hr = S_OK;
	}
	return hr;
}