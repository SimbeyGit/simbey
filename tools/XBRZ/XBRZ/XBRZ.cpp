#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Blend\BlendType.h"
#include "Blend\BlendInfo.h"
#include "Blend\BlendResult.h"
#include "Color\ColorEq.h"
#include "Scalers\Rot.h"
#include "Scalers\OutputMatrix.h"
#include "Scalers\Implementations.h"
#include "Scalers\Kernels.h"
#include "ScalerCfg.h"

BOOL APIENTRY DllMain (HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch(ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		Rot::Initialize();
		Output::Initialize();
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		Output::Free();
		break;
	}
	return TRUE;
}

class CXBRZScaler
{
private:
	CScalerCfg m_cfg;
	CColorDist m_colorDistance;
	CColorEq m_colorEqualizer;
	CBlendResult m_blendResult;
	IScaler* m_pScaler;
	Output::CMatrix* m_pOutput;

public:
	CXBRZScaler (const CScalerCfg& cfg, IScaler* pScaler) :
		m_cfg(static_cast<const SCALER_CONFIG*>(&cfg)),
		m_colorDistance(&m_cfg),
		m_colorEqualizer(&m_cfg),
		m_pScaler(pScaler),
		m_pOutput(NULL)
	{
	}

	~CXBRZScaler ()
	{
	}

	//fill block with the given color
	static VOID FillBlock (int* trg, int trgi, int pitch, int col, int blockSize)
	{
		for(int y = 0; y < blockSize; ++y, trgi += pitch)
		{
			for(int x = 0; x < blockSize; ++x)
			{
				trg[trgi + x] = col;
			}
		}
	}

	//detect blend direction
	VOID PreProcessCorners (Kernel4x4* pKer)
	{
		m_blendResult.Reset();

		if((pKer->F == pKer->G && pKer->J == pKer->K) || (pKer->F == pKer->J && pKer->G == pKer->K))
			return;

		const int weight = 4;
		double jg = m_colorDistance.DistYCbCr(pKer->I, pKer->F) + m_colorDistance.DistYCbCr(pKer->F, pKer->C) + m_colorDistance.DistYCbCr(pKer->N, pKer->K) + m_colorDistance.DistYCbCr(pKer->K, pKer->H) + weight * m_colorDistance.DistYCbCr(pKer->J, pKer->G);
		double fk = m_colorDistance.DistYCbCr(pKer->E, pKer->J) + m_colorDistance.DistYCbCr(pKer->J, pKer->O) + m_colorDistance.DistYCbCr(pKer->B, pKer->G) + m_colorDistance.DistYCbCr(pKer->G, pKer->L) + weight * m_colorDistance.DistYCbCr(pKer->F, pKer->K);

		if(jg < fk)
		{
			bool dominantGradient = m_cfg.DominantDirectionThreshold * jg < fk;
			if(pKer->F != pKer->G && pKer->F != pKer->J)
			{
				m_blendResult.F = (short)(dominantGradient ? BlendType::Dominant : BlendType::Normal);
			}
			if(pKer->K != pKer->J && pKer->K != pKer->G)
			{
				m_blendResult.K = (short)(dominantGradient ? BlendType::Dominant : BlendType::Normal);
			}
		}
		else if(fk < jg)
		{
			bool dominantGradient = m_cfg.DominantDirectionThreshold * fk < jg;
			if(pKer->J != pKer->F && pKer->J != pKer->K)
			{
				m_blendResult.J = (short)(dominantGradient ? BlendType::Dominant : BlendType::Normal);
			}
			if(pKer->G != pKer->F && pKer->G != pKer->K)
			{
				m_blendResult.G = (short)(dominantGradient ? BlendType::Dominant : BlendType::Normal);
			}
		}
	}

