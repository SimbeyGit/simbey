#include "WinPython.h"
#include "Library\Core\CoreDefs.h"
#include "PySysCallTarget.h"

CPySysCallTarget::CPySysCallTarget (PyObject* pyModule, PyObject* pySysCallTarget) :
	m_pyModule(pyModule),
	m_pySysCallTarget(pySysCallTarget)
{
	Py_INCREF(m_pyModule);
	Py_INCREF(m_pySysCallTarget);
}

CPySysCallTarget::~CPySysCallTarget ()
{
	Py_DECREF(m_pySysCallTarget);
	Py_DECREF(m_pyModule);
}

// IQuadooSysCallTarget

HRESULT STDMETHODCALLTYPE CPySysCallTarget::Invoke (IQuadooVM* pVM, DWORD dwSysCall, QuadooVM::QVPARAMS* pqvParams, __out QuadooVM::QVARIANT* pqvResult)
{
	HRESULT hr;
	INT cArgs = (INT)pqvParams->cArgs;
	PyObject* pySysCall = PyLong_FromUnsignedLong(dwSysCall);
	PyObject* pyArgs = PyTuple_New(cArgs + 1);
	PyObject* pyResult = NULL;

	CheckAlloc(pySysCall);
	CheckAlloc(pyArgs);

	PyTuple_SET_ITEM(pyArgs, 0, pySysCall);			// steals reference
	pySysCall = NULL;								// now owned by pyArgs

	for(BYTE i = 0; i < cArgs; i++)
	{
		PyObject* pyArg;
		Check(QuadooToPython(m_pyModule, pqvParams->pqvArgs + i, &pyArg));

		// The arguments are set from 1 through cArgs
		PyTuple_SET_ITEM(pyArgs, cArgs - i, pyArg);	// steals reference
	}

	pyResult = PyObject_CallObject(m_pySysCallTarget, pyArgs);
	CheckIf(NULL == pyResult, E_FAIL);

	if(GetSysCallPendingCapsule(m_pyModule) == pyResult)
		hr = E_PENDING;								// The VM will be suspended
	else
		Check(PythonToQuadoo(m_pyModule, pyResult, pqvResult));

Cleanup:
	Py_XDECREF(pyResult);
	Py_XDECREF(pyArgs);
	Py_XDECREF(pySysCall);
	return hr;
}
