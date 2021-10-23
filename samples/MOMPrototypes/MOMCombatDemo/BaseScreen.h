#pragma once

#include "Library\Core\BaseUnknown.h"
#include "IScreen.h"

class CBaseScreen :
	public CBaseUnknown,
	public IScreen
{
protected:
	IScreenHost* m_pHost;
	CInteractiveSurface* m_pSurface;
	CSIFPackage* m_pPackage;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IScreen)
	END_UNK_MAP

public:
	CBaseScreen (IScreenHost* pHost, CInteractiveSurface* pSurface, CSIFPackage* pPackage);
	virtual ~CBaseScreen ();

protected:
	HRESULT LoadAnimator (PCWSTR pcwzAnimator, INT cchAnimator, PCWSTR pcwzSIF, __deref_out ISimbeyInterchangeAnimator** ppAnimator, BOOL fUsePositionAsOffset);
	HRESULT CreateAnimator (IJSONObject* pDef, PCWSTR pcwzSIF, __deref_out ISimbeyInterchangeAnimator** ppAnimator, BOOL fUsePositionAsOffset);

	static HRESULT CreateAnimator (CSIFPackage* pPackage, IJSONObject* pDef, PCWSTR pcwzSIF, __deref_out ISimbeyInterchangeAnimator** ppAnimator, BOOL fUsePositionAsOffset);
	static HRESULT CreateAnimator (ISimbeyInterchangeFile* pSIF, IJSONObject* pDef, __deref_out ISimbeyInterchangeAnimator** ppAnimator, BOOL fUsePositionAsOffset);

	static HRESULT AddFramesToAnimation (ISimbeyInterchangeAnimator* pAnimator, INT nAnimation, IJSONArray* pFrames);

public:
	static HRESULT CreateDefaultAnimator (ISimbeyInterchangeFile* pSIF, BOOL fUsePositionAsOffset, INT nTickDelay, BOOL fRepeat, __deref_out ISimbeyInterchangeAnimator** ppAnimator, __out_opt INT* pcFrames);
};