	/*
		input kernel area naming convention:
		-------------
		| A | B | C |
		----|---|---|
		| D | E | F | //input pixel is at position E
		----|---|---|
		| G | H | I |
		-------------
		blendInfo: result of preprocessing all four corners of pixel "e"
	*/
	void ScalePixel (int rotDeg, Kernel3x3* pKer, int trgi, short blendInfo)
	{
		// int a = pKer->m_data[Rot::data[(0 << 2) + rotDeg]];
		int b = pKer->m_data[Rot::data[(1 << 2) + rotDeg]];
		int c = pKer->m_data[Rot::data[(2 << 2) + rotDeg]];
		int d = pKer->m_data[Rot::data[(3 << 2) + rotDeg]];
		int e = pKer->m_data[Rot::data[(4 << 2) + rotDeg]];
		int f = pKer->m_data[Rot::data[(5 << 2) + rotDeg]];
		int g = pKer->m_data[Rot::data[(6 << 2) + rotDeg]];
		int h = pKer->m_data[Rot::data[(7 << 2) + rotDeg]];
		int i = pKer->m_data[Rot::data[(8 << 2) + rotDeg]];

		short blend = BlendInfo::Rotate(blendInfo, static_cast<Rotation::Degree>(rotDeg));
		if(static_cast<BlendType::Value>(BlendInfo::GetBottomR(blend)) == BlendType::None) return;

		CColorEq& eq = m_colorEqualizer;
		CColorDist& dist = m_colorDistance;

		bool doLineBlend;

		if(BlendInfo::GetBottomR(blend) >= static_cast<short>(BlendType::Dominant))
			doLineBlend = true;
		//make sure there is no second blending in an adjacent
		//rotation for this pixel: handles insular pixels, mario eyes
		//but support double-blending for 90 degree corners
		else if(BlendInfo::GetTopR(blend) != static_cast<short>(BlendType::None) && !eq.IsColorEqual(e, g))
			doLineBlend = false;
		else if(BlendInfo::GetBottomL(blend) != static_cast<short>(BlendType::None) && !eq.IsColorEqual(e, c))
			doLineBlend = false;
		//no full blending for L-shapes; blend corner only (handles "mario mushroom eyes")
		else if(eq.IsColorEqual(g, h) && eq.IsColorEqual(h, i) && eq.IsColorEqual(i, f) && eq.IsColorEqual(f, c) && !eq.IsColorEqual(e, i))
			doLineBlend = false;
		else
			doLineBlend = true;

		//choose most similar color
		int px = dist.DistYCbCr(e, f) <= dist.DistYCbCr(e, h) ? f : h;

		Output::CMatrix& output = *m_pOutput;
		output.Move(rotDeg, trgi);

		if(!doLineBlend)
		{
			m_pScaler->BlendCorner(px, output);
			return;
		}

		//test sample: 70% of values max(fg, hc) / min(fg, hc)
		//are between 1.1 and 3.7 with median being 1.9
		double fg = dist.DistYCbCr(f, g);
		double hc = dist.DistYCbCr(h, c);

		bool haveShallowLine = m_cfg.SteepDirectionThreshold * fg <= hc && e != g && d != g;
		bool haveSteepLine = m_cfg.SteepDirectionThreshold * hc <= fg && e != c && b != c;

		if(haveShallowLine)
		{
			if(haveSteepLine)
				m_pScaler->BlendLineSteepAndShallow(px, output);
			else
				m_pScaler->BlendLineShallow(px, output);
		}
		else
		{
			if(haveSteepLine)
				m_pScaler->BlendLineSteep(px, output);
			else
				m_pScaler->BlendLineDiagonal(px, output);
		}
	}

