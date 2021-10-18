#pragma once

namespace DPI
{
	extern LONG xDPI;
	extern LONG yDPI;
	extern FLOAT xScale;
	extern FLOAT yScale;

	HRESULT SetDPIAware (VOID);

	BOOL Initialize (VOID);
	VOID SetDPI (LONG xCustomDPI, LONG yCustomDPI);

	// Normalize() and Scale() using the xScale member.
	inline FLOAT Normalize (FLOAT n) { return n / xScale; }
	inline FLOAT Scale (FLOAT n) { return n * xScale; }

	// Use ScaleY() when you need to scale an individual Y component.
	inline FLOAT ScaleY (FLOAT n) { return n * yScale; }

	VOID NormalizeScaledPoint (LPPOINT lpPoint);
	VOID ScaleNormalizedPoint (LPPOINT lpPoint);

	VOID NormalizeScaledRectSize (LPRECT lpRect);
	VOID ScaleNormalizedRectSize (LPRECT lpRect);

	VOID ScaleSize (SIZE* pSize);

	INT MapPointSize (INT nPointSize);
};
