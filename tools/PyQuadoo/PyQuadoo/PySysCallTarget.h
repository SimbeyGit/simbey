#pragma once

#include "Library\Core\BaseUnknown.h"
#include "PyQuadoo.h"

class CPySysCallTarget :
	public CBaseUnknown,
	public IQuadooSysCallTarget
{
private:
	PyObject* m_pyModule;
	PyObject* m_pySysCallTarget;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooSysCallTarget)
	END_UNK_MAP

public:
	CPySysCallTarget (PyObject* pyModule, PyObject* pySysCallTarget);
	~CPySysCallTarget ();

	// IQuadooInputSource
	virtual HRESULT STDMETHODCALLTYPE Invoke (IQuadooVM* pVM, DWORD dwSysCall, QuadooVM::QVPARAMS* pqvParams, __out QuadooVM::QVARIANT* pqvResult);
};
