#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Core\Map.h"
#include "Library\Util\Registry.h"
#include "Library\Window\BaseWindow.h"
#include "NeuralAPI.h"
#include "NeuralDocument.h"
#include "Extensibility.h"

struct PLUGIN_INSTANCE
{
	HMODULE hPlugin;
	INeuralPlugin* lpPlugin;
};

struct ASYNC_DATA
{
	IMessageHandler* pTarget;
	UINT nMsg;
	WPARAM wParam;
	LPARAM lParam;
};

class CPluginsManager : public INeuralPlugins
{
public:
	static CPluginsManager* m_lpPlugins;

protected:
	CRITICAL_SECTION m_cs;

	IBaseWindow* m_pWindow;
	UINT m_nAsyncMessage;

	CNeuralDocument* m_lpDoc;
	TNamedMapA<PLUGIN_INSTANCE> m_mapPlugins;
	TMap<INT, IOleCommandTarget*> m_mapCustom;
	TMap<UINT, IMessageHandler*> m_mapMessages;

	TArray<ASYNC_DATA> m_aAsyncData;

public:
	CPluginsManager (IBaseWindow* pWindow)
	{
		InitializeCriticalSection(&m_cs);

		SetInterface(m_pWindow, pWindow);
		m_nAsyncMessage = 0;
		m_lpDoc = NULL;
	}

	~CPluginsManager ()
	{
		SafeRelease(m_pWindow);

		DeleteCriticalSection(&m_cs);
	}

	template<typename T>
	static inline HRESULT TGetFeaturesByName (LPCSTR lpcszFeature, __deref_out_ecount(*lpcFeatures) T*** lplplpFeatures, __out INT* lpcFeatures)
	{
		TArray<IUnknown*> aFeatures;
		HRESULT hr = CPluginsManager::m_lpPlugins->GetFeaturesByName(lpcszFeature, &aFeatures);
		if(SUCCEEDED(hr))
		{
			INT cFeatures = aFeatures.Length();
			*lplplpFeatures = __new T*[cFeatures];
			if(*lplplpFeatures)
			{
				INT n = 0;
				T** lplpFeatures = *lplplpFeatures;
				for(INT i = 0; i < cFeatures; i++)
				{
					if(SUCCEEDED(aFeatures[i]->QueryInterface(lplpFeatures + n)))
						n++;
				}
				*lpcFeatures = n;
			}
			else
				hr = E_OUTOFMEMORY;

			for(INT i = 0; i < cFeatures; i++)
				aFeatures[i]->Release();
		}
		return hr;
	}

	HRESULT LoadPlugin (PLUGIN_INSTANCE* lpInstance, LPCSTR lpcszPath)
	{
		HRESULT hr = E_FAIL;
		if(NULL == lpInstance->hPlugin)
		{
			lpInstance->hPlugin = LoadLibrary(lpcszPath);
			if(NULL != lpInstance->hPlugin)
			{
				LPFNGETNEURALPLUGIN lpfnGetNeuralPlugin = (LPFNGETNEURALPLUGIN)GetProcAddress(lpInstance->hPlugin, PROC_GETNEURALPLUGIN);
				if(lpfnGetNeuralPlugin)
					hr = lpfnGetNeuralPlugin(this, &lpInstance->lpPlugin);
				else
					hr = HRESULT_FROM_WIN32(GetLastError());

				if(FAILED(hr))
					FreeLibrary(lpInstance->hPlugin);
			}
			else
				hr = HRESULT_FROM_WIN32(GetLastError());
		}
		return hr;
	}

