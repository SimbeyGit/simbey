#include "WinPython.h"
#include "Library\Core\CoreDefs.h"
#include "PyInputSource.h"

CPyInputSource::CPyInputSource (PyObject* pyRead) :
	m_pyRead(pyRead)
{
	Py_INCREF(m_pyRead);
}

CPyInputSource::~CPyInputSource ()
{
	Py_DECREF(m_pyRead);
}

// IQuadooInputSource

HRESULT STDMETHODCALLTYPE CPyInputSource::Read (__inout QuadooVM::QVARIANT* pqvRead)
{
	HRESULT hr;
	DWORD cbRead;
	PyObject* pyResult = NULL;
	PyObject* pyArgs = NULL, *pyCount = NULL;

	Check(QVMVariantToDWord(pqvRead, &cbRead));
	QVMClearVariant(pqvRead);

	pyCount = PyLong_FromLongLong(cbRead);
	CheckAlloc(pyCount);

	pyArgs = PyTuple_Pack(1, pyCount);
	CheckAlloc(pyArgs);

	pyResult = PyObject_CallObject(m_pyRead, pyArgs);
	CheckIf(NULL == pyResult, E_FAIL);

	Check(PythonToRSTRING(pyResult, &pqvRead->rstrVal));
	pqvRead->eType = QuadooVM::String;

Cleanup:
	Py_XDECREF(pyResult);
	Py_XDECREF(pyArgs);
	Py_XDECREF(pyCount);
	return hr;
}
