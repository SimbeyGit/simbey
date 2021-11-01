#include <Windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Core\StringCore.h"
#include "Library\GraphCtrl.h"
#include "NeuralAPI.h"
#include "LogicNot.h"

CLogicNot::CLogicNot ()
{
}

CLogicNot::~CLogicNot ()
{
}

// INetCycleProcessor

VOID CLogicNot::CheckThresholds (VOID)
{
	if(m_fThreshold > m_pfAccumulators[0])
		m_dwState |= NSTATE_EXCITED;
	m_pfAccumulators[0] = 0.0f;
}

// INetObject

HRESULT CLogicNot::GetObjectClass (LPSTR lpszClass, INT cchMaxClass, __out INT* pcchClass)
{
	return TStrCchCpyLen(lpszClass, cchMaxClass, SLP("CLogicNot"), pcchClass);
}

// INetDocObject

VOID CLogicNot::DrawForeground (IGrapher* lpGraph)
{
	POINT pt[3];
	INT xAxon, yAxon;
	FLOAT fx[3];
	FLOAT fy[3];

	GetAxonPosition(xAxon, yAxon);

	CLogicGate::DrawForeground(lpGraph);

	fx[0] = (FLOAT)(m_x + m_ptInput[0].x + AXON_RADIUS + 1);
	fy[0] = (FLOAT)(m_y + m_ptInput[0].y + AXON_RADIUS * 2);

	fx[1] = fx[0];
	fy[1] = (FLOAT)(m_y + m_ptInput[0].y - AXON_RADIUS * 2);

	fx[2] = xAxon - AXON_RADIUS;
	fy[2] = yAxon;

	for(INT i = 0; i < ARRAYSIZE(pt); i++)
		lpGraph->GraphToClientPoint(fx[i], fy[i], pt[i]);
	Polygon(lpGraph->GetClientDC(), pt, ARRAYSIZE(pt));
}

ULONG CLogicNot::GetInputPinCount (VOID)
{
	return 1;
}
