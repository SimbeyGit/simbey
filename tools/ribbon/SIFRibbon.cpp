#include <windows.h>
#include <Shlwapi.h>
#include "..\..\Shared\Library\Core\SDKExtras.h"
#include "..\..\Shared\Library\Core\CoreDefs.h"
#include "inc\SIFRibbon.h"

typedef IStream* (WINAPI* PFN_SHOPENREGSTREAM2A)(__in HKEY hKey, LPCSTR pcszSubKey, LPCSTR pcszValue, DWORD grfMode);

HRESULT CSIFRibbon::Create (FLOAT (*pfnScale)(FLOAT), __deref_out CSIFRibbon** ppRibbon)
{
	HRESULT hr;
	CSIFRibbon* pRibbon = __new CSIFRibbon(pfnScale);

	CheckAlloc(pRibbon);
	Check(CoCreateInstance(CLSID_UIRibbonImageFromBitmapFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pRibbon->m_pImageFactory)));
	Check(CoCreateInstance(CLSID_UIRibbonFramework, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pRibbon->m_pFramework)));
	*ppRibbon = pRibbon;
	pRibbon = NULL;

Cleanup:
	SafeRelease(pRibbon);
	return hr;
}

CSIFRibbon::CSIFRibbon (FLOAT (*pfnScale)(FLOAT)) :
	m_pfnScale(pfnScale),
	m_hwndParent(NULL),
	m_pHost(NULL),
	m_pImageFactory(NULL),
	m_pFramework(NULL),
	m_nModes(0),
	m_nRibbonHeight(0),
	m_pSmallImages(NULL),
	m_pLargeImages(NULL)
{
}

CSIFRibbon::~CSIFRibbon ()
{
	Assert(NULL == m_pHost);
	Assert(NULL == m_pFramework);
	Assert(NULL == m_pImageFactory);
	Assert(NULL == m_pSmallImages);
	Assert(NULL == m_pLargeImages);
}

HRESULT CSIFRibbon::Initialize (HWND hwnd, __in IRibbonHost* pHost)
{
	HRESULT hr;

	CheckIf(NULL == hwnd || NULL == pHost, E_INVALIDARG);
	CheckIf(m_hwndParent || NULL == m_pFramework, E_UNEXPECTED);

	m_hwndParent = hwnd;
	m_pHost = pHost;
	m_pHost->AddRef();

	Check(m_pFramework->Initialize(m_hwndParent, this));

Cleanup:
	return hr;
}

HRESULT CSIFRibbon::SetModes (UINT32 nModes)
{
	HRESULT hr;

	CheckIf(0 == nModes, E_INVALIDARG);

	if(0 == m_nModes)
	{
		HMODULE hModule;
		WCHAR wzResource[MAX_PATH];

		Check(m_pHost->GetRibbonResource(&hModule, wzResource, ARRAYSIZE(wzResource)));

		if(NULL == m_pSmallImages && NULL == m_pLargeImages)
		{
			if(m_pfnScale(96.0f) < 120.0f)
			{
				Check(sifLoadResource(hModule, L"SIF16_96", &m_pSmallImages));
				Check(sifLoadResource(hModule, L"SIF32_96", &m_pLargeImages));
			}
			else
			{
				Check(sifLoadResource(hModule, L"SIF16_144", &m_pSmallImages));
				Check(sifLoadResource(hModule, L"SIF32_144", &m_pLargeImages));
			}
		}

		m_nModes = nModes;
		Check(m_pFramework->LoadUI(hModule, wzResource));

		// During the call to IUIFramework::LoadUI(), we'll be called back on
		// OnViewChanged().  From there, we'll call IUIFramework::SetModes().
	}
	else
	{
		m_nModes = nModes;
		Check(m_pFramework->SetModes(nModes));
	}

Cleanup:
	return hr;
}

VOID CSIFRibbon::Unload (VOID)
{
	if(m_pFramework)
	{
		// Release circular reference with the ribbon framework.
		m_pFramework->Destroy();

		SafeRelease(m_pFramework);
	}

	// Release circular reference with the host.
	SafeRelease(m_pHost);

	SafeRelease(m_pImageFactory);

	if(m_pSmallImages)
	{
		m_pSmallImages->Close();
		SafeRelease(m_pSmallImages);
	}

	if(m_pLargeImages)
	{
		m_pLargeImages->Close();
		SafeRelease(m_pLargeImages);
	}

	// At this point, the only thing left to do with CSIFRibbon is to release it.
}

