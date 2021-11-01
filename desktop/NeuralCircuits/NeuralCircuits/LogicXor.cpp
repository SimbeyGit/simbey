#include <Windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Core\StringCore.h"
#include "Library\Util\Formatting.h"
#include "Library\GraphCtrl.h"
#include "NeuralAPI.h"
#include "LogicXor.h"

///////////////////////////////////////////////////////////////////////////////
// CLogicXor
///////////////////////////////////////////////////////////////////////////////

CLogicXor::CLogicXor ()
{
}

CLogicXor::~CLogicXor ()
{
}

// INetCycleProcessor

VOID CLogicXor::CheckThresholds (VOID)
{
	if((m_fThreshold <= m_pfAccumulators[0]) ^ (m_fThreshold <= m_pfAccumulators[1]))
		m_dwState |= NSTATE_EXCITED;
	ZeroMemory(m_pfAccumulators, sizeof(FLOAT) * GetInputPinCount());
}

// INetObject

HRESULT CLogicXor::GetObjectClass (LPSTR lpszClass, INT cchMaxClass, __out INT* pcchClass)
{
	return TStrCchCpyLen(lpszClass, cchMaxClass, SLP("CLogicXor"), pcchClass);
}

// INetDocObject

VOID CLogicXor::DrawForeground (IGrapher* lpGraph)
{
	FLOAT fx[6];
	FLOAT fy[6];
	POINT pt[6];

	CLogicGate::DrawForeground(lpGraph);
	CalculateShape(fx, fy);

	lpGraph->MoveTo(fx[0] - 1.0f, fy[0], 0.0f);
	lpGraph->LineTo(fx[1] - 1.0f, fy[1], 0.0f);
	lpGraph->LineTo(fx[2] - 1.0f, fy[2], 0.0f);

	fx[0] += 1.0f;
	fx[1] += 1.0f;
	fx[2] += 1.0f;

	for(INT i = 0; i < ARRAYSIZE(pt); i++)
		lpGraph->GraphToClientPoint(fx[i], fy[i], pt[i]);
	Polygon(lpGraph->GetClientDC(), pt, ARRAYSIZE(pt));
}

///////////////////////////////////////////////////////////////////////////////
// CLogicXorN
///////////////////////////////////////////////////////////////////////////////

CLogicXorN::CLogicXorN (ULONG cPins) : m_cPins(cPins)
{
}

CLogicXorN::~CLogicXorN ()
{
}

// INetCycleProcessor

VOID CLogicXorN::CheckThresholds (VOID)
{
	ULONG cHot = 0;

	for(ULONG i = 0; i < m_cPins; i++)
	{
		if(m_fThreshold <= m_pfAccumulators[i])
			cHot++;
	}

	if(1 & cHot)
		m_dwState |= NSTATE_EXCITED;

	ZeroMemory(m_pfAccumulators, sizeof(FLOAT) * GetInputPinCount());
}

// INetObject

HRESULT CLogicXorN::GetObjectClass (LPSTR lpszClass, INT cchMaxClass, __out INT* pcchClass)
{
	return Formatting::TPrintF(lpszClass, cchMaxClass, pcchClass, "CLogicXor%d", m_cPins);
}
