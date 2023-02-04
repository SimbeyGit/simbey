#pragma once

#include "..\Common\Mask.h"
#include "OutputMatrix.h"

interface IScaler
{
	virtual int GetScale (void) = 0;
	virtual void BlendLineSteep (int col, __out Output::CMatrix& out) = 0;
	virtual void BlendLineSteepAndShallow (int col, __out Output::CMatrix& out) = 0;
	virtual void BlendLineShallow (int col, __out Output::CMatrix& out) = 0;
	virtual void BlendLineDiagonal (int col, __out Output::CMatrix& out) = 0;
	virtual void BlendCorner (int col, __out Output::CMatrix& out) = 0;
};

class CScalerBase
{
protected:
	static void AlphaBlend (int n, int m, CIntPtr& dstPtr, int col)
	{
		//assert n < 256 : "possible overflow of (col & redMask) * N";
		//assert m < 256 : "possible overflow of (col & redMask) * N + (dst & redMask) * (M - N)";
		//assert 0 < n && n < m : "0 < N && N < M";

		//this works because 8 upper bits are free
		int dst = dstPtr.Get();
		int redComponent = BlendComponent(Mask::Red, n, m, dst, col);
		int greenComponent = BlendComponent(Mask::Green, n, m, dst, col);
		int blueComponent = BlendComponent(Mask::Blue, n, m, dst, col);
		int blend = (redComponent | greenComponent | blueComponent);
		dstPtr.Set(blend | (int)0xff000000);
	}

	static int BlendComponent (int mask, int n, int m, int inPixel, int setPixel)
	{
		int inChan = inPixel & mask;
		int setChan = setPixel & mask;
		int blend = setChan * n + inChan * (m - n);
		int component = mask & (blend / m);
		return component;
	}
};

class CScaler2X :
	public CScalerBase,
	public IScaler
{
public:
	virtual int GetScale (void)
	{
		return 2;
	}

	virtual void BlendLineSteep (int col, __out Output::CMatrix& out)
	{
		AlphaBlend(1, 4, out.Ref(0, GetScale() - 1), col);
		AlphaBlend(3, 4, out.Ref(1, GetScale() - 1), col);
	}

	virtual void BlendLineSteepAndShallow (int col, __out Output::CMatrix& out)
	{
		AlphaBlend(1, 4, out.Ref(1, 0), col);
		AlphaBlend(1, 4, out.Ref(0, 1), col);
		AlphaBlend(5, 6, out.Ref(1, 1), col); //[!] fixes 7/8 used in xBR
	}

	virtual void BlendLineShallow (int col, __out Output::CMatrix& out)
	{
		AlphaBlend(1, 4, out.Ref(GetScale() - 1, 0), col);
		AlphaBlend(3, 4, out.Ref(GetScale() - 1, 1), col);
	}

	virtual void BlendLineDiagonal (int col, __out Output::CMatrix& out)
	{
		AlphaBlend(1, 2, out.Ref(1, 1), col);
	}

	virtual void BlendCorner (int col, __out Output::CMatrix& out)
	{
		//model a round corner
		AlphaBlend(21, 100, out.Ref(1, 1), col); //exact: 1 - pi/4 = 0.2146018366
	}
};

