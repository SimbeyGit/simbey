#include "WinPython.h"
#include "Library\Core\CoreDefs.h"
#include "PyPrintTarget.h"
#include "PyInputSource.h"
#include "PyQuadooObject.h"
#include "PyQuadooVM.h"

static VOID PyQuadooVM_dealloc (PyQuadooVM* self)
{
	if(self->pVM)
	{
		self->pVM->Unload();
		self->pVM->Release();
	}

	Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* PyVM_FindFunction (PyQuadooVM* self, PyObject* args)
{
	PyObject* pyName;
	Py_ssize_t cchName;
	ULONG idxFunction;
	DWORD cParams;

	if(!PyArg_ParseTuple(args, "U", &pyName))
		return NULL;

	PWSTR pwzName = PyUnicode_AsWideCharString(pyName, &cchName);
	if(!pwzName)
		return NULL;

	HRESULT hrFind = self->pVM->FindFunction(pwzName, &idxFunction, &cParams);
	PyMem_Free(pwzName);

	if(FAILED(hrFind))
	{
		PyErr_SetString(PyExc_RuntimeError, "No such function");
		return NULL;
	}

	// Return (index, param_count) as a Python tuple
	return Py_BuildValue("(kI)", idxFunction, cParams);
}

static PyObject* PyVM_PushValue (PyQuadooVM* self, PyObject* args)
{
	PyObject* pyValue;
	if(!PyArg_ParseTuple(args, "O", &pyValue))
		return NULL;

	QuadooVM::QVARIANT qv;
	if(FAILED(PythonToQuadoo(pyValue, &qv)))
		return NULL;

	HRESULT hrPush = self->pVM->PushValue(&qv);
	QVMClearVariant(&qv);

	if(FAILED(hrPush))
	{
		PyErr_SetString(PyExc_RuntimeError, "Failed to push value");
		return NULL;
	}

	Py_RETURN_NONE;
}

static PyObject* PyVM_RunFunction (PyQuadooVM* self, PyObject* args)
{
	QuadooVM::QVARIANT qvResult; qvResult.eType = QuadooVM::Null;

	PyObject* pyValue;
	ULONG idxFunction;

	if(!PyArg_ParseTuple(args, "I", &idxFunction))
		return NULL;

	if(FAILED(self->pVM->RunFunction(idxFunction, &qvResult)))
	{
		PyErr_SetString(PyExc_RuntimeError, "Function failed to run");
		return NULL;
	}

	HRESULT hrConvert = QuadooToPython(&qvResult, &pyValue);
	QVMClearVariant(&qvResult);

	if(FAILED(hrConvert))
	{
		PyErr_SetString(PyExc_RuntimeError, "Could not convert QuadooScript result to Python");
		return NULL;
	}

	return pyValue;
}

static PyObject* PyVM_AddGlobal (PyQuadooVM* self, PyObject* args)
{
	CRString rsName;
	PyObject* pyName, *pyObject;

	if(!PyArg_ParseTuple(args, "UO", &pyName, &pyObject))
		return NULL;

	if(FAILED(PythonToRSTRING(pyName, &rsName)))
		return NULL;

	if(!PyObject_TypeCheck(pyObject, PY_QUADOO_OBJECT()))
	{
		PyErr_SetString(PyExc_TypeError, "Expected a Quadoo object as the second parameter");
		return NULL;
	}

	PyQuadooObject* pQuadoo = (PyQuadooObject*)pyObject;
	if(FAILED(self->pVM->AddGlobal(rsName, pQuadoo->pObject)))
	{
		PyErr_SetString(PyExc_RuntimeError, "AddGlobal() failed");
		return NULL;
	}

	Py_RETURN_NONE;
}

static PyObject* PyVM_RemoveGlobal (PyQuadooVM* self, PyObject* args)
{
	CRString rsName;
	PyObject* pyName;

	if(!PyArg_ParseTuple(args, "U", &pyName))
		return NULL;

	if(FAILED(self->pVM->RemoveGlobal(rsName, NULL)))
	{
		PyErr_SetString(PyExc_RuntimeError, "RemoveGlobal() failed");
		return NULL;
	}

	Py_RETURN_NONE;
}

static PyObject* PyVM_FindGlobal (PyQuadooVM* self, PyObject* args)
{
	CRString rsName;
	PyObject* pyName, *pyResult = NULL;
	QuadooVM::QVARIANT qvObject; qvObject.eType = QuadooVM::Null;

	PyCheckIf(!PyArg_ParseTuple(args, "U", &pyName), E_INVALIDARG);
	PyCheck(PythonToRSTRING(pyName, &rsName));
	PyCheck(self->pVM->FindGlobal(rsName, &qvObject.pObject));
	PyCheck(QuadooToPython(&qvObject, &pyResult));

Cleanup:
	QVMClearVariant(&qvObject);
	return pyResult;
}

static PyObject* PyVM_GetState (PyQuadooVM* self, PyObject* args)
{
	return PyLong_FromLong(self->pVM->GetState());
}

static PyObject* PyVM_Resume (PyQuadooVM* self, PyObject* args)
{
	PyObject* pyResult = NULL;
	PyObject* pyArg = NULL;
	QuadooVM::QVARIANT qvArg; qvArg.eType = QuadooVM::Null;
	QuadooVM::QVARIANT qv; qv.eType = QuadooVM::Null;

	PyCheckIf(!PyArg_ParseTuple(args, "|O", &pyArg), E_FAIL);
	if(pyArg)
	{
		PyCheck(PythonToQuadoo(pyArg, &qvArg));
		PyCheck(self->pVM->Resume(&qvArg, &qv));
	}
	else
		PyCheck(self->pVM->Resume(NULL, &qv));

	PyCheck(QuadooToPython(&qv, &pyResult));

Cleanup:
	QVMClearVariant(&qv);
	QVMClearVariant(&qvArg);
	return pyResult;
}

static PyObject* PyVM_SetPrintTarget (PyQuadooVM* self, PyObject* args)
{
	PyObject* pyObject, *pyResult = NULL;
	PyObject* pyPrint, *pyPrintLn;
	TStackRef<CPyPrintTarget> srPrintTarget;

	PyCheckIf(!PyArg_ParseTuple(args, "O", &pyObject), E_INVALIDARG);

	pyPrint = PyObject_GetAttrString(pyObject, "Print");
	if(NULL == pyPrint)
		pyPrint = PyObject_GetAttrString(pyObject, "write");
	PyCheckIfTypeMsg(NULL == pyPrint, "Expected object with 'Print' or 'write' method");

	pyPrintLn = PyObject_GetAttrString(pyObject, "PrintLn");
	if(NULL == pyPrintLn)
		pyPrintLn = PyObject_GetAttrString(pyObject, "writeLn");
	PyCheckIfTypeMsg(NULL == pyPrintLn, "Expected object with 'PrintLn' or 'writeLn' method");

	srPrintTarget.Attach(__new CPyPrintTarget(pyPrint, pyPrintLn));
	PyCheckAlloc(srPrintTarget);
	PyCheck(self->pVM->SetPrintTarget(srPrintTarget));
	pyResult = Py_NewRef(Py_None);

Cleanup:
	return pyResult;
}

static PyObject* PyVM_SetInputSource (PyQuadooVM* self, PyObject* args)
{
	PyObject* pyObject, *pyResult = NULL;
	PyObject* pyRead;
	TStackRef<CPyInputSource> srInputSource;

	PyCheckIf(!PyArg_ParseTuple(args, "O", &pyObject), E_INVALIDARG);

	pyRead = PyObject_GetAttrString(pyObject, "Read");
	if(NULL == pyRead)
		pyRead = PyObject_GetAttrString(pyObject, "read");
	PyCheckIfTypeMsg(NULL == pyRead, "Expected object with 'Read' or 'read' method");

	srInputSource.Attach(__new CPyInputSource(pyRead));
	PyCheckAlloc(srInputSource);
	PyCheck(self->pVM->SetInputSource(srInputSource));
	pyResult = Py_NewRef(Py_None);

Cleanup:
	return pyResult;
}

static PyMethodDef g_pyQuadooVMMethods[] =
{
	{ "FindFunction", (PyCFunction)PyVM_FindFunction, METH_VARARGS, "Find a global script function and return its index" },
	{ "PushValue", (PyCFunction)PyVM_PushValue, METH_VARARGS, "Push a value onto the call stack" },
	{ "RunFunction", (PyCFunction)PyVM_RunFunction, METH_VARARGS, "Run a script function" },
	{ "AddGlobal", (PyCFunction)PyVM_AddGlobal, METH_VARARGS, "Add a named global to the VM" },
	{ "RemoveGlobal", (PyCFunction)PyVM_RemoveGlobal, METH_VARARGS, "Remove a named global from the VM" },
	{ "FindGlobal", (PyCFunction)PyVM_FindGlobal, METH_VARARGS, "Find a named global from the VM" },
	{ "GetState", (PyCFunction)PyVM_GetState, METH_NOARGS, "Get the VM's current state" },
	{ "Resume", (PyCFunction)PyVM_Resume, METH_VARARGS, "Resume a suspended VM with an optional value to push onto the stack" },
	{ "SetPrintTarget", (PyCFunction)PyVM_SetPrintTarget, METH_VARARGS, "Set a Python object as the print target for the VM" },
	{ "SetInputSource", (PyCFunction)PyVM_SetInputSource, METH_VARARGS, "Set a Python object as the input source for the VM" },
	{ NULL, NULL, 0, NULL }
};

static PyTypeObject g_PyQuadooVM =
{
	PyVarObject_HEAD_INIT(NULL, 0)
	"PyQuadoo.VM",					// tp_name
	sizeof(PyQuadooVM),				// tp_basicsize
	0,								// tp_itemsize
	(destructor)PyQuadooVM_dealloc,	// tp_dealloc
	0, 0, 0, 0, 0,					// standard method slots
	0, 0, 0, 0, 0,					//
	0, 0, 0, 0,						//
	Py_TPFLAGS_DEFAULT,				// tp_flags
	"QuadooScript VM",				// tp_doc
	0, 0, 0, 0, 0, 0,
	g_pyQuadooVMMethods
};

PyTypeObject* PY_QUADOO_VM (VOID)
{
	return &g_PyQuadooVM;
}
