#include "WinPython.h"
#include "Library\Core\CoreDefs.h"
#include "PyObjectWrapper.h"

CPyObjectWrapper::CPyObjectWrapper (PyObject* pyObject) :
	m_pyObject(pyObject)
{
	Py_XINCREF(m_pyObject);
}

CPyObjectWrapper::~CPyObjectWrapper ()
{
	Py_DECREF(m_pyObject);
}

// IQuadooObject

HRESULT STDMETHODCALLTYPE CPyObjectWrapper::Invoke (__in_opt IQuadooVM* pVM, RSTRING rstrMethod, QuadooVM::QVPARAMS* pqvParams, __out QuadooVM::QVARIANT* pqvResult)
{
	HRESULT hr;
	PCWSTR pcwzMethod = RStrToWide(rstrMethod);
	PyObject* pyMethodName = PyUnicode_FromWideChar(pcwzMethod, RStrLen(rstrMethod));
	PyObject* pyCallable = NULL, *pyArgs = NULL, *pyResult = NULL;

	CheckAlloc(pyMethodName);

	// Look up method on the Python object
	pyCallable = PyObject_GetAttr(m_pyObject, pyMethodName);
	Py_DECREF(pyMethodName);

	CheckIf(NULL == pyCallable, DISP_E_UNKNOWNNAME);
	CheckIf(!PyCallable_Check(pyCallable), HRESULT_FROM_WIN32(ERROR_CALL_NOT_IMPLEMENTED));

	// Convert Quadoo params to Python tuple
	BYTE cArgs = pqvParams->cArgs;
	pyArgs = PyTuple_New(cArgs);
	CheckAlloc(pyArgs);

	BYTE cArgsMinusOne = cArgs - 1;
	for(BYTE i = 0; i < cArgs; i++)
	{
		PyObject* pyArg;
		Check(QuadooToPython(pqvParams->pqvArgs + i, &pyArg));
		PyTuple_SET_ITEM(pyArgs, cArgsMinusOne - i, pyArg);  // steals reference
	}

	pyResult = PyObject_CallObject(pyCallable, pyArgs);
	CheckIf(NULL == pyResult, E_FAIL);

	Check(PythonToQuadoo(pyResult, pqvResult));

Cleanup:
	Py_DECREF(pyResult);
	Py_DECREF(pyArgs);
	Py_DECREF(pyCallable);
	return hr;
}
