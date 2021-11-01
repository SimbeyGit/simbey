#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Core\MemoryStream.h"
#include "Library\Util\Formatting.h"
#include "NeuralAPI.h"
#include "Extensibility.h"
#include "NeuralLinks.h"

//
// CNeuralLink
//

CNeuralLink::CNeuralLink (CNeuralLinks* lpLinks)
{
	m_cRef = 1;

	m_lpcszName = NULL;

	m_lpLinks = lpLinks;
	m_lpSource = NULL;
	m_pvSourceData = NULL;
}

CNeuralLink::~CNeuralLink ()
{
	Assert(NULL == m_lpSource);
}

VOID CNeuralLink::SetLinkName (LPCSTR lpcszName)
{
	m_lpcszName = lpcszName;
}

VOID CNeuralLink::SetSource (__in INeuralSource* lpSource)
{
	Assert(lpSource);

	m_lpSource = lpSource;
	m_lpSource->AddRef();
}

VOID CNeuralLink::SendPulses (VOID)
{
	m_lpSource->SendPulses(this);
}

// IUnknown

HRESULT WINAPI CNeuralLink::QueryInterface (REFIID iid, LPVOID* lplpvObject)
{
	UNREFERENCED_PARAMETER(iid);
	UNREFERENCED_PARAMETER(lplpvObject);

	return E_NOTIMPL;
}

ULONG WINAPI CNeuralLink::AddRef (VOID)
{
	return (ULONG)InterlockedIncrement((LONG*)&m_cRef);
}

ULONG WINAPI CNeuralLink::Release (VOID)
{
	ULONG c = (ULONG)InterlockedDecrement((LONG*)&m_cRef);
	if(c == 0)
		__delete this;
	return c;
}

// INeuralLink

HRESULT CNeuralLink::Register (IIONeurone* lpIO)
{
	HRESULT hr = m_aIO.InsertSorted(&lpIO);
	if(SUCCEEDED(hr))
		lpIO->AddRef();
	return hr;
}

HRESULT CNeuralLink::Unregister (IIONeurone* lpIO)
{
	HRESULT hr = S_FALSE;
	INT nPosition;
	if(m_aIO.Find(lpIO, &nPosition))
	{
		m_aIO.Delete(nPosition);
		lpIO->Release();
		hr = S_OK;
	}
	return hr;
}

VOID CNeuralLink::OutputValue (FLOAT fValue, ULONG iPin)
{
	if(m_lpSource)
		m_lpSource->ReceiveOutputValue(this, fValue, iPin);
}

VOID CNeuralLink::InputValue (FLOAT fValue, ULONG iPin)
{
	IIONeurone** ppIO;
	INT cIO;
	m_aIO.GetData(&ppIO, &cIO);
	for(INT i = 0; i < cIO; i++)
		ppIO[i]->ReceiveValue(fValue, iPin);
}

HRESULT CNeuralLink::GetLinkName (LPSTR lpszName, INT cchMaxName)
{
	return TStrCchCpy(lpszName, cchMaxName, m_lpcszName);
}

HRESULT CNeuralLink::GetLinkSource (__deref_out INeuralSource** lplpSource)
{
	HRESULT hr = E_POINTER;
	if(lplpSource)
	{
		*lplpSource = m_lpSource;
		(*lplpSource)->AddRef();
		hr = S_OK;
	}
	return hr;
}

VOID CNeuralLink::SetSourceData (PVOID pvData)
{
	m_pvSourceData = pvData;
}

PVOID CNeuralLink::GetSourceData (VOID)
{
	return m_pvSourceData;
}

HRESULT CNeuralLink::Remove (VOID)
{
	HRESULT hr = E_FAIL;
	if(0 == m_aIO.Length())
	{
		UnregisterAndDetachSource();
		hr = m_lpLinks->Unlink(this);
	}
	return hr;
}

