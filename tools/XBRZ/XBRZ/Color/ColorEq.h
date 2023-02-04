#pragma once

#include "ColorDist.h"

class CColorEq : CColorDist
{
public:
	CColorEq (CScalerCfg* pCfg) :
		CColorDist(pCfg)
	{
	}

	bool IsColorEqual (int color1, int color2)
	{
		double eqColorThres = pow(m_pCfg->EqualColorTolerance, 2);
            return DistYCbCr(color1, color2) < eqColorThres;
	}
};