	VOID Load (VOID)
	{
		HKEY hKey;

		m_pWindow->RegisterUserMessage(&m_nAsyncMessage);

		if(SUCCEEDED(Registry::CreateKey(HKEY_CURRENT_USER, "Software\\Simbey\\NeuralNet\\Plugins", KEY_READ, &hKey)))
		{
			LONG lResult;
			DWORD dwIndex = 0;
			CHAR szName[256], szPath[MAX_PATH];
			DWORD cchName, cchPath;

			for(;;)
			{
				cchName = ARRAYSIZE(szName);
				cchPath = ARRAYSIZE(szPath);
				lResult = RegEnumValue(hKey, dwIndex, szName, &cchName, NULL, NULL, (LPBYTE)&szPath, &cchPath);
				if(ERROR_SUCCESS == lResult && 0 < cchPath)
				{
					PLUGIN_INSTANCE* lpInstance = m_mapPlugins[szName];
					if(lpInstance && NULL == lpInstance->hPlugin)
					{
						if(FAILED(LoadPlugin(lpInstance, szPath)))
							m_mapPlugins.Remove(szName, NULL);
					}
				}
				else if(ERROR_NO_MORE_ITEMS == lResult)
					break;

				dwIndex++;
			}
			RegCloseKey(hKey);

			for(INT i = 0; i < m_mapPlugins.Length(); i++)
			{
				PLUGIN_INSTANCE* lpInstance = m_mapPlugins.GetValuePtr(i);
				if(FAILED(lpInstance->lpPlugin->Initialize()))
				{
					m_mapPlugins.Remove(m_mapPlugins.GetKey(i), NULL);
					i--;
				}
			}
		}
	}

	VOID Unload (VOID)
	{
		for(INT i = 0; i < m_mapPlugins.Length(); i++)
		{
			PLUGIN_INSTANCE* lpInstance = m_mapPlugins.GetValuePtr(i);
			lpInstance->lpPlugin->Unload();
		}

		for(INT i = 0; i < m_mapPlugins.Length(); i++)
		{
			PLUGIN_INSTANCE* lpInstance = m_mapPlugins.GetValuePtr(i);
			SafeRelease(lpInstance->lpPlugin);
			FreeLibrary(lpInstance->hPlugin);
		}
		m_mapPlugins.Clear();

		m_pWindow->UnregisterUserMessage(m_nAsyncMessage);
	}

	VOID SetDocument (CNeuralDocument* lpDoc)
	{
		m_lpDoc = lpDoc;
	}

	VOID QueryMenuCommand (OLECMD* lpCmd)
	{
		IOleCommandTarget* lpTarget;

		if(SUCCEEDED(m_mapCustom.Find((INT)lpCmd->cmdID, &lpTarget)))
			lpTarget->QueryStatus(NULL, 1, lpCmd, NULL);
	}

	HRESULT ExecMenuCommand (DWORD nCmdID, DWORD nCmdExecOpt)
	{
		HRESULT hr = OLECMDERR_E_NOTSUPPORTED;
		IOleCommandTarget* lpTarget;
		
		if(SUCCEEDED(m_mapCustom.Find((INT)nCmdID, &lpTarget)))
			hr = lpTarget->Exec(NULL, nCmdID, nCmdExecOpt, NULL, NULL);

		return hr;
	}

	BOOL CustomMessage (UINT msg, WPARAM wParam, LPARAM lParam, __out LRESULT& lResult)
	{
		BOOL fHandled = FALSE;
		IMessageHandler* lpHandler;

		if(msg == m_nAsyncMessage)
		{
			ASYNC_DATA Data;
			EnterCriticalSection(&m_cs);
			if(SUCCEEDED(m_aAsyncData.RemoveChecked(0, &Data)))
			{
				LeaveCriticalSection(&m_cs);
				Data.pTarget->OnMessage(Data.nMsg, Data.wParam, Data.lParam, lResult);
				Data.pTarget->Release();
			}
			else
				LeaveCriticalSection(&m_cs);
		}
		else if(SUCCEEDED(m_mapMessages.Find(msg, &lpHandler)))
			fHandled = lpHandler->OnMessage(msg, wParam, lParam, lResult);

		return fHandled;
	}

