#include <Windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Core\StringCore.h"
#include "Library\GraphCtrl.h"
#include "NeuralAPI.h"
#include "LogicAnd.h"

CLogicAnd::CLogicAnd ()
{
}

CLogicAnd::~CLogicAnd ()
{
}

// INetCycleProcessor

VOID CLogicAnd::CheckThresholds (VOID)
{
	if(m_fThreshold <= m_pfAccumulators[0] && m_fThreshold <= m_pfAccumulators[1])
		m_dwState |= NSTATE_EXCITED;
	ZeroMemory(m_pfAccumulators, sizeof(FLOAT) * GetInputPinCount());
}

// INetObject

HRESULT CLogicAnd::GetObjectClass (LPSTR lpszClass, INT cchMaxClass, __out INT* pcchClass)
{
	return TStrCchCpyLen(lpszClass, cchMaxClass, SLP("CLogicAnd"), pcchClass);
}

// INetDocObject

VOID CLogicAnd::DrawForeground (IGrapher* lpGraph)
{
	POINT pt[5];
	INT xAxon, yAxon;
	FLOAT fx[5];
	FLOAT fy[5];

	GetAxonPosition(xAxon, yAxon);

	CLogicGate::DrawForeground(lpGraph);

	fx[0] = (FLOAT)(m_x + m_ptInput[0].x + AXON_RADIUS + 1);
	fy[0] = (FLOAT)(m_y + m_ptInput[0].y + AXON_RADIUS);
	fx[1] = (FLOAT)(m_x + m_ptInput[1].x + AXON_RADIUS + 1);
	fy[1] = (FLOAT)(m_y + m_ptInput[1].y - AXON_RADIUS);

	fx[3] = (FLOAT)(xAxon - AXON_RADIUS - 1);
	fy[3] = (FLOAT)yAxon;

	fx[2] = fx[1] + (fx[3] - fx[1]) / 1.25f;
	fy[2] = fy[1] + 1;

	fx[4] = fx[2];
	fy[4] = fy[0] - 1;

	for(INT i = 0; i < ARRAYSIZE(pt); i++)
		lpGraph->GraphToClientPoint(fx[i], fy[i], pt[i]);
	Polygon(lpGraph->GetClientDC(), pt, ARRAYSIZE(pt));
}

ULONG CLogicAnd::GetInputPinCount (VOID)
{
	return 2;
}
