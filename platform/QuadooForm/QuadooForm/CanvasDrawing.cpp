#include "Runtime.h"
#include "CanvasDrawing.h"
#include "ColorNames.h"

namespace QF
{
	CCanvasSurface::CCanvasSurface() : width(0), height(0), dpi(0)
	{
	}

	HDC CCanvasSurface::DC() const
	{
		return m_surface.DC();
	}

	BYTE* CCanvasSurface::Pixels() const
	{
		return m_surface.Pixels();
	}

	HRESULT CCanvasSurface::Opaque()
	{
		HRESULT hr = S_OK;
		CheckIf(!GdiFlush(),E_FAIL);
		for(UINT n = 0; n < width * height; n++)
		{
			Pixels()[n * 4 + 3] = 255;
		}

	Cleanup:
		return hr;
	}

	HRESULT CCanvasSurface::Fill(COLORREF color)
	{
		HRESULT hr;
		Check(Opaque());
		for(UINT n = 0; n < width * height; n++)
		{
			BYTE* pixel = Pixels() + n * 4;
			pixel[0] = GetBValue(color);
			pixel[1] = GetGValue(color);
			pixel[2] = GetRValue(color);
		}
	Cleanup:
		return hr;
	}

	HRESULT CCanvasSurface::Create(UINT cx, UINT cy, UINT resolution, COLORREF background, CCanvasSurface* previous)
	{
		HRESULT hr;
		Check(m_surface.Create(cx,cy));
		width = cx;
		height = cy;
		dpi = resolution;
		Check(Fill(background));
		if(previous)
		{
			Check(previous->Opaque());
			CheckIf(!BitBlt(DC(),0,0,min(cx,previous->width),min(cy,previous->height),previous->DC(),0,0,SRCCOPY),E_FAIL);
			Check(Opaque());
		}
	Cleanup:
		return hr;
	}

	HRESULT CCanvasSurface::Export(ISequentialStream** data, ULONG* count)
	{
		HRESULT hr;
		CMemoryStream rgba;
		TStackRef<CMemoryStream> encoded;
		BYTE* target;
		CheckIf(!data || !count,E_POINTER);
		*data = NULL;
		*count = 0;
		Check(Opaque());
		Check(rgba.TWriteAdvance<BYTE>(&target,width * height * 4));
		for(UINT n = 0; n < width * height; n++)
		{
			const BYTE* pixel = Pixels() + n * 4;
			target[n * 4] = pixel[2];
			target[n * 4 + 1] = pixel[1];
			target[n * 4 + 2] = pixel[0];
			target[n * 4 + 3] = 255;
		}
		encoded.Attach(__new CMemoryStream);
		CheckAlloc(encoded);
		// Keep a deterministic RGBA PNG and avoid the bundled codec's legacy
		// palette profiler, which assumes its color-tree allocation succeeds.
		Check(QFI::EncodePNG(rgba.GetReadPtr(),width,height,*encoded,FALSE));
		*count = encoded->DataRemaining();
		*data = encoded.Detach();
	Cleanup:
		return hr;
	}

	HRESULT CanvasColor(COLORREF color, BOOL resolve, COLORREF* result)
	{
		HRESULT hr = S_OK;
		DWORD tag = color & 0xff000000;
		CheckIf(!result,E_POINTER);
		*result = 0;
		if(tag)
		{
			CheckIf(!resolve || tag != 0x80000000 || !QuadooColors::SystemColorName(color & 0xffffff)[0],E_INVALIDARG);
		}
		*result = resolve ? ResolveQuadooColor(color) : color;
	Cleanup:
		return hr;
	}

	struct CanvasRun
	{
		INT start;
		INT count;
		INT x;
		INT y;
	};