	HRESULT ScaleImage (const INT* src, const SIZE* pcszImage, INT yFirst, INT yLast, INT* trg)
	{
		HRESULT hr;
		SHORT* preProcBuffer = NULL;
		Kernel4x4 ker4;
		Kernel3x3 ker3;

		yFirst = max(yFirst, 0);
		yLast = min(yLast, pcszImage->cy);

		CheckIf(yFirst >= yLast || pcszImage->cx <= 0, E_INVALIDARG);

		INT trgWidth = pcszImage->cx * m_pScaler->GetScale();

		//temporary buffer for "on the fly preprocessing"
		preProcBuffer = new SHORT[pcszImage->cx];

		//initialize preprocessing buffer for first row:
		//detect upper left and right corner blending
		//this cannot be optimized for adjacent processing
		//stripes; we must not allow for a memory race condition!
		if(yFirst > 0)
		{
			INT y = yFirst - 1;

			INT sM1 = pcszImage->cx * max(y - 1, 0);
			INT s0 = pcszImage->cx * y; //center line
			INT sP1 = pcszImage->cx * min(y + 1, pcszImage->cy - 1);
			INT sP2 = pcszImage->cx * min(y + 2, pcszImage->cy - 1);

			for(LONG x = 0; x < pcszImage->cx; x++)
			{
				INT xM1 = max(x - 1, 0);
				INT xP1 = min(x + 1, pcszImage->cx - 1);
				INT xP2 = min(x + 2, pcszImage->cy - 1);

				//read sequentially from memory as far as possible
				ker4.A = src[sM1 + xM1];
				ker4.B = src[sM1 + x];
				ker4.C = src[sM1 + xP1];
				ker4.D = src[sM1 + xP2];

				ker4.E = src[s0 + xM1];
				ker4.F = src[s0 + x];
				ker4.G = src[s0 + xP1];
				ker4.H = src[s0 + xP2];

				ker4.I = src[sP1 + xM1];
				ker4.J = src[sP1 + x];
				ker4.K = src[sP1 + xP1];
				ker4.L = src[sP1 + xP2];

				ker4.M = src[sP2 + xM1];
				ker4.N = src[sP2 + x];
				ker4.O = src[sP2 + xP1];
				ker4.P = src[sP2 + xP2];

				PreProcessCorners(&ker4); // writes to m_blendResult
				/*
				preprocessing blend result:
				---------
				| F | G | //evaluate corner between F, G, J, K
				----|---| //input pixel is at position F
				| J | K |
				---------
				*/
				preProcBuffer[x] = BlendInfo::SetTopR(preProcBuffer[x], m_blendResult.J);

				if(x + 1 < pcszImage->cx)
					preProcBuffer[x + 1] = BlendInfo::SetTopL(preProcBuffer[x + 1], m_blendResult.K);
			}
		}

		m_pOutput = __new Output::CMatrix(m_pScaler->GetScale(), trg, trgWidth);
		CheckAlloc(m_pOutput);

		for(INT y = yFirst; y < yLast; y++)
		{
			//consider MT "striped" access
			int trgi = m_pScaler->GetScale() * y * trgWidth;

			int sM1 = pcszImage->cx * max(y - 1, 0);
			int s0 = pcszImage->cx * y; //center line
			int sP1 = pcszImage->cx * min(y + 1, pcszImage->cy - 1);
			int sP2 = pcszImage->cx * min(y + 2, pcszImage->cy - 1);

			short blendXy1 = (short)0;

			for(int x = 0; x < pcszImage->cx; ++x, trgi += m_pScaler->GetScale())
			{
				int xM1 = max(x - 1, 0);
				int xP1 = min(x + 1, pcszImage->cx - 1);
				int xP2 = min(x + 2, pcszImage->cx - 1);

				//evaluate the four corners on bottom-right of current pixel
				//blend_xy for current (x, y) position

				//read sequentially from memory as far as possible
				ker4.A = src[sM1 + xM1];
				ker4.B = src[sM1 + x];
				ker4.C = src[sM1 + xP1];
				ker4.D = src[sM1 + xP2];

				ker4.E = src[s0 + xM1];
				ker4.F = src[s0 + x];
				ker4.G = src[s0 + xP1];
				ker4.H = src[s0 + xP2];

				ker4.I = src[sP1 + xM1];
				ker4.J = src[sP1 + x];
				ker4.K = src[sP1 + xP1];
				ker4.L = src[sP1 + xP2];

				ker4.M = src[sP2 + xM1];
				ker4.N = src[sP2 + x];
				ker4.O = src[sP2 + xP1];
				ker4.P = src[sP2 + xP2];

				PreProcessCorners(&ker4); // writes to blendResult

				/*
					preprocessing blend result:
					---------
					| F | G | //evaluate corner between F, G, J, K
					----|---| //current input pixel is at position F
					| J | K |
					---------
				*/

				//all four corners of (x, y) have been determined at
				//this point due to processing sequence!
				short blendXy = BlendInfo::SetBottomR(preProcBuffer[x], m_blendResult.F);

				//set 2nd known corner for (x, y + 1)
				blendXy1 = BlendInfo::SetTopR(blendXy1, m_blendResult.J);
				//store on current buffer position for use on next row
				preProcBuffer[x] = blendXy1;

				//set 1st known corner for (x + 1, y + 1) and
				//buffer for use on next column
				blendXy1 = BlendInfo::SetTopL((short)0, m_blendResult.K);

				if(x + 1 < pcszImage->cx)
				{
					//set 3rd known corner for (x + 1, y)
					preProcBuffer[x + 1] = BlendInfo::SetBottomL(preProcBuffer[x + 1], m_blendResult.G);
				}

				//fill block of size scale * scale with the given color
				//  //place *after* preprocessing step, to not overwrite the
				//  //results while processing the the last pixel!
				FillBlock(trg, trgi, trgWidth, src[s0 + x], m_pScaler->GetScale());

				//blend four corners of current pixel
				if(blendXy == 0) continue;

				const int a = 0, b = 1, c = 2, d = 3, e = 4, f = 5, g = 6, h = 7, i = 8;

				//read sequentially from memory as far as possible
				ker3.m_data[a] = src[sM1 + xM1];
				ker3.m_data[b] = src[sM1 + x];
				ker3.m_data[c] = src[sM1 + xP1];

				ker3.m_data[d] = src[s0 + xM1];
				ker3.m_data[e] = src[s0 + x];
				ker3.m_data[f] = src[s0 + xP1];

				ker3.m_data[g] = src[sP1 + xM1];
				ker3.m_data[h] = src[sP1 + x];
				ker3.m_data[i] = src[sP1 + xP1];

				ScalePixel((int)Rotation::R0, &ker3, trgi, blendXy);
				ScalePixel((int)Rotation::R90, &ker3, trgi, blendXy);
				ScalePixel((int)Rotation::R180, &ker3, trgi, blendXy);
				ScalePixel((int)Rotation::R270, &ker3, trgi, blendXy);
		   }
		}

		hr = S_OK;

	Cleanup:
		SafeDelete(m_pOutput);
		SafeDeleteArray(preProcBuffer);
		return hr;
	}
};

