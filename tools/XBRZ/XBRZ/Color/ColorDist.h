#pragma once

#include <math.h>
#include "..\Common\Mask.h"
#include "..\ScalerCfg.h"

class CColorDist
{
protected:
	CScalerCfg* m_pCfg;

public:
	CColorDist (CScalerCfg* pCfg) :
		m_pCfg(pCfg)
	{
	}

	double DistYCbCr (int pix1, int pix2)
	{
		if(pix1 == pix2) return 0.0;

		//http://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.601_conversion
		//YCbCr conversion is a matrix multiplication => take advantage of linearity by subtracting first!
		double rDiff = ((pix1 & Mask::Red) - (pix2 & Mask::Red)) >> 16; //we may delay division by 255 to after matrix multiplication
		double gDiff = ((pix1 & Mask::Green) - (pix2 & Mask::Green)) >> 8;
		double bDiff = (pix1 & Mask::Blue) - (pix2 & Mask::Blue); //subtraction for int is noticeable faster than for double

		const double kB = 0.0722; //ITU-R BT.709 conversion
		const double kR = 0.2126;
		const double kG = 1.0 - kB - kR;

		const double scaleB = 0.5 / (1.0 - kB);
		const double scaleR = 0.5 / (1.0 - kR);

		double y = kR * rDiff + kG * gDiff + kB * bDiff; //[!], analog YCbCr!
		double cB = scaleB * (bDiff - y);
		double cR = scaleR * (rDiff - y);

		// Skip division by 255.
		// Also skip square root here by pre-squaring the config option equalColorTolerance.
		return pow(m_pCfg->LuminanceWeight * y, 2) + pow(cB, 2) + pow(cR, 2);
	}
};