UINT32 CSIFRibbon::GetHeight (VOID)
{
	return m_nRibbonHeight;
}

HRESULT CSIFRibbon::InvalidateEnabled (VOID)
{
	return m_pFramework->InvalidateUICommand(0, UI_INVALIDATIONS_STATE, &UI_PKEY_Enabled);
}

HRESULT CSIFRibbon::InvalidateEnabled (UINT32 commandId)
{
	return m_pFramework->InvalidateUICommand(commandId, UI_INVALIDATIONS_STATE, &UI_PKEY_Enabled);
}

HRESULT CSIFRibbon::UpdateProperty (UINT32 commandId, const PROPERTYKEY* key)
{
	return m_pFramework->InvalidateUICommand(commandId, UI_INVALIDATIONS_PROPERTY, key);
}

HRESULT CSIFRibbon::SaveSettings (VOID)
{
	HRESULT hr;

	CheckIf(NULL == m_pHost || NULL == m_pFramework, E_UNEXPECTED);
	Check(_SaveSettings());

Cleanup:
	return hr;
}

HRESULT CSIFRibbon::LoadImageForCommand (UINT32 commandId, REFPROPERTYKEY key, __out PROPVARIANT* pValue)
{
	HRESULT hr;
	ISimbeyInterchangeFile* pSource;	// No reference will be owned by this pointer
	TStackRef<ISimbeyInterchangeFileLayer> srLayer;
	TStackRef<IUIImage> srImage;
	HBITMAP hDIB;

	Assert(UI_PKEY_LargeImage == key || UI_PKEY_SmallImage == key);

	commandId = m_pHost->TranslateGroupToImage(commandId);

	if(UI_PKEY_LargeImage == key)
	{
		commandId = m_pHost->TranslateImageToLargeImage(commandId);
		pSource = m_pLargeImages;
	}
	else
		pSource = m_pSmallImages;

	Check(pSource->GetLayer(commandId, &srLayer));
	Check(srLayer->GetAsDIB(NULL, &hDIB));
	Check(m_pImageFactory->CreateImage(hDIB, UI_OWNERSHIP_TRANSFER, &srImage));
	hDIB = NULL;	// Now owned by srImage

	pValue->punkVal = srImage.Detach();
	pValue->vt = VT_UNKNOWN;

Cleanup:
	SafeDeleteGdiObject(hDIB);
	return hr;
}

HRESULT CSIFRibbon::CreateImageFromDIB (HBITMAP hDIB, __deref_out IUIImage** ppImage)
{
	return m_pImageFactory->CreateImage(hDIB, UI_OWNERSHIP_TRANSFER, ppImage);
}

// IUIApplication

HRESULT STDMETHODCALLTYPE CSIFRibbon::OnViewChanged (UINT32 nViewID, UI_VIEWTYPE typeID, IUnknown* pView, UI_VIEWVERB verb, INT32 uReasonCode)
{
	HRESULT hr;

	Assert(m_pHost);

	if(UI_VIEWVERB_CREATE == verb)
	{
		// The Scenic Ribbon team says setting the application mode during the UI_VIEWVERB_CREATE
		// callback is the best way to initially set the application mode without flickering.
		Check(m_pFramework->SetModes(m_nModes));

		// The Scenic Ribbon team also says loading settings during UI_VIEWVERB_CREATE will
		// further reduce time spent redrawing, which means less flickering on startup.
		Check(_LoadSettings());
	}
	else if(UI_VIEWVERB_SIZE == verb)
	{
		TStackRef<IUIRibbon> srUIRibbon;
		Check(m_pFramework->GetView(0, IID_PPV_ARGS(&srUIRibbon)));
		Check(srUIRibbon->GetHeight(&m_nRibbonHeight));
	}

	Check(m_pHost->OnViewChanged(nViewID, typeID, pView, verb, uReasonCode));

Cleanup:
	return hr;
}