VOID CNeuralLink::UnregisterAndDetachSource (VOID)
{
	if(m_lpSource)
	{
		m_lpSource->DetachLink(this);
		m_lpSource->Release();
		m_lpSource = NULL;
	}
}

//
// CNeuralLinks
//

CNeuralLinks::CNeuralLinks ()
{
}

CNeuralLinks::~CNeuralLinks ()
{
	Assert(0 == m_mapLinks.Length());
	Assert(0 == m_mapSources.Length());
}

HRESULT CNeuralLinks::Initialize (VOID)
{
	INeuralSource** lplpSources;
	INT cSources;
	HRESULT hr = ExtGetLinkSources(&lplpSources, &cSources);
	if(SUCCEEDED(hr))
	{
		CHAR szName[256];

		Assert(0 == m_mapSources.Length());

		// Transfer reference ownership to the map.
		for(INT i = 0; i < cSources; i++)
		{
			hr = lplpSources[i]->GetSourceName(szName, ARRAYSIZE(szName));
			if(FAILED(hr))
				break;
			hr = m_mapSources.Add(szName, lplpSources[i]);
			if(FAILED(hr))
				break;
		}

		if(FAILED(hr))
		{
			// If there was a problem, clear the map and the references.
			m_mapSources.Clear();
			for(INT i = 0; i < cSources; i++)
				lplpSources[i]->Release();
		}

		__delete_array lplpSources;
	}
	return hr;
}

VOID CNeuralLinks::Unload (VOID)
{
	INT c;

	while(m_mapLinks.Length())
	{
		CNeuralLink** lplpLink = m_mapLinks.GetValuePtr(0);
		(*lplpLink)->Remove();
	}
	Assert(0 == m_mapLinks.Length());

	c = m_mapSources.Length();
	for(INT i = 0; i < c; i++)
	{
		INeuralSource** lplpSource = m_mapSources.GetValuePtr(i);
		(*lplpSource)->Release();
	}
	m_mapSources.Clear();
}

VOID CNeuralLinks::EditLink (HWND hwndParent, LPCSTR szName)
{
	CNeuralLink* pLink;
	if(SUCCEEDED(m_mapLinks.Find(szName, &pLink)))
	{
		INeuralSource* pSource;
		if(SUCCEEDED(pLink->GetLinkSource(&pSource)))
		{
			pSource->EditLinkProperties(hwndParent, pLink);
			pSource->Release();
		}
	}
}

VOID CNeuralLinks::FillLinksList (HWND hwndBox, LPCSTR lpcszSelected, BOOL fIsComboBox)
{
	INT nAddMsg, nSetCurSelMsg;
	INT cLinks = m_mapLinks.Length();
	bool fFoundSelection = false;

	if(fIsComboBox)
	{
		nAddMsg = CB_ADDSTRING;
		nSetCurSelMsg = CB_SETCURSEL;
	}
	else
	{
		nAddMsg = LB_ADDSTRING;
		nSetCurSelMsg = LB_SETCURSEL;
	}

	INT nPos = (INT)SendMessage(hwndBox, nAddMsg, 0, (LPARAM)DEFAULT_SOURCE_IO_NAME);
	if(0 == lstrcmp(DEFAULT_SOURCE_IO_NAME, lpcszSelected))
	{
		SendMessage(hwndBox, nSetCurSelMsg, nPos, 0);
		fFoundSelection = true;
	}

	for(INT i = 0; i < cLinks; i++)
	{
		CNeuralLink** lplpLink = m_mapLinks.GetValuePtr(i);
		CHAR szName[256];

		if(SUCCEEDED((*lplpLink)->GetLinkName(szName, ARRAYSIZE(szName))))
		{
			nPos = (INT)SendMessage(hwndBox, nAddMsg, 0, (LPARAM)szName);
			if(!fFoundSelection && 0 == lstrcmp(szName, lpcszSelected))
			{
				SendMessage(hwndBox, nSetCurSelMsg, nPos, 0);
				fFoundSelection = true;
			}
		}
	}
}

