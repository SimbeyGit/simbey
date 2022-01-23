#pragma once

#include "QuadooVM.h"

interface __declspec(uuid("7D9081A8-811A-42f0-A0A9-8BD2D5C041E1")) IWebSocketsInterface : IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE GetInterface (RSTRING rstrInterface, __deref_out IQuadooInterface** ppInterface) = 0;
	virtual HRESULT STDMETHODCALLTYPE ProtectedCall (IQuadooInterface* pInterface, ULONG idxCall, QuadooVM::QVPARAMS* pqvParams, __out QuadooVM::QVARIANT* pqvResult) = 0;
	virtual HRESULT STDMETHODCALLTYPE SendToClient (QuadooVM::QVARIANT* pqv) = 0;
};