	HRESULT GetFeaturesByName (LPCSTR lpcszFeature, TArray<IUnknown*>* lpaFeatures)
	{
		HRESULT hr = S_OK;
		INT cFeatures;

		for(INT i = 0; i < m_mapPlugins.Length(); i++)
		{
			PLUGIN_INSTANCE* lpInstance = m_mapPlugins.GetValuePtr(i);
			if(SUCCEEDED(lpInstance->lpPlugin->GetMultipleFeatureCount(lpcszFeature, &cFeatures)))
			{
				IUnknown** lplpFeatures = __new IUnknown*[cFeatures];
				if(lplpFeatures)
				{
					hr = lpInstance->lpPlugin->GetMultipleFeatures(lpcszFeature, lplpFeatures, cFeatures);
					if(SUCCEEDED(hr))
					{
						for(INT n = 0; n < cFeatures; n++)
						{
							hr = lpaFeatures->Append(lplpFeatures + n);
							if(FAILED(hr))
							{
								for(INT x = n; x < cFeatures; x++)
									lplpFeatures[x]->Release();
								break;
							}
						}
					}
					__delete_array lplpFeatures;
				}
				else
					hr = E_OUTOFMEMORY;

				if(FAILED(hr))
				{
					for(INT x = 0; x < lpaFeatures->Length(); x++)
						(*lpaFeatures)[x]->Release();
					lpaFeatures->Clear();
					break;
				}
			}
		}

		return hr;
	}

	BOOL ExtendMenu (HMENU hMenu, INT nExtMenuType)
	{
		BOOL fExtended = FALSE;

		for(sysint i = 0; i < m_mapPlugins.Length(); i++)
		{
			PLUGIN_INSTANCE* pInstance = m_mapPlugins.GetValuePtr(i);
			fExtended |= pInstance->lpPlugin->ExtendMenu(hMenu, nExtMenuType);
		}

		return fExtended;
	}

	// INeuralPlugins

	virtual HRESULT GetServiceByName (LPCSTR lpcszName, DWORD dwVersionRequested, __deref_out IUnknown** ppunkService)
	{
		return E_NOTIMPL;
	}

	virtual HRESULT RegisterCustomMenuCmd (__in IOleCommandTarget* lpTarget, __out INT* pnCmd)
	{
		HRESULT hr = E_INVALIDARG;
		if(lpTarget && pnCmd)
		{
			INT nLast = EXT_FIRST_CUSTOM_MENU_CMD;
			INT c = m_mapCustom.Length();
			for(INT i = 0; i < c; i++)
			{
				INT n = m_mapCustom.GetKey(i);
				if(n > nLast)
					break;
				nLast = n + 1;
			}

			hr = m_mapCustom.Add(nLast, lpTarget);
			if(SUCCEEDED(hr))
			{
				lpTarget->AddRef();
				*pnCmd = nLast;
			}
		}
		return hr;
	}

	virtual HRESULT UnregisterCustomMenuCmd (__in IOleCommandTarget* lpTarget, INT nCmd)
	{
		IOleCommandTarget* lpRegistered;
		HRESULT hr = m_mapCustom.Find(nCmd, &lpRegistered);
		if(SUCCEEDED(hr))
		{
			if(lpTarget == lpRegistered)
			{
				hr = m_mapCustom.Remove(nCmd, NULL);
				if(SUCCEEDED(hr))
					lpRegistered->Release();
			}
			else
				hr = E_FAIL;
		}
		return hr;
	}

	virtual HRESULT RegisterCustomMessage (__in IMessageHandler* lpHandler, UINT* pmsg)
	{
		HRESULT hr = E_INVALIDARG;
		if(lpHandler && pmsg)
		{
			hr = m_pWindow->RegisterUserMessage(pmsg);
			if(SUCCEEDED(hr))
			{
				hr = m_mapMessages.Add(*pmsg, lpHandler);
				if(SUCCEEDED(hr))
					lpHandler->AddRef();
			}
		}
		return hr;
	}