VOID CNeuralLinks::FillSources (HWND hwndBox)
{
	INT cSources = m_mapSources.Length();
	for(INT i = 0; i < cSources; i++)
	{
		INeuralSource** lplpSource = m_mapSources.GetValuePtr(i);
		CHAR szName[256];

		if(SUCCEEDED((*lplpSource)->GetSourceName(szName, ARRAYSIZE(szName))))
			SendMessage(hwndBox, LB_ADDSTRING, 0, (LPARAM)szName);
	}
}

HRESULT CNeuralLinks::GetNeuralLink (LPCSTR lpcszName, __deref_out INeuralLink** ppLink)
{
	HRESULT hr;
	CNeuralLink* pLink;

	hr = m_mapLinks.Find(lpcszName, &pLink);
	if(FAILED(hr))
	{
		pLink = __new CNeuralLink(this);
		if(pLink)
		{
			hr = m_mapLinks.Add(lpcszName, pLink);
			if(SUCCEEDED(hr))
			{
				pLink->SetLinkName(m_mapLinks.GetKeyPtr(lpcszName));
				BindLinkWithSource(pLink, lpcszName);
			}
			else
				pLink->Release();
		}
		else
			hr = E_OUTOFMEMORY;
	}

	if(SUCCEEDED(hr))
	{
		*ppLink = pLink;
		(*ppLink)->AddRef();
	}

	return hr;
}

HRESULT CNeuralLinks::CreateLink (__in_opt HWND hwndParent, LPCSTR lpcszSource, LPCSTR lpcszName)
{
	INeuralSource* lpSource;
	HRESULT hr = m_mapSources.Find(lpcszSource, &lpSource);
	if(SUCCEEDED(hr))
	{
		CHAR szLink[256];
		hr = Formatting::TPrintF(szLink, ARRAYSIZE(szLink), NULL, "%s (%s)", lpcszName, lpcszSource);
		if(SUCCEEDED(hr))
		{
			INeuralLink* lpLink;
			hr = GetNeuralLink(szLink, &lpLink);
			if(SUCCEEDED(hr))
			{
				if(hwndParent)
					lpSource->EditLinkProperties(hwndParent, lpLink);
				lpLink->Release();
			}
		}
	}
	return hr;
}

HRESULT CNeuralLinks::Load (LPBYTE* lplpData, DWORD* lpcbData)
{
	HRESULT hr;

	if(0 == *lpcbData)
		hr = S_OK;
	else if(sizeof(DWORD) <= *lpcbData)
	{
		LPBYTE lpData = *lplpData;
		DWORD cbData = *lpcbData;
		DWORD cNames;

		hr = S_OK;

		CopyMemory(&cNames, lpData, sizeof(cNames));
		lpData += sizeof(cNames);
		cbData -= sizeof(cNames);

		for(DWORD i = 0; i < cNames; i++)
		{
			INeuralLink* lpLink;
			INeuralSource* lpSource;
			CHAR szName[256];
			BYTE cchName;
			DWORD cbLink;

			if(sizeof(BYTE) > cbData)
			{
				hr = E_FAIL;
				break;
			}
			CopyMemory(&cchName, lpData, sizeof(cchName));
			lpData += sizeof(cchName);
			cbData -= sizeof(cchName);

			if(cchName > cbData)
			{
				hr = E_FAIL;
				break;
			}
			CopyMemory(szName, lpData, cchName);
			szName[cchName] = '\0';
			lpData += cchName;
			cbData -= cchName;

			if(cbData < sizeof(cbLink))
			{
				hr = E_FAIL;
				break;
			}
			CopyMemory(&cbLink, lpData, sizeof(cbLink));
			lpData += sizeof(cbLink);
			cbData -= sizeof(cbLink);

			if(cbLink > cbData)
			{
				hr = E_FAIL;
				break;
			}

			hr = GetNeuralLink(szName, &lpLink);
			if(FAILED(hr))
				break;
			hr = lpLink->GetLinkSource(&lpSource);
			if(SUCCEEDED(hr))
			{
				hr = lpSource->LoadLinkData(lpLink, lpData, cbLink);
				lpSource->Release();
			}
			lpLink->Release();
			if(FAILED(hr))
				break;

			lpData += cbLink;
			cbData -= cbLink;
		}

		if(SUCCEEDED(hr))
		{
			*lplpData = lpData;
			*lpcbData = cbData;
		}
	}
	else
		hr = E_FAIL;

	return hr;
}

