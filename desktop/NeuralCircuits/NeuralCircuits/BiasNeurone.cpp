#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Core\StringCore.h"
#include "NeuralAPI.h"
#include "BiasNeurone.h"

CBiasNeurone::CBiasNeurone () :
	m_rBias(0.0f)
{
}

CBiasNeurone::~CBiasNeurone ()
{
}

// IUnknown

HRESULT WINAPI CBiasNeurone::QueryInterface (REFIID iid, LPVOID* lplpvObject)
{
	HRESULT hr;

	if(iid == __uuidof(IBiasNeurone))
	{
		*lplpvObject = static_cast<IBiasNeurone*>(this);
		AddRef();
		hr = S_OK;
	}
	else
		hr = __super::QueryInterface(iid, lplpvObject);

	return hr;
}

ULONG WINAPI CBiasNeurone::AddRef (VOID)
{
	return __super::AddRef();
}

ULONG WINAPI CBiasNeurone::Release (VOID)
{
	return __super::Release();
}

// INetCycleProcessor

VOID CBiasNeurone::SendPulses (VOID)
{
	__super::SendPulses();
}

VOID CBiasNeurone::CheckThresholds (VOID)
{
	m_fAccumulator += m_rBias;
	__super::CheckThresholds();
}

// INetObject

HRESULT CBiasNeurone::GetObjectClass (LPSTR lpszClass, INT cchMaxClass, __out INT* pcchClass)
{
	return TStrCchCpyLen(lpszClass, cchMaxClass, SLP("CBiasNeurone"), pcchClass);
}

VOID CBiasNeurone::GetPosition (INT& x, INT& y)
{
	return __super::GetPosition(x, y);
}

VOID CBiasNeurone::ReceiveValue (FLOAT fValue, ULONG iPin)
{
	return __super::ReceiveValue(fValue, iPin);
}

ULONG CBiasNeurone::GetInputPin (INT x, INT y)
{
	return __super::GetInputPin(x, y);
}

BOOL CBiasNeurone::GetInputPinPosition (ULONG iPin, INT& x, INT& y)
{
	return __super::GetInputPinPosition(iPin, x, y);
}

HRESULT CBiasNeurone::Load (INeuralFactory* pFactory, LPNLOADDATA lpLoadData, DWORD cLoadData, LPBYTE lpData, ULONG cbData)
{
	CopyMemory(&m_rBias, lpData, sizeof(m_rBias));
	return __super::Load(pFactory, lpLoadData, cLoadData, lpData + sizeof(m_rBias), cbData - sizeof(m_rBias));
}

HRESULT CBiasNeurone::Save (INeuralNet* lpNet, ISequentialStream* lpStream)
{
	HRESULT hr;
	ULONG cb;

	Check(lpStream->Write(&m_rBias, sizeof(m_rBias), &cb));
	Check(__super::Save(lpNet, lpStream));

Cleanup:
	return hr;
}
