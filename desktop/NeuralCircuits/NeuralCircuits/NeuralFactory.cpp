#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Core\StringCore.h"
#include "Library\Util\Formatting.h"
#include "NeuralAPI.h"
#include "InputPad.h"
#include "Splitter.h"
#include "InputNeurone.h"
#include "OutputNeurone.h"
#include "LogicAnd.h"
#include "LogicOr.h"
#include "LogicNot.h"
#include "LogicXor.h"
#include "BiasNeurone.h"
#include "SigmoidNeurone.h"
#include "NeuralChip.h"
#include "NeuralFrame.h"
#include "NeuralFactory.h"

CNeuralFactory::CNeuralFactory ()
{
	m_cRef = 1;
}

CNeuralFactory::~CNeuralFactory ()
{
}

// IUnknown

HRESULT WINAPI CNeuralFactory::QueryInterface (REFIID iid, LPVOID* lplpvObject)
{
	UNREFERENCED_PARAMETER(iid);
	UNREFERENCED_PARAMETER(lplpvObject);

	return E_NOTIMPL;
}

ULONG WINAPI CNeuralFactory::AddRef (VOID)
{
	return (ULONG)InterlockedIncrement((LONG*)&m_cRef);
}

ULONG WINAPI CNeuralFactory::Release (VOID)
{
	ULONG c = (ULONG)InterlockedDecrement((LONG*)&m_cRef);
	if(c == 0)
		__delete this;
	return c;
}

HRESULT CNeuralFactory::Create (__in_opt INeuralNet* pNet, PCSTR pcszClass, __deref_out INetDocObject** ppObject)
{
	HRESULT hr = S_OK;
	INetDocObject* pObject = NULL;

	if(lstrcmp(pcszClass,"CNeurone") == 0)
		pObject = __new CNeurone;
	else if(lstrcmp(pcszClass,"CInputPad") == 0)
	{
		CInputPad* pPad = __new CInputPad;
		if(pPad && pNet)
			pPad->SetSquareSize(4);
		pObject = pPad;
	}
	else if(lstrcmp(pcszClass,"CSplitter") == 0)
		pObject = __new CSplitter;
	else if(lstrcmp(pcszClass,"CInputNeurone") == 0)
	{
		CInputNeurone* pInput = __new CInputNeurone;
		if(pInput && pNet)
			pInput->SetPin(pNet->GetUniquePin(INPUT_NEURONE));
		pObject = pInput;
	}
	else if(lstrcmp(pcszClass,"COutputNeurone") == 0)
	{
		COutputNeurone* pOutput = __new COutputNeurone;
		if(pOutput && pNet)
			pOutput->SetPin(pNet->GetUniquePin(OUTPUT_NEURONE));
		pObject = pOutput;
	}
	else if(lstrcmp(pcszClass,"CNeuralChip") == 0)
		pObject = __new CNeuralChip;
	else if(lstrcmp(pcszClass,"CNeuralFrame") == 0)
		pObject = __new CNeuralFrame;
	else if(lstrcmp(pcszClass,"CLogicAnd") == 0)
		hr = InitializeLogicGate(__new CLogicAnd, &pObject);
	else if(lstrcmp(pcszClass,"CLogicOr") == 0)
		hr = InitializeLogicGate(__new CLogicOr, &pObject);
	else if(lstrcmp(pcszClass,"CLogicNot") == 0)
		hr = InitializeLogicGate(__new CLogicNot, &pObject);
	else if(lstrcmp(pcszClass,"CLogicXor") == 0)
		hr = InitializeLogicGate(__new CLogicXor, &pObject);
	else if(TStrMatchLeftAssert(pcszClass, SLP("CLogicXor")))
	{
		INT cPins = Formatting::TAscToInt32(pcszClass + StaticLength("CLogicXor"));
		if(3 <= cPins)
			hr = InitializeLogicGate(__new CLogicXorN(cPins), &pObject);
		else
			hr = E_FAIL;
	}
	else if(lstrcmp(pcszClass,"CBiasNeurone") == 0)
		pObject = __new CBiasNeurone;
	else if(lstrcmp(pcszClass,"CSigmoidNeurone") == 0)
		pObject = __new CSigmoidNeurone;
	else
		hr = E_INVALIDARG;

	if(SUCCEEDED(hr))
	{
		*ppObject = pObject;
		if(NULL == pObject)
			hr = E_OUTOFMEMORY;
	}

	return hr;
}

HRESULT CNeuralFactory::InitializeLogicGate (CLogicGate* pNewGate, __deref_out INetDocObject** ppObject)
{
	HRESULT hr;
	if(pNewGate)
	{
		hr = pNewGate->Initialize();
		if(SUCCEEDED(hr))
			*ppObject = pNewGate;
		else
			pNewGate->Release();
	}
	else
		hr = E_OUTOFMEMORY;
	return hr;
}