	virtual HRESULT UnregisterCustomMessage (__in IMessageHandler* lpHandler, UINT msg)
	{
		IMessageHandler* lpRegistered;
		HRESULT hr = m_mapMessages.Find(msg, &lpRegistered);
		if(SUCCEEDED(hr))
		{
			if(lpHandler == lpRegistered)
			{
				hr = m_mapMessages.Remove(msg, NULL);
				if(SUCCEEDED(hr))
				{
					hr = m_pWindow->UnregisterUserMessage(msg);
					lpRegistered->Release();
				}
			}
			else
				hr = E_FAIL;
		}
		return hr;
	}

	virtual HRESULT GetDocument (INeuralDocument** lplpDocument)
	{
		HRESULT hr = E_POINTER;
		if(lplpDocument)
		{
			*lplpDocument = m_lpDoc;
			if(*lplpDocument)
			{
				(*lplpDocument)->AddRef();
				hr = S_OK;
			}
			else
				hr = E_FAIL;
		}
		return hr;
	}

	virtual HRESULT SendAsyncMessage (IMessageHandler* pTarget, UINT nMsg, WPARAM wParam, LPARAM lParam)
	{
		HRESULT hr;
		HWND hwnd;
		ASYNC_DATA* pData;

		EnterCriticalSection(&m_cs);

		CheckIf(NULL == m_pWindow, E_FAIL);
		Check(m_pWindow->GetWindow(&hwnd));
		Check(m_aAsyncData.AppendSlot(&pData));
		SetInterface(pData->pTarget, pTarget);
		pData->nMsg = nMsg;
		pData->wParam = wParam;
		pData->lParam = lParam;
		CheckIfGetLastError(!PostMessage(hwnd, m_nAsyncMessage, 0, 0));

	Cleanup:
		LeaveCriticalSection(&m_cs);
		return hr;
	}
};

CPluginsManager* CPluginsManager::m_lpPlugins = NULL;

HRESULT ExtLoad (IBaseWindow* pWindow)
{
	HRESULT hr = S_OK;
	CPluginsManager::m_lpPlugins = __new CPluginsManager(pWindow);
	if(CPluginsManager::m_lpPlugins)
		CPluginsManager::m_lpPlugins->Load();
	else
		hr = E_OUTOFMEMORY;
	return hr;
}

VOID ExtUnload (VOID)
{
	if(CPluginsManager::m_lpPlugins)
	{
		CPluginsManager::m_lpPlugins->Unload();
		__delete CPluginsManager::m_lpPlugins;
		CPluginsManager::m_lpPlugins = NULL;
	}
}

VOID ExtModifyAppMenu (HMENU hMenu)
{
	CPluginsManager::m_lpPlugins->ExtendMenu(hMenu, PLUGIN_MENU_EXT_APP);
}

VOID ExtAddFramesToMenu (HMENU hMenu)
{
	CPluginsManager::m_lpPlugins->ExtendMenu(hMenu, PLUGIN_MENU_EXT_GROUPS);
}

BOOL ExtAddMoreNeuronesToMenu (HMENU hMenu)
{
	return CPluginsManager::m_lpPlugins->ExtendMenu(hMenu, PLUGIN_MENU_EXT_NEURONES);
}

VOID ExtSetDocument (CNeuralDocument* lpDoc)
{
	CPluginsManager::m_lpPlugins->SetDocument(lpDoc);
}

VOID ExtQueryMenuCommand (OLECMD* lpCmd)
{
	CPluginsManager::m_lpPlugins->QueryMenuCommand(lpCmd);
}

HRESULT ExtExecMenuCommand (DWORD nCmdID, DWORD nCmdExecOpt)
{
	return CPluginsManager::m_lpPlugins->ExecMenuCommand(nCmdID, nCmdExecOpt);
}

BOOL ExtCustomMessage (UINT msg, WPARAM wParam, LPARAM lParam, __out LRESULT& lResult)
{
	return CPluginsManager::m_lpPlugins->CustomMessage(msg, wParam, lParam, lResult);
}

HRESULT ExtGetLinkSources (INeuralSource*** lplplpSources, INT* lpcSources)
{
	return CPluginsManager::TGetFeaturesByName(PLUGIN_FEATURE_SOURCE, lplplpSources, lpcSources);
}