HRESULT ScaleImageUsingConfig (const CScalerCfg& cfg, const INT* pcnImage, const SIZE* pcszImage, INT nScaleSize, __deref_out INT** ppnOutput, __out SIZE* pszOutput)
{
	HRESULT hr;
	INT* pnOutput = NULL;

	CheckIf(0 > nScaleSize || nScaleSize > 3, E_INVALIDARG);

	{
		INT nScaleFactor = nScaleSize + 2;
		CXBRZScaler scaler(cfg, ScaleSize::rgScalers[nScaleSize]);

		CheckIf(NULL == pcnImage, E_INVALIDARG);
		CheckIf(NULL == pcszImage, E_INVALIDARG);
		CheckIf(NULL == pszOutput, E_INVALIDARG);

		pszOutput->cx = pcszImage->cx * nScaleFactor;
		pszOutput->cy = pcszImage->cy * nScaleFactor;

		INT cpEnlarged = pszOutput->cx * pszOutput->cy;
		pnOutput = __new INT[cpEnlarged];
		CheckAlloc(pnOutput);

		Check(scaler.ScaleImage(pcnImage, pcszImage, 0, INT_MAX, pnOutput));

		*ppnOutput = pnOutput;
		pnOutput = NULL;
	}

Cleanup:
	SafeDeleteArray(pnOutput);
	return hr;
}

HRESULT XBRZScaleImage (__in_opt const SCALER_CONFIG* pcConfig, const INT* pcnImage, const SIZE* pcszImage, INT nScaleSize, __deref_out INT** ppnOutput, __out SIZE* pszOutput)
{
	if(pcConfig)
	{
		CScalerCfg cfgUser(pcConfig);
		return ScaleImageUsingConfig(cfgUser, pcnImage, pcszImage, nScaleSize, ppnOutput, pszOutput);
	}
	else
	{
		CScalerCfg cfgDefault;
		return ScaleImageUsingConfig(cfgDefault, pcnImage, pcszImage, nScaleSize, ppnOutput, pszOutput);
	}
}

VOID XBRZFreeImage (__in_opt INT* pnImage)
{
	__delete_array pnImage;
}

VOID XBRZGetDefaults (__out SCALER_CONFIG* pConfig)
{
	if(pConfig)
	{
		CScalerCfg cfgDefault;
		pConfig->DominantDirectionThreshold = cfgDefault.DominantDirectionThreshold;
		pConfig->EqualColorTolerance = cfgDefault.EqualColorTolerance;
		pConfig->LuminanceWeight = cfgDefault.LuminanceWeight;
		pConfig->SteepDirectionThreshold = cfgDefault.SteepDirectionThreshold;
	}
}
