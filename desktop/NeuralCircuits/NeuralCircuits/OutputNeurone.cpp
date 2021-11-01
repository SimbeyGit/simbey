#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Core\StringCore.h"
#include "Library\Util\Formatting.h"
#include "Library\GraphCtrl.h"
#include "NeuralAPI.h"
#include "GdiList.h"
#include "OutputNeurone.h"

COutputNeurone::COutputNeurone ()
{
}

COutputNeurone::~COutputNeurone ()
{
}

// INetObject

HRESULT COutputNeurone::GetObjectClass (LPSTR lpszClass, INT cchMaxClass, __out INT* pcchClass)
{
	return TStrCchCpyLen(lpszClass, cchMaxClass, SLP("COutputNeurone"), pcchClass);
}

VOID COutputNeurone::GetPosition (INT& x, INT& y)
{
	x = m_x;
	y = m_y;
}

VOID COutputNeurone::ReceiveValue (FLOAT fValue, ULONG iPin)
{
	UNREFERENCED_PARAMETER(iPin);

	if(m_lpLink)
		m_lpLink->OutputValue(fValue, m_iPin);
	else if(m_lpParent)
		m_lpParent->ReceiveOutputValue(fValue, m_iPin);
}

ULONG COutputNeurone::GetInputPin (INT x, INT y)
{
	return CNeurone::GetInputPin(x,y);
}

BOOL COutputNeurone::GetInputPinPosition (ULONG iPin, INT& x, INT& y)
{
	return CNeurone::GetInputPinPosition(iPin,x,y);
}

// INeurone

BOOL COutputNeurone::GetCurrentValue (FLOAT& fValue)
{
	UNREFERENCED_PARAMETER(fValue);

	return FALSE;
}

// INetDocObject

VOID COutputNeurone::DrawForeground (IGrapher* lpGraph)
{
	CHAR szPin[32];
	INT cchPin, xStart = m_x + DEFAULT_RADIUS;
	HBRUSH hbrDef;
	HFONT hfDef;

	CNeurone::DrawForeground(lpGraph);

	lpGraph->MoveTo((FLOAT)xStart,(FLOAT)m_y,0.0f);
	lpGraph->LineTo((FLOAT)(xStart + 5),(FLOAT)m_y,0.0f);
	lpGraph->LineTo((FLOAT)(xStart + 5),(FLOAT)(m_y + 5),0.0f);
	lpGraph->LineTo((FLOAT)(xStart + 2),(FLOAT)(m_y + 2),0.0f);
	lpGraph->MoveTo((FLOAT)(xStart + 5),(FLOAT)(m_y + 5),0.0f);
	lpGraph->LineTo((FLOAT)(xStart + 8),(FLOAT)(m_y + 2),0.0f);

	Formatting::TUInt32ToAsc(m_iPin, szPin, ARRAYSIZE(szPin), 10, &cchPin);

	hbrDef = lpGraph->SelectBrush(GdiList::hbrPinLabel);
	hfDef = lpGraph->SelectFont(GdiList::hfLabels);
	lpGraph->LabelText((FLOAT)(m_x - 5),(FLOAT)(m_y - 5),0.0f,szPin,cchPin);
	lpGraph->SelectFont(hfDef);
	lpGraph->SelectBrush(hbrDef);
}

// IIONeurone

ULONG COutputNeurone::GetPin (VOID)
{
	return m_iPin;
}

VOID COutputNeurone::SetPin (ULONG iPin)
{
	m_iPin = iPin;
}

EIO_TYPE COutputNeurone::GetIOType (VOID)
{
	return (m_lpLink) ? OUTPUT_LINK : OUTPUT_NEURONE;
}

BOOL COutputNeurone::GetAxonPosition (INT& x, INT& y)
{
	UNREFERENCED_PARAMETER(x);
	UNREFERENCED_PARAMETER(y);

	return FALSE;
}