#include <windows.h>
#include "Library\Core\Assert.h"
#include "Library\Core\Array.h"
#include "Library\Util\Formatting.h"
#include "Library\Util\FileStream.h"
#include "Library\Core\MemoryStream.h"
#include "Library\GraphCtrl.h"
#include "NeuralAPI.h"
#include "GdiList.h"
#include "Neurone.h"
#include "BrainBox.h"
#include "NeuralFactory.h"
#include "NeuralChip.h"
#include "NeuralLinks.h"
#include "NeuralNet.h"

CNeuralNet::CNeuralNet (INeuralFactory* pFactory)
{
	m_cRef = 1;

	m_pFactory = pFactory;
	m_pFactory->AddRef();

	m_cList = 0;
	m_lpList = NULL;
	m_lpEnd = NULL;

	m_lpFrames = NULL;
}

CNeuralNet::~CNeuralNet ()
{
	LPNLIST lpTemp, lpList = m_lpList;
	while(lpList)
	{
		NotifyRemovalOf(lpList->lpObject);
		lpList = lpList->Next;
	}
	while(m_lpList)
	{
		lpTemp = m_lpList;
		m_lpList = m_lpList->Next;
		UnloadNeuralChip(lpTemp->lpObject);
		lpTemp->lpObject->Release();
		__delete lpTemp;
	}
	SafeRelease(m_pFactory);
	Assert(NULL == m_lpFrames);
}

// IUnknown

