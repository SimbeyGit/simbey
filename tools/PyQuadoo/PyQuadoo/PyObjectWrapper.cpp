#include "WinPython.h"
#include "Library\Core\CoreDefs.h"
#include "PyObjectWrapper.h"

CPyObjectWrapper::CPyObjectWrapper (PyObject* pyModule, PyObject* pyObject) :
	m_pyModule(pyModule),
	m_pyObject(pyObject)
{
	Py_INCREF(m_pyModule);
	Py_INCREF(m_pyObject);
}

CPyObjectWrapper::~CPyObjectWrapper ()
{
	Py_DECREF(m_pyObject);
	Py_DECREF(m_pyModule);
}

// IQuadooObject

HRESULT STDMETHODCALLTYPE CPyObjectWrapper::Invoke (__in_opt IQuadooVM* pVM, RSTRING rstrMethod, QuadooVM::QVPARAMS* pqvParams, __out QuadooVM::QVARIANT* pqvResult)
{
	HRESULT hr;
	PyObject* pyCallable = NULL, *pyArgs = NULL, *pyResult = NULL;

	Check(GetAttribute(rstrMethod, &pyCallable));
	CheckIf(!PyCallable_Check(pyCallable), HRESULT_FROM_WIN32(ERROR_CALL_NOT_IMPLEMENTED));

	// Convert Quadoo params to Python tuple
	BYTE cArgs = pqvParams->cArgs;
	pyArgs = PyTuple_New(cArgs);
	CheckAlloc(pyArgs);

	BYTE cArgsMinusOne = cArgs - 1;
	for(BYTE i = 0; i < cArgs; i++)
	{
		PyObject* pyArg;
		Check(QuadooToPython(m_pyModule, pqvParams->pqvArgs + i, &pyArg));
		PyTuple_SET_ITEM(pyArgs, cArgsMinusOne - i, pyArg);  // steals reference
	}

	pyResult = PyObject_CallObject(pyCallable, pyArgs);
	CheckIf(NULL == pyResult, E_FAIL);

	Check(PythonToQuadoo(m_pyModule, pyResult, pqvResult));

Cleanup:
	Py_XDECREF(pyResult);
	Py_XDECREF(pyArgs);
	Py_XDECREF(pyCallable);
	return hr;
}

HRESULT STDMETHODCALLTYPE CPyObjectWrapper::GetProperty (__in_opt IQuadooVM* pVM, RSTRING rstrProperty, __out QuadooVM::QVARIANT* pqvResult)
{
	HRESULT hr;
	PyObject* pyProperty = NULL;

	Check(GetAttribute(rstrProperty, &pyProperty));
	Check(PythonToQuadoo(m_pyModule, pyProperty, pqvResult));

Cleanup:
	Py_XDECREF(pyProperty);
	return hr;
}

HRESULT STDMETHODCALLTYPE CPyObjectWrapper::GetIndexedProperty (__in_opt IQuadooVM* pVM, RSTRING rstrProperty, QuadooVM::QVARIANT* pqvIndex, __out QuadooVM::QVARIANT* pqvResult)
{
	HRESULT hr;
	PyObject* pyProperty = NULL, *pyIndex = NULL, *pyValue = NULL;

	Check(GetAttribute(rstrProperty, &pyProperty));
	Check(QuadooToPython(m_pyModule, pqvIndex, &pyIndex));

	pyValue = PyObject_GetItem(pyProperty, pyIndex);
	CheckIf(NULL == pyValue, E_FAIL);

	Check(PythonToQuadoo(m_pyModule, pyValue, pqvResult));

Cleanup:
	Py_XDECREF(pyValue);
	Py_XDECREF(pyIndex);
	Py_XDECREF(pyProperty);
	return hr;
}

HRESULT STDMETHODCALLTYPE CPyObjectWrapper::SetProperty (__in_opt IQuadooVM* pVM, RSTRING rstrProperty, QuadooVM::QVARIANT* pqv)
{
	HRESULT hr;
	PCWSTR pcwzProperty = RStrToWide(rstrProperty);
	PyObject* pyProperty = PyUnicode_FromWideChar(pcwzProperty, RStrLen(rstrProperty)), *pyValue = NULL;

	CheckAlloc(pyProperty);
	Check(QuadooToPython(m_pyModule, pqv, &pyValue));
	CheckIf(0 != PyObject_SetAttr(m_pyObject, pyProperty, pyValue), DISP_E_UNKNOWNNAME);

Cleanup:
	Py_XDECREF(pyValue);
	Py_XDECREF(pyProperty);
	return hr;
}

HRESULT STDMETHODCALLTYPE CPyObjectWrapper::SetIndexedProperty (__in_opt IQuadooVM* pVM, RSTRING rstrProperty, QuadooVM::QVARIANT* pqvIndex, QuadooVM::QVARIANT* pqv)
{
	HRESULT hr;
	PyObject* pyProperty = NULL, *pyIndex = NULL, *pyValue = NULL;

	Check(GetAttribute(rstrProperty, &pyProperty));
	Check(QuadooToPython(m_pyModule, pqvIndex, &pyIndex));
	Check(QuadooToPython(m_pyModule, pqv, &pyValue));

	CheckIf(0 != PyObject_SetItem(pyProperty, pyIndex, pyValue), E_FAIL);

Cleanup:
	Py_XDECREF(pyValue);
	Py_XDECREF(pyIndex);
	Py_XDECREF(pyProperty);
	return hr;
}

HRESULT STDMETHODCALLTYPE CPyObjectWrapper::DeleteProperty (RSTRING rstrProperty, __out_opt QuadooVM::QVARIANT* pqv)
{
	HRESULT hr;
	PCWSTR pcwzProperty = RStrToWide(rstrProperty);
	PyObject* pyProperty = PyUnicode_FromWideChar(pcwzProperty, RStrLen(rstrProperty)), *pyValue = NULL;

	CheckAlloc(pyProperty);

	if(pqv)
	{
		pyValue = PyObject_GetAttr(m_pyObject, pyProperty);
		CheckIf(NULL == pyValue, DISP_E_UNKNOWNNAME);
		Check(PythonToQuadoo(m_pyModule, pyValue, pqv));
	}

	CheckIf(0 != PyObject_DelAttr(m_pyObject, pyProperty), E_FAIL);
	hr = S_OK;

Cleanup:
	Py_XDECREF(pyValue);
	Py_XDECREF(pyProperty);
	return hr;
}

HRESULT CPyObjectWrapper::GetAttribute (RSTRING rstrAttribute, __deref_out PyObject** ppyObject)
{
	HRESULT hr;
	PCWSTR pcwzAttribute = RStrToWide(rstrAttribute);
	PyObject* pyAttribute = PyUnicode_FromWideChar(pcwzAttribute, RStrLen(rstrAttribute));

	CheckAlloc(pyAttribute);

	// Look up method on the Python object
	*ppyObject = PyObject_GetAttr(m_pyObject, pyAttribute);
	Py_DECREF(pyAttribute);

	CheckIf(NULL == *ppyObject, DISP_E_UNKNOWNNAME);
	hr = S_OK;

Cleanup:
	return hr;
}
