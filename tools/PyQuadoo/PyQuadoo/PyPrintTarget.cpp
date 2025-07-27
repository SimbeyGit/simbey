#include "WinPython.h"
#include "Library\Core\CoreDefs.h"
#include "PyPrintTarget.h"

CPyPrintTarget::CPyPrintTarget (PyObject* pyPrint, PyObject* pyPrintLn) :
	m_pyPrint(pyPrint),
	m_pyPrintLn(pyPrintLn)
{
	Py_INCREF(m_pyPrint);
	Py_INCREF(m_pyPrintLn);
}

CPyPrintTarget::~CPyPrintTarget ()
{
	Py_DECREF(m_pyPrint);
	Py_DECREF(m_pyPrintLn);
}

// IQuadooPrintTarget

HRESULT STDMETHODCALLTYPE CPyPrintTarget::Print (RSTRING rstrText)
{
	return CallTarget(m_pyPrint, rstrText);
}

HRESULT STDMETHODCALLTYPE CPyPrintTarget::PrintLn (RSTRING rstrText)
{
	return CallTarget(m_pyPrintLn, rstrText);
}

HRESULT CPyPrintTarget::CallTarget (PyObject* pyTarget, RSTRING rstrText)
{
	HRESULT hr;
	PyObject* pyResult = NULL, *pyArgs = NULL, *pyText;

	pyArgs = PyTuple_New(1);
	CheckAlloc(pyArgs);

	pyText = PyUnicode_FromWideChar(RStrToWide(rstrText), RStrLen(rstrText));
	CheckAlloc(pyText);
	PyTuple_SET_ITEM(pyArgs, 0, pyText);	// steals reference

	pyResult = PyObject_CallObject(pyTarget, pyArgs);
	PyCheckIf(NULL == pyResult, E_FAIL);
	hr = S_OK;

Cleanup:
	Py_XDECREF(pyResult);
	Py_XDECREF(pyArgs);
	return hr;
}
