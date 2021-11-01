#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Core\StringCore.h"
#include "Library\Util\Formatting.h"
#include "Library\GraphCtrl.h"
#include "NeuralAPI.h"
#include "GdiList.h"
#include "InputNeurone.h"

CInputNeurone::CInputNeurone ()
{
}

CInputNeurone::~CInputNeurone ()
{
}

// INetObject

HRESULT CInputNeurone::GetObjectClass (LPSTR lpszClass, INT cchMaxClass, __out INT* pcchClass)
{
	return TStrCchCpyLen(lpszClass, cchMaxClass, SLP("CInputNeurone"), pcchClass);
}

VOID CInputNeurone::GetPosition (INT& x, INT& y)
{
	x = m_x;
	y = m_y;
}

VOID CInputNeurone::ReceiveValue (FLOAT fValue, ULONG iPin)
{
	UNREFERENCED_PARAMETER(iPin);

	Assert(iPin == m_iPin);

    m_Connections.SendPulses(fValue);
}

ULONG CInputNeurone::GetInputPin (INT x, INT y)
{
	UNREFERENCED_PARAMETER(x);
	UNREFERENCED_PARAMETER(y);

	return 0xFFFFFFFF;
}

BOOL CInputNeurone::GetInputPinPosition (ULONG iPin, INT& x, INT& y)
{
	UNREFERENCED_PARAMETER(iPin);
	UNREFERENCED_PARAMETER(x);
	UNREFERENCED_PARAMETER(y);

	return FALSE;
}

// INeurone

BOOL CInputNeurone::GetCurrentValue (FLOAT& fValue)
{
	UNREFERENCED_PARAMETER(fValue);

	return FALSE;
}

// INetDocObject

VOID CInputNeurone::DrawForeground (IGrapher* lpGraph)
{
	CHAR szPin[32];
	INT cchPin;
	HBRUSH hbrDef;

	CNeurone::DrawForeground(lpGraph);

	Formatting::TUInt32ToAsc(m_iPin, szPin, ARRAYSIZE(szPin), 10, &cchPin);

	hbrDef = lpGraph->SelectBrush(GdiList::hbrPinLabel);
	lpGraph->LabelText((FLOAT)(m_x - 5),(FLOAT)(m_y - 5),0.0f,szPin,cchPin);
	lpGraph->SelectBrush(hbrDef);
}

// IIONeurone

ULONG CInputNeurone::GetPin (VOID)
{
	return m_iPin;
}

VOID CInputNeurone::SetPin (ULONG iPin)
{
	m_iPin = iPin;
}

EIO_TYPE CInputNeurone::GetIOType (VOID)
{
	return (m_lpLink) ? INPUT_LINK : INPUT_NEURONE;
}