class CScaler3X :
	public CScalerBase,
	public IScaler
{
public:
	virtual int GetScale (void)
	{
		return 3;
	}

	virtual void BlendLineSteep (int col, __out Output::CMatrix& out)
	{
		AlphaBlend(1, 4, out.Ref(0, GetScale() - 1), col);
		AlphaBlend(1, 4, out.Ref(2, GetScale() - 2), col);
		AlphaBlend(3, 4, out.Ref(1, GetScale() - 1), col);
		out.Ref(2, GetScale() - 1).Set(col);
	}

	virtual void BlendLineSteepAndShallow (int col, __out Output::CMatrix& out)
	{
		AlphaBlend(1, 4, out.Ref(2, 0), col);
		AlphaBlend(1, 4, out.Ref(0, 2), col);
		AlphaBlend(3, 4, out.Ref(2, 1), col);
		AlphaBlend(3, 4, out.Ref(1, 2), col);
		out.Ref(2, 2).Set(col);
	}

	virtual void BlendLineShallow (int col, __out Output::CMatrix& out)
	{
		AlphaBlend(1, 4, out.Ref(GetScale() - 1, 0), col);
		AlphaBlend(1, 4, out.Ref(GetScale() - 2, 2), col);
		AlphaBlend(3, 4, out.Ref(GetScale() - 1, 1), col);
		out.Ref(GetScale() - 1, 2).Set(col);
	}

	virtual void BlendLineDiagonal (int col, __out Output::CMatrix& out)
	{
		AlphaBlend(1, 8, out.Ref(1, 2), col);
		AlphaBlend(1, 8, out.Ref(2, 1), col);
		AlphaBlend(7, 8, out.Ref(2, 2), col);
	}

	virtual void BlendCorner (int col, __out Output::CMatrix& out)
	{
        //model a round corner
		AlphaBlend(45, 100, out.Ref(2, 2), col); //exact: 0.4545939598
		//alphaBlend(14, 1000, out.ref(2, 1), col); //0.01413008627 -> negligable
		//alphaBlend(14, 1000, out.ref(1, 2), col); //0.01413008627
	}
};

class CScaler4X :
	public CScalerBase,
	public IScaler
{
public:
	virtual int GetScale (void)
	{
		return 4;
	}

	virtual void BlendLineSteep (int col, __out Output::CMatrix& out)
	{
		AlphaBlend(1, 4, out.Ref(0, GetScale() - 1), col);
		AlphaBlend(1, 4, out.Ref(2, GetScale() - 2), col);
		AlphaBlend(3, 4, out.Ref(1, GetScale() - 1), col);
		AlphaBlend(3, 4, out.Ref(3, GetScale() - 2), col);
		out.Ref(2, GetScale() - 1).Set(col);
		out.Ref(3, GetScale() - 1).Set(col);
	}

	virtual void BlendLineSteepAndShallow (int col, __out Output::CMatrix& out)
	{
		AlphaBlend(3, 4, out.Ref(3, 1), col);
		AlphaBlend(3, 4, out.Ref(1, 3), col);
		AlphaBlend(1, 4, out.Ref(3, 0), col);
		AlphaBlend(1, 4, out.Ref(0, 3), col);
		AlphaBlend(1, 3, out.Ref(2, 2), col); //[!] fixes 1/4 used in xBR
		out.Ref(3, 3).Set(col);
		out.Ref(3, 2).Set(col);
		out.Ref(2, 3).Set(col);
	}

	virtual void BlendLineShallow (int col, __out Output::CMatrix& out)
	{
		AlphaBlend(1, 4, out.Ref(GetScale() - 1, 0), col);
		AlphaBlend(1, 4, out.Ref(GetScale() - 2, 2), col);
		AlphaBlend(3, 4, out.Ref(GetScale() - 1, 1), col);
		AlphaBlend(3, 4, out.Ref(GetScale() - 2, 3), col);
		out.Ref(GetScale() - 1, 2).Set(col);
		out.Ref(GetScale() - 1, 3).Set(col);
	}

	virtual void BlendLineDiagonal (int col, __out Output::CMatrix& out)
	{
		AlphaBlend(1, 2, out.Ref(GetScale() - 1, GetScale() / 2), col);
		AlphaBlend(1, 2, out.Ref(GetScale() - 2, GetScale() / 2 + 1), col);
		out.Ref(GetScale() - 1, GetScale() - 1).Set(col);
	}

	virtual void BlendCorner (int col, __out Output::CMatrix& out)
	{
		//model a round corner
		AlphaBlend(68, 100, out.Ref(3, 3), col); //exact: 0.6848532563
		AlphaBlend(9, 100, out.Ref(3, 2), col); //0.08677704501
		AlphaBlend(9, 100, out.Ref(2, 3), col); //0.08677704501
	}
};

