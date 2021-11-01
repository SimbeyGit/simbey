#include <Windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Core\StringCore.h"
#include "Library\GraphCtrl.h"
#include "NeuralAPI.h"
#include "LogicOr.h"

CLogicOr::CLogicOr ()
{
}

CLogicOr::~CLogicOr ()
{
}

// INetCycleProcessor

VOID CLogicOr::CheckThresholds (VOID)
{
	if(m_fThreshold <= m_pfAccumulators[0] || m_fThreshold <= m_pfAccumulators[1])
		m_dwState |= NSTATE_EXCITED;
	ZeroMemory(m_pfAccumulators, sizeof(FLOAT) * GetInputPinCount());
}

// INetObject

HRESULT CLogicOr::GetObjectClass (LPSTR lpszClass, INT cchMaxClass, __out INT* pcchClass)
{
	return TStrCchCpyLen(lpszClass, cchMaxClass, SLP("CLogicOr"), pcchClass);
}

// INetDocObject

VOID CLogicOr::DrawForeground (IGrapher* lpGraph)
{
	FLOAT fx[6];
	FLOAT fy[6];
	POINT pt[6];

	CLogicGate::DrawForeground(lpGraph);
	CalculateShape(fx, fy);
	for(INT i = 0; i < ARRAYSIZE(pt); i++)
		lpGraph->GraphToClientPoint(fx[i], fy[i], pt[i]);
	Polygon(lpGraph->GetClientDC(), pt, ARRAYSIZE(pt));
}

ULONG CLogicOr::GetInputPinCount (VOID)
{
	return 2;
}

VOID CLogicOr::CalculateShape (FLOAT* fx, FLOAT* fy)
{
	INT xAxon, yAxon;
	ULONG nLastPin = GetInputPinCount() - 1;

	GetAxonPosition(xAxon, yAxon);

	fx[0] = (FLOAT)(m_x + m_ptInput[0].x + AXON_RADIUS + 1);
	fy[0] = (FLOAT)(m_y + m_ptInput[0].y + AXON_RADIUS);
	fx[2] = (FLOAT)(m_x + m_ptInput[nLastPin].x + AXON_RADIUS + 1);
	fy[2] = (FLOAT)(m_y + m_ptInput[nLastPin].y - AXON_RADIUS);

	fx[1] = fx[0] + 3.0f;
	fy[1] = (FLOAT)m_y;

	fx[4] = (FLOAT)(xAxon - AXON_RADIUS - 1);
	fy[4] = (FLOAT)yAxon;

	fx[3] = fx[2] + (fx[4] - fx[2]) / 1.25f;
	fy[3] = fy[2] + 1;

	fx[5] = fx[3];
	fy[5] = fy[0] - 1;
}
