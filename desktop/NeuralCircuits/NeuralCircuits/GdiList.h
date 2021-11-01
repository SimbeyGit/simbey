#pragma once

#include "Library\Util\DIBDrawing.h"

namespace GdiList
{
	extern HBRUSH hbrRest;
	extern HBRUSH hbrExcited;
	extern HBRUSH hbrTarget;
	extern HBRUSH hbrPinLabel;
	extern HPEN hpnBorder;
	extern HPEN hpnSelected;
	extern HPEN hpnHighlight;
	extern HPEN hpnConnection;
	extern HPEN hpnLiveConn;
	extern HFONT hfLabels;
	extern HFONT hfFrameLabels;

	HRESULT Initialize (VOID);
	VOID Uninitialize (VOID);

	VOID ScalePens (FLOAT fScale);
	VOID ScaleFonts (FLOAT fScale);

	VOID NormalizeRect (RECT* lprc);
	VOID DrawOuterBorder (LPBYTE lpPtr, INT xView, INT yView, LONG lPitch, const RECT* lprc, LONG nBorderThickness, COLORREF crBorder, BYTE bAlpha);
};