HRESULT STDMETHODCALLTYPE CSIFRibbon::OnCreateUICommand (UINT32 commandId, UI_COMMANDTYPE typeID, IUICommandHandler** ppCommandHandler)
{
	return m_pHost->OnCreateUICommand(commandId, typeID, ppCommandHandler);
}

HRESULT STDMETHODCALLTYPE CSIFRibbon::OnDestroyUICommand (UINT32 commandId, UI_COMMANDTYPE typeID, IUICommandHandler* pCommandHandler)
{
	return m_pHost->OnDestroyUICommand(commandId, typeID, pCommandHandler);
}

HRESULT CSIFRibbon::_SaveSettings (VOID)
{
	HRESULT hr;
	CHAR szRegKey[MAX_PATH];
	CHAR szRegValue[MAX_PATH];
	HMODULE hShellUtil = NULL;
	IStream* pStream = NULL;
	IUIRibbon* pRibbon = NULL;

	CheckIfIgnore(0 == m_nModes, S_OK);

	Assert(m_pHost && m_pFramework);

	Check(m_pHost->GetRibbonSettingsKey(szRegKey, ARRAYSIZE(szRegKey)));
	Check(m_pHost->GetRibbonSettingsValue(szRegValue, ARRAYSIZE(szRegValue)));
	Check(RegOpenStream(HKEY_CURRENT_USER, szRegKey, szRegValue, STGM_WRITE, &pStream, &hShellUtil));

	Check(m_pFramework->GetView(0, IID_PPV_ARGS(&pRibbon)));
	Check(pRibbon->SaveSettingsToStream(pStream));

Cleanup:
	SafeRelease(pRibbon);
	SafeRelease(pStream);
	if(hShellUtil)
		FreeLibrary(hShellUtil);
	return hr;
}

HRESULT CSIFRibbon::_LoadSettings (VOID)
{
	HRESULT hr;
	CHAR szRegKey[MAX_PATH];
	CHAR szRegValue[MAX_PATH];
	HMODULE hShellUtil = NULL;
	IStream* pStream = NULL;
	IUIRibbon* pRibbon = NULL;
	STATSTG stgStat;
	LARGE_INTEGER liSeek = {0};

	Assert(0 != m_nModes);
	Assert(m_pHost && m_pFramework);

	Check(m_pHost->GetRibbonSettingsKey(szRegKey, ARRAYSIZE(szRegKey)));
	Check(m_pHost->GetRibbonSettingsValue(szRegValue, ARRAYSIZE(szRegValue)));

	hr = RegOpenStream(HKEY_CURRENT_USER, szRegKey, szRegValue, STGM_READ, &pStream, &hShellUtil);
	CheckIfIgnore(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) == hr, S_FALSE);
	Check(hr);  // Check all other errors.

	Check(pStream->Stat(&stgStat, STATFLAG_NONAME));
	CheckIfIgnore(0 == stgStat.cbSize.QuadPart, S_OK);
	Check(pStream->Seek(liSeek, STREAM_SEEK_SET, NULL));

	Check(m_pFramework->GetView(0, IID_PPV_ARGS(&pRibbon)));
	Check(pRibbon->LoadSettingsFromStream(pStream));

Cleanup:
	SafeRelease(pRibbon);
	SafeRelease(pStream);
	if(hShellUtil)
		FreeLibrary(hShellUtil);
	return hr;
}

HRESULT CSIFRibbon::RegOpenStream (HKEY hKey, PCSTR pcszSubKey, PCSTR pcszValue, DWORD grfMode, __deref_out IStream** ppStream, __out HMODULE* phShellUtil)
{
	HRESULT hr;
	HMODULE hShellUtil = LoadLibraryA("SHLWAPI.DLL");
	PFN_SHOPENREGSTREAM2A pfnSHOpenRegStream2A;

	CheckIfGetLastError(NULL == hShellUtil);
	Check(TGetFunction(hShellUtil, "SHOpenRegStream2A", &pfnSHOpenRegStream2A));

	*ppStream = pfnSHOpenRegStream2A(hKey, pcszSubKey, pcszValue, grfMode);
	CheckIf(NULL == *ppStream, HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

	*phShellUtil = hShellUtil;

Cleanup:
	if(FAILED(hr) && hShellUtil)
		FreeLibrary(hShellUtil);
	return hr;
}