class CScaler5X :
	public CScalerBase,
	public IScaler
{
public:
	virtual int GetScale (void)
	{
		return 5;
	}

	virtual void BlendLineSteep (int col, __out Output::CMatrix& out)
	{
		AlphaBlend(1, 4, out.Ref(0, GetScale() - 1), col);
		AlphaBlend(1, 4, out.Ref(2, GetScale() - 2), col);
		AlphaBlend(1, 4, out.Ref(4, GetScale() - 3), col);
		AlphaBlend(3, 4, out.Ref(1, GetScale() - 1), col);
		AlphaBlend(3, 4, out.Ref(3, GetScale() - 2), col);
		out.Ref(2, GetScale() - 1).Set(col);
		out.Ref(3, GetScale() - 1).Set(col);
		out.Ref(4, GetScale() - 1).Set(col);
		out.Ref(4, GetScale() - 2).Set(col);
	}

	virtual void BlendLineSteepAndShallow (int col, __out Output::CMatrix& out)
	{
		AlphaBlend(1, 4, out.Ref(0, GetScale() - 1), col);
		AlphaBlend(1, 4, out.Ref(2, GetScale() - 2), col);
		AlphaBlend(3, 4, out.Ref(1, GetScale() - 1), col);
		AlphaBlend(1, 4, out.Ref(GetScale() - 1, 0), col);
		AlphaBlend(1, 4, out.Ref(GetScale() - 2, 2), col);
		AlphaBlend(3, 4, out.Ref(GetScale() - 1, 1), col);
		out.Ref(2, GetScale() - 1).Set(col);
		out.Ref(3, GetScale() - 1).Set(col);
		out.Ref(GetScale() - 1, 2).Set(col);
		out.Ref(GetScale() - 1, 3).Set(col);
		out.Ref(4, GetScale() - 1).Set(col);
		AlphaBlend(2, 3, out.Ref(3, 3), col);
	}

	virtual void BlendLineShallow (int col, __out Output::CMatrix& out)
	{
		AlphaBlend(1, 4, out.Ref(GetScale() - 1, 0), col);
		AlphaBlend(1, 4, out.Ref(GetScale() - 2, 2), col);
		AlphaBlend(1, 4, out.Ref(GetScale() - 3, 4), col);
		AlphaBlend(3, 4, out.Ref(GetScale() - 1, 1), col);
		AlphaBlend(3, 4, out.Ref(GetScale() - 2, 3), col);
		out.Ref(GetScale() - 1, 2).Set(col);
		out.Ref(GetScale() - 1, 3).Set(col);
		out.Ref(GetScale() - 1, 4).Set(col);
		out.Ref(GetScale() - 2, 4).Set(col);
	}

	virtual void BlendLineDiagonal (int col, __out Output::CMatrix& out)
	{
		AlphaBlend(1, 8, out.Ref(GetScale() - 1, GetScale() / 2), col);
		AlphaBlend(1, 8, out.Ref(GetScale() - 2, GetScale() / 2 + 1), col);
		AlphaBlend(1, 8, out.Ref(GetScale() - 3, GetScale() / 2 + 2), col);
		AlphaBlend(7, 8, out.Ref(4, 3), col);
		AlphaBlend(7, 8, out.Ref(3, 4), col);
		out.Ref(4, 4).Set(col);
	}

	virtual void BlendCorner (int col, __out Output::CMatrix& out)
	{
		//model a round corner
		AlphaBlend(86, 100, out.Ref(4, 4), col); //exact: 0.8631434088
		AlphaBlend(23, 100, out.Ref(4, 3), col); //0.2306749731
		AlphaBlend(23, 100, out.Ref(3, 4), col); //0.2306749731
		//alphaBlend(8, 1000, out.ref(4, 2), col); //0.008384061834 -> negligable
		//alphaBlend(8, 1000, out.ref(2, 4), col); //0.008384061834
	}
};

namespace ScaleSize
{
	CScaler2X Scale2X;
	CScaler3X Scale3X;
	CScaler4X Scale4X;
	CScaler5X Scale5X;

	IScaler* rgScalers[] = { &Scale2X, &Scale3X, &Scale4X, &Scale5X };
}