HRESULT CNeuralLinks::Save (ISequentialStream* lpStream)
{
	DWORD cb;
	DWORD cNames = m_mapLinks.Length();
	HRESULT hr = lpStream->Write(&cNames, sizeof(cNames), &cb);
	if(SUCCEEDED(hr) && 0 < cNames)
	{
		for(DWORD i = 0; i < cNames; i++)
		{
			ULONG cb;
			CHAR szName[256];
			BYTE cchName;
			CMemoryStream Stream;
			INeuralSource* lpSource;
			CNeuralLink** lplpLink = m_mapLinks.GetValuePtr(i);

			hr = (*lplpLink)->GetLinkName(szName, ARRAYSIZE(szName));
			if(FAILED(hr))
				break;

			cchName = (BYTE)lstrlen(szName);
			hr = lpStream->Write(&cchName, sizeof(cchName), &cb);
			if(FAILED(hr))
				break;

			hr = lpStream->Write(szName, cchName, &cb);
			if(FAILED(hr))
				break;

			hr = (*lplpLink)->GetLinkSource(&lpSource);
			if(FAILED(hr))
				break;

			hr = lpSource->SaveLinkData(*lplpLink, &Stream);
			if(SUCCEEDED(hr))
			{
				ULONG cbLink = Stream.DataRemaining();
				hr = lpStream->Write(&cbLink, sizeof(cbLink), &cb);
				if(SUCCEEDED(hr) && 0 < cbLink)
					hr = lpStream->Write(Stream.GetReadPtr(), cbLink, &cb);
			}
			lpSource->Release();
			if(FAILED(hr))
				break;
		}
	}
	return hr;
}

VOID CNeuralLinks::SendPulses (VOID)
{
	CNeuralLink** lplpLink;
	for(INT i = 0; i < m_mapLinks.Length(); i++)
	{
		lplpLink = m_mapLinks.GetValuePtr(i);
		(*lplpLink)->SendPulses();
	}
}

HRESULT CNeuralLinks::Unlink (__in INeuralLink* lpLink)
{
	CHAR szName[256];
	HRESULT hr = lpLink->GetLinkName(szName, ARRAYSIZE(szName));
	if(SUCCEEDED(hr))
	{
		m_mapLinks.Remove(szName, NULL);
		lpLink->Release();
	}
	return hr;
}

HRESULT CNeuralLinks::BindLinkWithSource (CNeuralLink* lpLink, LPCSTR lpcszName)
{
	HRESULT hr = E_FAIL;
	LPCSTR lpcszStart = TStrRChr(lpcszName, '(');
	if(lpcszStart)
	{
		LPCSTR lpcszEnd = TStrChr(++lpcszStart, ')');
		INT cchSource = (INT)(lpcszEnd - lpcszStart);
		LPSTR lpszSource = __new CHAR[cchSource + 1];
		if(lpszSource)
		{
			INeuralSource* lpSource;

			CopyMemory(lpszSource, lpcszStart, cchSource);
			lpszSource[cchSource] = '\0';

			hr = m_mapSources.Find(lpszSource, &lpSource);
			if(SUCCEEDED(hr))
				hr = lpSource->AttachLink(lpLink);
			if(SUCCEEDED(hr))
				lpLink->SetSource(lpSource);

			__delete_array lpszSource;
		}
		else
			hr = E_OUTOFMEMORY;
	}
	return hr;
}