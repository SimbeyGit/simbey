#include "WinPython.h"
#include "Library\Core\CoreDefs.h"
#include "Library\Util\Formatting.h"
#include "PyQuadooObject.h"
#include "PyQuadooVM.h"
#include "PyQuadooLoader.h"

static VOID PyQuadooLoader_dealloc (PyQuadooLoader* self)
{
	SafeRelease(self->pLastConstructorException);

	if(self->pLoader)
		self->pLoader->Release();

	Py_DECREF(self->pyModule);

	Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* PyLoader_AddProgram (PyQuadooLoader* self, PyObject* args)
{
	PyObject* pyName, *pyBytes;
	const char* pByteCode;
	Py_ssize_t cbByteCode;
	CRString rsName;

	// Expect: (name: str, bytes)
	if(!PyArg_ParseTuple(args, "UO!", &pyName, &PyBytes_Type, &pyBytes))
		return NULL;

	if(FAILED(PythonToRSTRING(pyName, &rsName)))
		return NULL;

	if(PyBytes_AsStringAndSize(pyBytes, (CHAR**)&pByteCode, &cbByteCode) != 0)
		return NULL;

	HRESULT hr = self->pLoader->AddInstance(rsName, NULL, (const BYTE*)pByteCode, (DWORD)cbByteCode, NULL);
	if(FAILED(hr))
	{
		CHAR szError[64];
		SideAssertHr(Formatting::TPrintF(szError, ARRAYSIZE(szError), NULL, "AddInstance failed [0x%.8X]", hr));
		PyErr_SetString(PyExc_RuntimeError, szError);
		return NULL;
	}

	Py_RETURN_NONE;
}

static PyObject* PyLoader_RemoveProgram (PyQuadooLoader* self, PyObject* args)
{
	PyObject* pyName;
	CRString rsName;

	if(!PyArg_ParseTuple(args, "U", &pyName))
		return NULL;

	if(FAILED(PythonToRSTRING(pyName, &rsName)))
		return NULL;

	HRESULT hr = self->pLoader->RemoveInstance(rsName);
	if(FAILED(hr))
	{
		CHAR szError[64];
		SideAssertHr(Formatting::TPrintF(szError, ARRAYSIZE(szError), NULL, "RemoveInstance failed [0x%.8X]", hr));
		PyErr_SetString(PyExc_RuntimeError, szError);
		return NULL;
	}

	Py_RETURN_NONE;
}

static PyObject* PyLoader_LoadVM (PyQuadooLoader* self, PyObject* args)
{
	TStackRef<IQuadooVM> srVM;
	PyObject* pyName;
	CRString rsName;

	if(!PyArg_ParseTuple(args, "U", &pyName))
		return NULL;

	if(FAILED(PythonToRSTRING(pyName, &rsName)))
		return NULL;

	HRESULT hrRegistered;
	HRESULT hr = self->pLoader->LoadVM(rsName, &hrRegistered, &srVM);
	if(FAILED(hr))
	{
		if(FAILED(hrRegistered))
			PyErr_SetString(PyExc_RuntimeError, "No program registered under provided name");
		else
			PyErr_SetString(PyExc_RuntimeError, "Failed to load VM");
		return NULL;
	}

	SafeRelease(self->pLastConstructorException);
	if(FAILED(srVM->RunConstructor(&self->pLastConstructorException)))
	{
		PyErr_SetString(PyExc_RuntimeError, "VM construction failed");
		return NULL;
	}

	PyQuadooVM* pyVM = PyObject_New(PyQuadooVM, PY_QUADOO_VM());

	pyVM->pyModule = self->pyModule;
	Py_INCREF(pyVM->pyModule);

	if(NULL == pyVM)
		return PyErr_NoMemory();
	pyVM->pVM = srVM.Detach();

	return (PyObject*)pyVM;
}

static PyObject* PyLoader_GetLastException (PyQuadooLoader* self, PyObject* args)
{
	PyQuadooObject* pyObject = PyObject_New(PyQuadooObject, PY_QUADOO_OBJECT());
	if(NULL == pyObject)
		return PyErr_NoMemory();
	SetInterface(pyObject->pObject, self->pLastConstructorException);

	return (PyObject*)pyObject;
}

static PyMethodDef g_pyQuadooLoaderMethods[] =
{
	{ "AddProgram", (PyCFunction)PyLoader_AddProgram, METH_VARARGS, "Add a compiled program to the VM loader" },
	{ "RemoveProgram", (PyCFunction)PyLoader_RemoveProgram, METH_VARARGS, "Remove a compiled program from the VM loader" },
	{ "LoadVM", (PyCFunction)PyLoader_LoadVM, METH_VARARGS, "Load a registered program into a new VM instance" },
	{ "GetLastException", (PyCFunction)PyLoader_GetLastException, METH_NOARGS, "Return the last VM constructor exception" },
	{ NULL, NULL, 0, NULL }
};

static PyTypeObject g_pyQuadooLoader =
{
	PyVarObject_HEAD_INIT(NULL, 0)
	"PyQuadoo.Loader",				// tp_name
	sizeof(PyQuadooLoader),			// tp_basicsize
	0,								// tp_itemsize
	(destructor)PyQuadooLoader_dealloc, // tp_dealloc
	0, 0, 0, 0, 0,					// standard method slots
	0, 0, 0, 0, 0,					//
	0, 0, 0, 0,						//
	Py_TPFLAGS_DEFAULT,				// tp_flags
	"QuadooScript VM Loader",		// tp_doc
	0, 0, 0, 0, 0, 0,
	g_pyQuadooLoaderMethods
};

PyTypeObject* PY_QUADOO_LOADER (VOID)
{
	return &g_pyQuadooLoader;
}
