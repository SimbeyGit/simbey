#pragma once

#include "..\Common\IntPair.h"
#include "..\Common\IntPtr.h"
#include "Rot.h"

namespace Output
{
	const int MaxScale = 5;	// Highest possible scale
	const int MaxScaleSquared = MaxScale * MaxScale;

	CIntPair* rgMatrixRotations[(MaxScale - 1) * MaxScaleSquared * Rot::MaxRotations];

	CIntPair* BuildMatrixRotation (int rotDeg, int i, int j, int n)
	{
		int iOld, jOld;

		if(rotDeg == 0)
		{
			iOld = i;
			jOld = j;
		}
		else
		{
			//old coordinates before rotation!
			CIntPair* pOld = BuildMatrixRotation(rotDeg - 1, i, j, n);
			iOld = n - 1 - pOld->J;
			jOld = pOld->I;
			__delete pOld;
		}

		return __new CIntPair(iOld, jOld);
	}

	HRESULT Initialize (VOID)
	{
		HRESULT hr = S_OK;

		for(int n = 2; n <= MaxScale; n++)
		{
			for(int r = 0; r < Rot::MaxRotations; r++)
			{
				int nr = (n - 2) * (Rot::MaxRotations * MaxScaleSquared) + r * MaxScaleSquared;
				for(int i = 0; i < MaxScale; i++)
				{
					for(int j = 0; j < MaxScale; j++)
					{
						CIntPair* pRotation = BuildMatrixRotation(r, i, j, n);
						if(NULL == pRotation)
						{
							hr = E_OUTOFMEMORY;
							goto Cleanup;
						}
						rgMatrixRotations[nr + i * MaxScale + j] = pRotation;
					}
				}
			}
		}

	Cleanup:
		return hr;
	}

	VOID Free (VOID)
	{
		for(int i = 0; i < ARRAYSIZE(rgMatrixRotations); i++)
		{
			__delete rgMatrixRotations[i];
			rgMatrixRotations[i] = NULL;
		}
	}

	class CMatrix
	{
	private:
		CIntPtr m_out;
		int m_nOutWidth;
		int m_n;
		int m_nOutI;
		int m_nr;

	public:
		CMatrix (int nScale, int* pnPtr, int nOutWidth) :
			m_out(pnPtr),
			m_n((nScale - 2) * Rot::MaxRotations * MaxScaleSquared),
			m_nOutWidth(nOutWidth)
		{
		}

		void Move (int nRotDeg, int nOutI)
		{
			m_nr = m_n + nRotDeg * MaxScaleSquared;
			m_nOutI = nOutI;
		}

		CIntPtr& Ref (int i, int j)
		{
			CIntPair* pRot = rgMatrixRotations[m_nr + i * MaxScale + j];
			m_out.Position(m_nOutI + pRot->J + pRot->I * m_nOutWidth);
			return m_out;
		}
	};
}
