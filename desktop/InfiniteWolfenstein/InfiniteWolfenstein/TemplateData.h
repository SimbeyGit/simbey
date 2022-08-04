#pragma once

#include "Library\Core\RStrMap.h"
#include "Published\JSON.h"

class CSIFPackage;

template <typename TNamespaceClass, typename TBaseDataClass, typename TItemClass>
class TTemplateData
{
protected:
	TRStrMap<TNamespaceClass*> m_mapNamespaces;

public:
	TTemplateData ()
	{
	}

	~TTemplateData ()
	{
		m_mapNamespaces.DeleteAll();
	}

	HRESULT Load (CSIFPackage* pPackage, TBaseDataClass* pBaseData, PCWSTR pcwzDataFile, INT cchDataFile, BOOL fRequired)
	{
		HRESULT hr;
		RSTRING rstrNamespaceW = NULL, rstrTemplateW = NULL;
		TStackRef<IJSONValue> srv;
		TStackRef<IJSONObject> srFile;
		TStackRef<IJSONArray> srData, srTemplates;
		TNamespaceClass* pNamespace = NULL;
		bool fAdded = false;

		hr = pPackage->GetJSONData(pcwzDataFile, cchDataFile, &srv);
		if(FAILED(hr))
		{
			CheckIf(!fRequired, S_FALSE);
			Check(hr);
		}

		Check(srv->GetObject(&srFile));
		srv.Release();

		Check(srFile->FindNonNullValueW(L"namespace", &srv));
		Check(srv->GetString(&rstrNamespaceW));
		srv.Release();

		Check(srFile->FindNonNullValueW(L"data", &srv));
		Check(srv->GetArray(&srData));
		srv.Release();

		if(SUCCEEDED(srFile->FindNonNullValueW(L"templates", &srv)))
		{
			Check(srv->GetArray(&srTemplates));
			srv.Release();
		}

		pNamespace = __new TNamespaceClass(rstrNamespaceW);
		CheckAlloc(pNamespace);

		sysint cData = srData->Count();
		for(sysint i = 0; i < cData; i++)
		{
			TStackRef<IJSONObject> srItem;
			RSTRING rstrNameW = NULL;

			Check(srData->GetObject(i, &srItem));

			if(SUCCEEDED(srItem->FindNonNullValueW(L"template", &srv)))
			{
				TStackRef<IJSONObject> srTemplate, srClone;

				Check(srv->GetString(&rstrTemplateW));
				srv.Release();

				Check(JSONFindArrayObject(srTemplates, RSTRING_CAST(L"name"), rstrTemplateW, &srTemplate, NULL));
				Check(JSONCloneObject(srTemplate, &srv, FALSE));
				Check(srv->GetObject(&srClone));
				srv.Release();

				Check(JSONMergeObject(srClone, srItem));
				srItem = srClone;

				RStrRelease(rstrTemplateW); rstrTemplateW = NULL;
			}

			Check(srItem->FindNonNullValueW(L"name", &srv));
			Check(srv->GetString(&rstrNameW));
			srv.Release();

			hr = pNamespace->Add(pBaseData, rstrNameW, srItem);
			RStrRelease(rstrNameW);
			Check(hr);
		}

		Check(m_mapNamespaces.Add(rstrNamespaceW, pNamespace));
		fAdded = true;

	Cleanup:
		if(!fAdded)
			__delete pNamespace;
		RStrRelease(rstrTemplateW);
		RStrRelease(rstrNamespaceW);
		return hr;
	}

	HRESULT Find (RSTRING rstrKeyW, __deref_out TItemClass** ppItem)
	{
		HRESULT hr;
		RSTRING rstrNamespaceW = NULL, rstrNameW = NULL;
		TNamespaceClass* pNamespace;

		Check(SplitKey(rstrKeyW, &rstrNamespaceW, &rstrNameW));
		Check(m_mapNamespaces.Find(rstrNamespaceW, &pNamespace));
		Check(pNamespace->Find(rstrNameW, ppItem));

	Cleanup:
		RStrRelease(rstrNameW);
		RStrRelease(rstrNamespaceW);
		return hr;
	}

	HRESULT GetItems (__out TArray<TItemClass*>& aItems)
	{
		HRESULT hr = S_FALSE;

		for(sysint i = 0; i < m_mapNamespaces.Length(); i++)
		{
			TNamespaceClass* pNamespace = *m_mapNamespaces.GetValuePtr(i);
			Check(pNamespace->GetItems(aItems));
		}

	Cleanup:
		return hr;
	}
};

template <typename TBaseDataClass, typename TArrayStruct>
HRESULT TParseChanceArray (TBaseDataClass* pBaseData, IJSONObject* pItem, PCWSTR pcwzArrayName, __deref_out TArrayStruct** ppData, __out sysint* pcData, __out INT* pnMaxRandom)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	TStackRef<IJSONArray> srArray;
	RSTRING rstrThemeW = NULL;
	INT nMaxRandom = 0;

	Check(pItem->FindNonNullValueW(pcwzArrayName, &srv));
	Check(srv->GetArray(&srArray));

	*pcData = srArray->Count();
	*ppData = __new TArrayStruct[*pcData];
	CheckAlloc(*ppData);
	ZeroMemory(*ppData, *pcData * sizeof(TArrayStruct));

	for(sysint i = 0; i < *pcData; i++)
	{
		TArrayStruct* pStruct = (*ppData) + i;
		TStackRef<IJSONObject> srItem;
		INT nChance;

		Check(srArray->GetObject(i, &srItem));

		srv.Release();
		Check(srItem->FindNonNullValueW(L"theme", &srv));
		Check(srv->GetString(&rstrThemeW));
		Check(pBaseData->Find(rstrThemeW, &pStruct->pTheme));

		srv.Release();
		Check(srItem->FindNonNullValueW(L"chance", &srv));
		Check(srv->GetInteger(&nChance));

		nMaxRandom += nChance;
		pStruct->nMaxRange = nMaxRandom;

		RStrRelease(rstrThemeW); rstrThemeW = NULL;
	}

	*pnMaxRandom = nMaxRandom;

Cleanup:
	RStrRelease(rstrThemeW);
	return hr;
}

HRESULT SplitKey (RSTRING rstrKeyW, __out RSTRING* prstrNamespaceW, __out RSTRING* prstrNameW);