	HRESULT CanvasText(HDC target, HFONT font, RSTRING text, INT x, INT y, COLORREF color, SIZE* measured, BOOL draw)
	{
		HRESULT hr;
		HDC dc = target;
		INT saved = 0;
		TArray<CanvasRun> runs;
		TEXTMETRICW metrics;
		SIZE space;
		SIZE result = {0,0};
		INT length = RStrLen(text);
		INT lineWidth = 0;
		INT line = 0;
		CheckIf(!measured,E_POINTER);
		measured->cx = measured->cy = 0;
		CheckIf(!font || (text && !RStrIsWide(text)),E_INVALIDARG);
		if(!dc)
		{
			dc = CreateCompatibleDC(NULL);
			CheckIf(!dc,HrEnsureLastError());
		}
		saved = SaveDC(dc);
		CheckIf(!saved,E_FAIL);
		CheckIf(!SetMapMode(dc,MM_TEXT),E_FAIL);
		CheckIf(!SelectObject(dc,font),E_FAIL);
		SetTextAlign(dc,TA_LEFT | TA_TOP | TA_NOUPDATECP);
		SetTextCharacterExtra(dc,0);
		SetTextJustification(dc,0,0);
		CheckIf(!GetTextMetricsW(dc,&metrics) || metrics.tmHeight <= 0,E_FAIL);
		CheckIf(!GetTextExtentPoint32W(dc,L" ",StaticLength(L" "),&space),E_FAIL);
		CheckIf(space.cx <= 0 || space.cx > INT_MAX / 8,E_FAIL);
		for(INT pos = 0; pos < length; )
		{
			PCWSTR characters = RStrToWide(text);
			INT start = pos;
			SIZE extent;
			while(pos < length && characters[pos] != L'\r' && characters[pos] != L'\n' && characters[pos] != L'\t')
			{
				CheckIf(!characters[pos],E_INVALIDARG);
				pos++;
			}
			if(pos > start)
			{
				CanvasRun run;
				CheckIf(!GetTextExtentPoint32W(dc,characters + start,pos - start,&extent),E_FAIL);
				CheckIf(extent.cx < 0 || static_cast<LONGLONG>(lineWidth) + extent.cx > INT_MAX,HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW));
				run.start = start;
				run.count = pos - start;
				run.x = lineWidth;
				CheckIf(static_cast<LONGLONG>(line) * metrics.tmHeight > INT_MAX,HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW));
				run.y = line * metrics.tmHeight;
				Check(runs.Append(run));
				lineWidth += extent.cx;
			}
			if(pos < length)
			{
				WCHAR ch = characters[pos++];
				if(ch == L'\t')
				{
					LONGLONG stop = (static_cast<LONGLONG>(lineWidth) / (space.cx * 8) + 1) * (space.cx * 8);
					CheckIf(stop > INT_MAX,HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW));
					lineWidth = static_cast<INT>(stop);
				}
				else
				{
					if(ch == L'\r' && pos < length && characters[pos] == L'\n')
					{
						pos++;
					}
					result.cx = max(result.cx,lineWidth);
					lineWidth = 0;
					line++;
				}
			}
		}
		result.cx = max(result.cx,lineWidth);
		CheckIf((static_cast<LONGLONG>(line) + 1) * metrics.tmHeight > INT_MAX,HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW));
		result.cy = length ? (line + 1) * metrics.tmHeight : 0;
		if(draw)
		{
			CheckIf(static_cast<LONGLONG>(x) + result.cx > INT_MAX || static_cast<LONGLONG>(y) + result.cy > INT_MAX,HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW));
			SetTextColor(dc,color);
			SetBkMode(dc,TRANSPARENT);
			for(sysint n = 0; n < runs.Length(); n++)
			{
				CheckIf(!ExtTextOutW(dc,x + runs[n].x,y + runs[n].y,0,NULL,RStrToWide(text) + runs[n].start,runs[n].count,NULL),E_FAIL);
			}
		}
		*measured = result;
		hr = S_OK;

	Cleanup:
		if(saved)
		{
			RestoreDC(dc,saved);
		}
		if(dc && !target)
		{
			DeleteDC(dc);
		}
		return hr;
	}
}
