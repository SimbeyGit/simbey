#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\DPI.h"
#include "GdiList.h"

namespace GdiList
{
	static INT nPenWidth = 0;

	HBRUSH hbrRest = NULL;
	HBRUSH hbrExcited = NULL;
	HBRUSH hbrTarget = NULL;
	HBRUSH hbrPinLabel = NULL;
	HPEN hpnBorder = NULL;
	HPEN hpnSelected = NULL;
	HPEN hpnHighlight = NULL;
	HPEN hpnConnection = NULL;
	HPEN hpnLiveConn = NULL;
	HFONT hfLabels = NULL;
	HFONT hfFrameLabels = NULL;

	HRESULT Initialize (VOID)
	{
		HRESULT hr;
		LOGFONT lfLabels = {0};

		hbrRest = CreateSolidBrush(RGB(192,192,192));
		hbrExcited = CreateSolidBrush(RGB(64,64,255));
		hbrTarget = CreateSolidBrush(RGB(64,255,64));
		hbrPinLabel = CreateSolidBrush(RGB(220,225,255));
		hpnBorder = CreatePen(PS_SOLID,1,0);
		hpnSelected = CreatePen(PS_SOLID,2,RGB(255,0,0));
		hpnHighlight = CreatePen(PS_SOLID,3,RGB(255,0,0));
		hpnConnection = NULL;	// Initialized later.
		hpnLiveConn = NULL;		// Initialized later.

		lfLabels.lfHeight = (LONG)DPI::Scale(16.0f);
		lfLabels.lfWeight = FW_SEMIBOLD;
		lfLabels.lfCharSet = ANSI_CHARSET;
		lfLabels.lfOutPrecision = OUT_DEFAULT_PRECIS;
		lfLabels.lfClipPrecision = CLIP_DEFAULT_PRECIS;
		lfLabels.lfQuality = DRAFT_QUALITY;
		lfLabels.lfPitchAndFamily = FIXED_PITCH;
		lstrcpy(lfLabels.lfFaceName,"MS Shell Dlg 2");
		hfLabels = CreateFontIndirect(&lfLabels);
		if(hfLabels == NULL)
		{
			lstrcpy(lfLabels.lfFaceName,"MS Shell Dlg");
			hfLabels = CreateFontIndirect(&lfLabels);
		}

		hfFrameLabels = NULL;

		if(hbrRest && hbrExcited && hbrTarget && hbrPinLabel && hpnBorder && hpnSelected && hpnHighlight && hfLabels)
			hr = S_OK;
		else
			hr = E_FAIL;
		return hr;
	}

	VOID Uninitialize (VOID)
	{
		DeleteObject(hfFrameLabels);
		DeleteObject(hfLabels);
		DeleteObject(hpnLiveConn);
		DeleteObject(hpnConnection);
		DeleteObject(hpnHighlight);
		DeleteObject(hpnSelected);
		DeleteObject(hpnBorder);
		DeleteObject(hbrPinLabel);
		DeleteObject(hbrTarget);
		DeleteObject(hbrExcited);
		DeleteObject(hbrRest);

		hbrRest = NULL;
		hbrExcited = NULL;
		hbrTarget = NULL;
		hbrPinLabel = NULL;
		hpnBorder = NULL;
		hpnSelected = NULL;
		hpnHighlight = NULL;
		hpnConnection = NULL;
		hpnLiveConn = NULL;
		hfLabels = NULL;
		hfFrameLabels = NULL;
	}

	VOID ScalePens (FLOAT fScale)
	{
		INT nNewWidth = (INT)(fScale + 0.5f);
		if(nNewWidth < 1)
			nNewWidth = 1;
		if(nNewWidth != nPenWidth)
		{
			DeleteObject(hpnLiveConn);
			DeleteObject(hpnConnection);

			hpnConnection = CreatePen(PS_SOLID,nNewWidth,0);
			hpnLiveConn = CreatePen(PS_SOLID,nNewWidth,RGB(64,64,255));

			nPenWidth = nNewWidth;
		}
	}

	VOID ScaleFonts (FLOAT fScale)
	{
		LOGFONT lf = {0};

		lf.lfHeight = (LONG)(16.0f * fScale);
		lf.lfWeight = FW_SEMIBOLD;
		lf.lfCharSet = ANSI_CHARSET;
		lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
		lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
		lf.lfQuality = DRAFT_QUALITY;
		lf.lfPitchAndFamily = FIXED_PITCH;
		lstrcpy(lf.lfFaceName,"MS Shell Dlg 2");

		SafeDeleteGdiObject(hfFrameLabels);

		if(0 < lf.lfHeight)
		{
			hfFrameLabels = CreateFontIndirect(&lf);
			if(NULL == hfFrameLabels)
			{
				lstrcpy(lf.lfFaceName,"MS Shell Dlg");
				hfFrameLabels = CreateFontIndirect(&lf);
			}
		}
	}

	VOID DrawOuterBorder (LPBYTE lpPtr, INT xView, INT yView, LONG lPitch, const RECT* lprc, LONG nBorderThickness, COLORREF crBorder, BYTE bAlpha)
	{
		RECT rcBorder;

		// Left
		rcBorder.left = lprc->left - nBorderThickness;
		rcBorder.top = lprc->top - nBorderThickness;
		rcBorder.right = lprc->left;
		rcBorder.bottom = lprc->bottom + nBorderThickness;
		DIBDrawing::AlphaFill24(lpPtr,xView,yView,lPitch,&rcBorder,crBorder,bAlpha);

		// Top
		rcBorder.left = lprc->left;
		rcBorder.bottom = lprc->top;
		rcBorder.right = lprc->right;
		DIBDrawing::AlphaFill24(lpPtr,xView,yView,lPitch,&rcBorder,crBorder,bAlpha);

		// Right
		rcBorder.left = lprc->right;
		rcBorder.right = lprc->right + nBorderThickness;
		rcBorder.bottom = lprc->bottom + nBorderThickness;
		DIBDrawing::AlphaFill24(lpPtr,xView,yView,lPitch,&rcBorder,crBorder,bAlpha);

		// Bottom
		rcBorder.left = lprc->left;
		rcBorder.top = lprc->bottom;
		rcBorder.right = lprc->right;
		DIBDrawing::AlphaFill24(lpPtr,xView,yView,lPitch,&rcBorder,crBorder,bAlpha);
	}
}
