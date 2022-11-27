#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Core\MemoryStream.h"
#include "Published\JSON.h"
#include "BaseScreen.h"

CBaseScreen::CBaseScreen (IScreenHost* pHost, CInteractiveSurface* pSurface, CSIFPackage* pPackage) :
	m_pHost(pHost),
	m_pSurface(pSurface),
	m_pPackage(pPackage)
{
	m_pSurface->AddRef();
	m_pPackage->AddRef();
}

CBaseScreen::~CBaseScreen ()
{
	SafeRelease(m_pPackage);
	SafeRelease(m_pSurface);
}

HRESULT CBaseScreen::LoadAnimator (PCWSTR pcwzAnimator, INT cchAnimator, PCWSTR pcwzSIF, __deref_out ISimbeyInterchangeAnimator** ppAnimator, BOOL fUsePositionAsOffset)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	TStackRef<IJSONObject> srDef;

	Check(m_pPackage->GetJSONData(pcwzAnimator, cchAnimator, &srv));
	Check(srv->GetObject(&srDef));
	Check(CreateAnimator(srDef, pcwzSIF, ppAnimator, fUsePositionAsOffset));

Cleanup:
	return hr;
}

HRESULT CBaseScreen::CreateAnimator (IJSONObject* pDef, PCWSTR pcwzSIF, __deref_out ISimbeyInterchangeAnimator** ppAnimator, BOOL fUsePositionAsOffset)
{
	return CreateAnimator(m_pPackage, pDef, pcwzSIF, ppAnimator, fUsePositionAsOffset);
}

HRESULT CBaseScreen::CreateAnimator (CSIFPackage* pPackage, IJSONObject* pDef, PCWSTR pcwzSIF, __deref_out ISimbeyInterchangeAnimator** ppAnimator, BOOL fUsePositionAsOffset)
{
	HRESULT hr;
	ISimbeyInterchangeFile* pSIF = NULL;

	Check(pPackage->OpenSIF(pcwzSIF, &pSIF));
	Check(CreateAnimator(pSIF, pDef, ppAnimator, fUsePositionAsOffset));

Cleanup:
	if(pSIF)
	{
		pSIF->Close();
		pSIF->Release();
	}
	return hr;
}

HRESULT CBaseScreen::CreateAnimator (ISimbeyInterchangeFile* pSIF, IJSONObject* pDef, __deref_out ISimbeyInterchangeAnimator** ppAnimator, BOOL fUsePositionAsOffset)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	TStackRef<IJSONArray> srImages, srAnimations;
	TStackRef<ISimbeyInterchangeAnimator> srAnimator;
	sysint cImages, cAnimations;

	Check(pDef->FindNonNullValueW(L"images", &srv));
	Check(srv->GetArray(&srImages));
	cImages = srImages->Count();
	srv.Release();

	Check(pDef->FindNonNullValueW(L"animations", &srv));
	Check(srv->GetArray(&srAnimations));
	cAnimations = srAnimations->Count();
	srv.Release();

	Check(sifCreateAnimator(cImages, cAnimations, &srAnimator));

	for(sysint i = 0; i < cImages; i++)
	{
		TStackRef<IJSONObject> srImage;
		TStackRef<ISimbeyInterchangeFileLayer> srLayer;
		DWORD nLayer;

		Check(srImages->GetObject(i, &srImage));
		Check(srImage->FindNonNullValueW(L"layer", &srv));
		Check(srv->GetDWord(&nLayer));
		srv.Release();

		Check(pSIF->GetLayerByIndex(nLayer, &srLayer));
		Check(srAnimator->SetImage(i, FALSE, srLayer, fUsePositionAsOffset));
	}

	for(sysint i = 0; i < cAnimations; i++)
	{
		TStackRef<IJSONObject> srAnimation;
		TStackRef<IJSONArray> srFrames;

		Check(srAnimations->GetObject(i, &srAnimation));
		Check(srAnimation->FindNonNullValueW(L"frames", &srv));
		Check(srv->GetArray(&srFrames));
		Check(AddFramesToAnimation(srAnimator, i, srFrames));
		srv.Release();

		if(SUCCEEDED(srAnimation->FindNonNullValueW(L"next", &srv)))
		{
			INT nNextAnimation;
			Check(srv->GetInteger(&nNextAnimation));
			Check(srAnimator->ConfigureAnimation(i, NULL, nNextAnimation));
			srv.Release();
		}
	}

	*ppAnimator = srAnimator.Detach();

Cleanup:
	return hr;
}

HRESULT CBaseScreen::AddFramesToAnimation (ISimbeyInterchangeAnimator* pAnimator, INT nAnimation, IJSONArray* pFrames)
{
	HRESULT hr;
	sysint cFrames;
	TStackRef<IJSONValue> srv;

	cFrames = pFrames->Count();

	for(sysint i = 0; i < cFrames; i++)
	{
		TStackRef<IJSONObject> srFrame;
		INT nImage, cTicks, xOffset, yOffset;

		Check(pFrames->GetObject(i, &srFrame));

		Check(srFrame->FindNonNullValueW(L"image", &srv));
		Check(srv->GetInteger(&nImage));
		srv.Release();

		Check(srFrame->FindNonNullValueW(L"ticks", &srv));
		Check(srv->GetInteger(&cTicks));
		srv.Release();

		Check(srFrame->FindNonNullValueW(L"xoffset", &srv));
		Check(srv->GetInteger(&xOffset));
		srv.Release();

		Check(srFrame->FindNonNullValueW(L"yoffset", &srv));
		Check(srv->GetInteger(&yOffset));
		srv.Release();

		Check(pAnimator->AddFrame(nAnimation, cTicks, nImage, xOffset, yOffset));
	}

	Check(pAnimator->CompactFrames(nAnimation));

Cleanup:
	return hr;
}

HRESULT CBaseScreen::CreateDefaultAnimator (ISimbeyInterchangeFile* pSIF, BOOL fUsePositionAsOffset, INT nTickDelay, BOOL fRepeat, BOOL fPremultiply, __deref_out ISimbeyInterchangeAnimator** ppAnimator, __out_opt INT* pcFrames)
{
	HRESULT hr;
	TStackRef<ISimbeyInterchangeAnimator> srAnimator;
	DWORD cLayers = pSIF->GetLayerCount();

	Check(sifCreateAnimator(cLayers, 1, &srAnimator));

	for(DWORD i = 0; i < cLayers; i++)
	{
		TStackRef<ISimbeyInterchangeFileLayer> srLayer;

		Check(pSIF->GetLayerByIndex(i, &srLayer));
		Check(srAnimator->SetImage(i, fPremultiply, srLayer, fUsePositionAsOffset));
		Check(srAnimator->AddFrame(0, nTickDelay, i, 0, 0));
	}

	if(!fRepeat)
		Check(srAnimator->ConfigureAnimation(0, NULL, 0));

	Check(srAnimator->CompactFrames(0));

	*ppAnimator = srAnimator.Detach();
	if(pcFrames)
		*pcFrames = cLayers;

Cleanup:
	return hr;
}
