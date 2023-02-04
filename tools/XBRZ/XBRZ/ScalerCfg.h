#pragma once

#include "XBRZ.h"

class CScalerCfg : public SCALER_CONFIG
{
public:
	CScalerCfg ()
	{
		// These are the default values:
		LuminanceWeight = 1.0;
		EqualColorTolerance = 30.0;
		DominantDirectionThreshold = 3.6;
		SteepDirectionThreshold = 2.2;
	}

	CScalerCfg (const SCALER_CONFIG* pConfig)
	{
		LuminanceWeight = pConfig->LuminanceWeight;
		EqualColorTolerance = pConfig->EqualColorTolerance;
		DominantDirectionThreshold = pConfig->DominantDirectionThreshold;
		SteepDirectionThreshold = pConfig->SteepDirectionThreshold;
	}
};
