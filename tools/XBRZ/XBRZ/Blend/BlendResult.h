#pragma once

class CBlendResult
{
public:
	short F;
	short G;
	short J;
	short K;

	CBlendResult ()
	{
		Reset();
	}

	void Reset (void)
	{
		F = G = J = K = (short)0;
	}
};