HRESULT WINAPI CNeuralNet::QueryInterface (REFIID iid, LPVOID* lplpvObject)
{
	HRESULT hr = E_INVALIDARG;
	if(lplpvObject)
	{
		if(iid == IID_IUnknown)
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

ULONG WINAPI CNeuralNet::AddRef (VOID)
{
	return (ULONG)InterlockedIncrement((LONG*)&m_cRef);
}

ULONG WINAPI CNeuralNet::Release (VOID)
{
	ULONG c = (ULONG)InterlockedDecrement((LONG*)&m_cRef);
	if(c == 0)
		__delete this;
	return c;
}

// INetCycleProcessor

VOID CNeuralNet::SendPulses (VOID)
{
	LPNLIST lpList = m_lpList;
	while(lpList)
	{
		lpList->lpObject->SendPulses();
		lpList = lpList->Next;
	}
}

VOID CNeuralNet::CheckThresholds (VOID)
{
	LPNLIST lpList = m_lpList;
	while(lpList)
	{
		lpList->lpObject->CheckThresholds();
		lpList = lpList->Next;
	}
}

// INeuralNet

HRESULT CNeuralNet::Load (__in INeuralLinks* lpLinks, LPBYTE* lplpData, DWORD* lpcbData)
{
	HRESULT hr = S_FALSE;
	LPBYTE lpData = *lplpData;
	DWORD cbData = *lpcbData;
	DWORD cObjects;

	if(cbData >= sizeof(cObjects))
	{
		CopyMemory(&cObjects,lpData,sizeof(cObjects));
		lpData += sizeof(cObjects);
		cbData -= sizeof(cObjects);

		if(cObjects > 0)
		{
			LPNLOADDATA lpLoad = __new NLOADDATA[cObjects];
			if(lpLoad)
			{
				BYTE cchClass;
				CHAR szClass[256];
				DWORD i = 0;

				ZeroMemory(lpLoad,sizeof(NLOADDATA) * cObjects);
				lpLoad->lpLinks = lpLinks;

				while(i < cObjects)
				{
					if(cbData < sizeof(BYTE))
					{
						hr = E_FAIL;
						break;
					}
					cchClass = lpData[0];
					lpData++;
					cbData--;

					if(cchClass == 0 || cbData < (DWORD)cchClass)
					{
						hr = E_FAIL;
						break;
					}
					CopyMemory(szClass,lpData,cchClass);
					szClass[cchClass] = 0;
					lpData += cchClass;
					cbData -= cchClass;

					if(cbData < sizeof(DWORD))
					{
						hr = E_FAIL;
						break;
					}
					CopyMemory(&lpLoad[i].cbData,lpData,sizeof(DWORD));
					lpData += sizeof(DWORD);
					cbData -= sizeof(DWORD);

					if(cbData < lpLoad[i].cbData)
					{
						hr = E_FAIL;
						break;
					}
					lpLoad[i].lpData = lpData;
					lpData += lpLoad[i].cbData;
					cbData -= lpLoad[i].cbData;

					// Pass NULL to the factory because all settings will be
					// loaded from storage.
					hr = m_pFactory->Create(NULL, szClass, &lpLoad[i].lpObject);
					if(FAILED(hr))
						break;
					hr = AttachObject(lpLoad[i].lpObject);
					if(FAILED(hr))
						break;

					i++;
				}

				if(SUCCEEDED(hr))
				{
					for(i = 0; i < cObjects; i++)
					{
						hr = lpLoad[i].lpObject->Load(m_pFactory, lpLoad,cObjects,lpLoad[i].lpData,lpLoad[i].cbData);
						if(FAILED(hr))
							break;
					}
				}

				for(i = 0; i < cObjects; i++)
				{
					if(lpLoad[i].lpObject)
						lpLoad[i].lpObject->Release();
				}
				__delete_array lpLoad;
			}
			else
				hr = E_OUTOFMEMORY;
		}

		*lplpData = lpData;
		*lpcbData = cbData;
	}
	else
		hr = E_FAIL;
	return hr;
}

HRESULT CNeuralNet::LoadBrainBox (LPBYTE* lplpData, DWORD* lpcbData, BOOL fUseBiasNeuronesForBrainBox)
{
	CBrainBox bbLoader(this);
	return bbLoader.Load(*lplpData, *lpcbData, fUseBiasNeuronesForBrainBox);
}

HRESULT CNeuralNet::Save (ISequentialStream* lpStream)
{
	HRESULT hr;
	DWORD cbWritten;

	Check(lpStream->Write(&m_cList, sizeof(m_cList), &cbWritten));
	if(0 < m_cList)
	{
		CMemoryStream Mem;
		CHAR szClass[256];
		LPNLIST lpList = m_lpList;

		while(lpList)
		{
			INT cchClass;
			DWORD cbObject;
			BYTE cchByteClass;
			const BYTE* lpReadPtr;

			Check(lpList->lpObject->GetObjectClass(szClass, ARRAYSIZE(szClass), &cchClass));
			CheckIf(255 < cchClass, E_FAIL);
			cchByteClass = (BYTE)cchClass;

			Check(lpStream->Write(&cchByteClass, sizeof(BYTE), &cbWritten));
			Check(lpStream->Write(szClass, cchClass, &cbWritten));

			Mem.Reset();
			Check(lpList->lpObject->Save(this, &Mem));

			cbObject = Mem.DataRemaining();
			Check(lpStream->Write(&cbObject, sizeof(cbObject), &cbWritten));

			lpReadPtr = Mem.GetReadPtr();
			Check(lpStream->Write(lpReadPtr, cbObject, &cbWritten));

			lpList = lpList->Next;
		}
	}

Cleanup:
	return hr;
}

VOID CNeuralNet::ResetNet (VOID)
{
}

INetDocObject* CNeuralNet::FindObject (INT x, INT y)
{
	INetDocObject* lpObject = NULL;
	LPNLIST lpList = m_lpEnd;
	while(lpList)
	{
		if(lpList->lpObject->HitTest(x,y) != HITTEST_NONE)
		{
			lpObject = lpList->lpObject;
			break;
		}
		lpList = lpList->Prev;
	}
	return lpObject;
}

INetDocObject* CNeuralNet::GetObject (INT index)
{
	INetDocObject* lpObject = NULL;
	INT i = 0;
	LPNLIST lpList = m_lpList;
	while(lpList)
	{
		if(i == index)
		{
			lpObject = lpList->lpObject;
			break;
		}
		lpList = lpList->Next;
		i++;
	}
	return lpObject;
}

BOOL CNeuralNet::GetFrame (INT x, INT y, INeuralFrame** lplpFrame)
{
	BOOL fFound = FALSE;
	INT nHitTest;
	LPNLIST lpList = m_lpFrames;
	while(lpList)
	{
		nHitTest = lpList->lpObject->HitTest(x,y);
		if(nHitTest & HITTEST_DRAG_FRAME)
		{
			if(SUCCEEDED(lpList->lpObject->QueryInterface(lplpFrame)))
			{
				fFound = TRUE;
				break;
			}
		}
		lpList = lpList->Next;
	}
	return fFound;
}

BOOL CNeuralNet::GetFrame (INetDocObject* lpObject, INeuralFrame** lplpFrame)
{
	BOOL fFound = FALSE;
	LPNLIST lpList = m_lpFrames;
	while(lpList)
	{
		if(SUCCEEDED(lpList->lpObject->QueryInterface(lplpFrame)))
		{
			if((*lplpFrame)->ContainsObject(lpObject))
			{
				fFound = TRUE;
				break;
			}
			(*lplpFrame)->Release();
			*lplpFrame = NULL;
		}
		lpList = lpList->Next;
	}
	return fFound;
}

INT CNeuralNet::GetObjectCount (VOID)
{
	return m_cList;
}

INT CNeuralNet::GetObjectIndex (INetDocObject* lpObject)
{
	INT n = -1, i = 0;
	LPNLIST lpList = m_lpList;
	while(lpList)
	{
		if(lpList->lpObject == lpObject)
		{
			n = i;
			break;
		}
		lpList = lpList->Next;
		i++;
	}
	return n;
}

HRESULT CNeuralNet::AttachObject (INetDocObject* lpObject)
{
	HRESULT hr = E_OUTOFMEMORY;
	LPNLIST lpNew = __new NLIST;
	if(lpNew)
	{
		INeuralFrame* lpFrame;
		LPNLIST lpList = m_lpList, lpPrev = NULL;
		INT zOrder = lpObject->GetZOrder();

		lpNew->lpObject = lpObject;
		lpNew->lpObject->AddRef();

		while(lpList)
		{
			if(lpList->lpObject->GetZOrder() > zOrder)
				break;
			lpPrev = lpList;
			lpList = lpList->Next;
		}

		lpNew->Prev = lpPrev;
		if(lpPrev)
		{
			lpNew->Next = lpList;
			lpPrev->Next = lpNew;
		}
		else
		{
			lpNew->Next = m_lpList;
			m_lpList = lpNew;
		}
		if(lpList)
			lpList->Prev = lpNew;
		else
			m_lpEnd = lpNew;

		m_cList++;

		if(SUCCEEDED(lpObject->QueryInterface(&lpFrame)))
		{
			AddFrame(lpFrame);
			lpFrame->Release();
		}

		hr = S_OK;
	}
	return hr;
}

HRESULT CNeuralNet::RemoveObject (INetDocObject* lpObject)
{
	HRESULT hr = S_FALSE;
	LPNLIST lpList = m_lpList, lpNext;
	while(lpList)
	{
		lpNext = lpList->Next;
		if(lpList->lpObject == lpObject)
		{
			if(lpList->Prev)
				lpList->Prev->Next = lpNext;
			else
				m_lpList = lpNext;
			if(lpNext)
				lpNext->Prev = lpList->Prev;
			else
				m_lpEnd = lpList->Prev;

			__delete lpList;
			m_cList--;

			NotifyRemovalOf(lpObject);
			UnloadNeuralChip(lpObject);
			lpObject->Release();

			hr = S_OK;
			break;
		}
		lpList = lpNext;
	}
	return hr;
}

VOID CNeuralNet::Draw (IGrapher* lpGraph, BOOL fDrawLabels)
{
	LPNLIST lpList = m_lpList;
	while(lpList)
	{
		lpList->lpObject->DrawBackground(lpGraph);
		lpList = lpList->Next;
	}

	lpList = m_lpList;
	while(lpList)
	{
		lpList->lpObject->DrawForeground(lpGraph);
		lpList = lpList->Next;
	}

	if(fDrawLabels)
	{
		INeurone* lpNeurone;
		HPEN hpnDefPen = lpGraph->SelectPen(GdiList::hpnBorder);
		HBRUSH hbrDefBrush = lpGraph->SelectBrush(GdiList::hbrRest);
		HFONT hfDefFont = lpGraph->SelectFont(GdiList::hfLabels);

		lpList = m_lpList;
		while(lpList)
		{
			if(SUCCEEDED(lpList->lpObject->QueryInterface(&lpNeurone)))
			{
				FLOAT fValue;
				if(lpNeurone->GetCurrentValue(fValue))
				{
					CHAR szValue[32];
					INT x, y, cchValue;
					Formatting::TFloatToString(fValue, szValue, ARRAYSIZE(szValue), 4, &cchValue);
					lpNeurone->GetPosition(x,y);
					lpGraph->LabelTextAbove((FLOAT)(x - 4),(FLOAT)(y + 4),0.0f,szValue,cchValue);
				}
				lpNeurone->Release();
			}
			lpList = lpList->Next;
		}

		lpGraph->SelectFont(hfDefFont);
		lpGraph->SelectBrush(hbrDefBrush);
		lpGraph->SelectPen(hpnDefPen);
	}
}

ULONG CNeuralNet::GetUniquePin (EIO_TYPE eType)
{
	ULONG iPin = 1, nTestPin;
	LPNLIST lpList = m_lpList;
	IIONeurone* lpIO;
	TSortedArray<ULONG> aPins;
	INT c;

	while(lpList)
	{
		if(SUCCEEDED(lpList->lpObject->QueryInterface(&lpIO)))
		{
			if(lpIO->GetIOType() == eType)
			{
				ULONG n = lpIO->GetPin();
				aPins.InsertSorted(&n);
			}
			lpIO->Release();
		}
		lpList = lpList->Next;
	}

	c = aPins.Length();
	for(INT i = 0; i < c; i++)
	{
		nTestPin = aPins[i];
		if(nTestPin > iPin)
			break;
		iPin = nTestPin + 1;
	}

	return iPin;
}

LPNLIST CNeuralNet::GetObjects (VOID)
{
	return m_lpList;
}

HRESULT CNeuralNet::GetObjectsInRange (FLOAT xLeft, FLOAT yTop, FLOAT xRight, FLOAT yBottom, INetDocObject*** lplplpObjects, INT* lpcObjects)
{
	HRESULT hr = S_FALSE;
	CMemoryStream List;
	INeurone* lpNeurone;
	LPNLIST lpList = m_lpList;
	ULONG cb;

	while(lpList)
	{
		if(SUCCEEDED(lpList->lpObject->QueryInterface(&lpNeurone)))
		{
			INT x, y;
			lpNeurone->GetPosition(x,y);
			if(x >= xLeft && x < xRight)
			{
				if(y > yBottom && y <= yTop)
				{
					hr = List.Write(&lpList->lpObject,sizeof(INetDocObject*),&cb);
					if(FAILED(hr))
						break;
				}
			}
			lpNeurone->Release();
		}
		lpList = lpList->Next;
	}

	if(S_OK == hr)
	{
		INetDocObject** lplpList = (INetDocObject**)List.Detach(&cb);
		*lpcObjects = cb / sizeof(INetDocObject*);
		for(INT i = 0; i < *lpcObjects; i++)
			lplpList[i]->AddRef();
		*lplplpObjects = lplpList;
	}
	else if(SUCCEEDED(hr))
	{
		*lpcObjects = 0;
		*lplplpObjects = NULL;
	}

	return hr;
}

HRESULT CNeuralNet::LoadFromFile (__in INeuralNet* lpNet, __in INeuralLinks* lpLinks, PCSTR pcszFile, INT cchFile, BOOL fUseBiasNeuronesForBrainBox)
{
	HRESULT hr = E_INVALIDARG;
	if(lpNet && pcszFile && *pcszFile)
	{
		HANDLE hFile = CreateFile(pcszFile,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
		if(hFile != INVALID_HANDLE_VALUE)
		{
			DWORD dwSizeHigh = 0;
			DWORD dwSize = GetFileSize(hFile,&dwSizeHigh);
			if(dwSize > 0 && dwSizeHigh == 0)
			{
				LPBYTE lpData = __new BYTE[dwSize];
				if(lpData)
				{
					DWORD dwTemp;
					if(ReadFile(hFile,lpData,dwSize,&dwTemp,NULL) && dwTemp == dwSize)
					{
						LPBYTE lpStream = lpData;
						DWORD dwStream = dwSize;

						if(3 < cchFile && 0 == lstrcmpi(pcszFile + cchFile - 4, ".bbx"))
							hr = lpNet->LoadBrainBox(&lpStream, &dwStream, fUseBiasNeuronesForBrainBox);
						else
						{
							hr = lpNet->Load(lpLinks, &lpStream, &dwStream);
							if(SUCCEEDED(hr))
								hr = lpLinks->Load(&lpStream, &dwStream);
						}
					}
					else
						hr = HRESULT_FROM_WIN32(GetLastError());
					__delete_array lpData;
				}
				else
					hr = E_OUTOFMEMORY;
			}
			else
				hr = E_FAIL;
			CloseHandle(hFile);
		}
		else
			hr = HRESULT_FROM_WIN32(GetLastError());
	}
	return hr;
}

HRESULT CNeuralNet::SaveToFile (__in INeuralNet* lpNet, __in INeuralLinks* lpLinks, PCSTR pcszFile)
{
	HRESULT hr = E_INVALIDARG;
	if(pcszFile && *pcszFile)
	{
		HANDLE hFile = CreateFile(pcszFile,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
		if(hFile != INVALID_HANDLE_VALUE)
		{
			CFileStream Stream(hFile);
			hr = lpNet->Save(&Stream);
			if(SUCCEEDED(hr))
				hr = lpLinks->Save(&Stream);
			CloseHandle(hFile);
		}
		else
			hr = HRESULT_FROM_WIN32(GetLastError());
	}
	return hr;
}

//

HRESULT CNeuralNet::AddFrame (INeuralFrame* lpFrame)
{
	HRESULT hr;
	LPNLIST lpNew = __new NLIST;
	if(lpNew)
	{
		lpNew->lpObject = lpFrame;	// Don't take a reference.
		lpNew->Next = m_lpFrames;
		m_lpFrames = lpNew;
		hr = S_OK;
	}
	else
		hr = E_OUTOFMEMORY;
	return hr;
}

VOID CNeuralNet::CheckFrameUnload (INetDocObject* lpObject)
{
	LPNLIST lpPrev = NULL, lpList = m_lpFrames;
	while(lpList)
	{
		if(lpList->lpObject == lpObject)
		{
			// It's not reference counted.
			if(lpPrev)
				lpPrev->Next = lpList->Next;
			else
				m_lpFrames = lpList->Next;
			__delete lpList;
			break;
		}
		lpPrev = lpList;
		lpList = lpList->Next;
	}
}

VOID CNeuralNet::NotifyRemovalOf (INetDocObject* lpObject)
{
	LPNLIST lpList = m_lpList;
	while(lpList)
	{
		// It's intentional that even the object being removed is notified of its own removal.
		lpList->lpObject->NotifyRemovalOf(lpObject);
		lpList = lpList->Next;
	}
	CheckFrameUnload(lpObject);
	lpObject->UnloadAccessibility();
}

VOID CNeuralNet::UnloadNeuralChip (INetDocObject* lpObject)
{
	INeuralChip* lpChip;
	if(SUCCEEDED(lpObject->QueryInterface(&lpChip)))
	{
		lpChip->UnloadEmbeddedNet();
		lpChip->Release();
	}
}
