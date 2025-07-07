#include "WinPython.h"
#include "Library\Core\CoreDefs.h"
#include "PyQuadooJSONArray.h"

static VOID PyQuadooJSONArray_dealloc (PyQuadooJSONArray* self)
{
	SafeRelease(self->pJSONArray);
	Py_TYPE(self)->tp_free((PyObject*)self);
}

static Py_ssize_t JSONArray_len (PyQuadooJSONArray* self)
{
	return self->pJSONArray->Count();
}

static PyObject* JSONArray_getitem (PyQuadooJSONArray* self, Py_ssize_t index)
{
	PyObject* pyResult = NULL;
	TStackRef<IJSONValue> srv;
	QuadooVM::QVARIANT qv; qv.eType = QuadooVM::Null;

	if(SUCCEEDED(self->pJSONArray->GetValue(index, &srv)))
	{
		PyCheck(QVMConvertFromJSON(srv, &qv));
		PyCheck(QuadooToPython(&qv, &pyResult));
	}
	else
		PyErr_SetString(PyExc_IndexError, "Index out of range");

Cleanup:
	QVMClearVariant(&qv);
	return pyResult;
}

static int JSONArray_setitem (PyQuadooJSONArray* self, Py_ssize_t index, PyObject* value)
{
	HRESULT hr;
	QuadooVM::QVARIANT qv; qv.eType = QuadooVM::Null;
	TStackRef<IJSONValue> srv;

	if(NULL == value)
		Check(self->pJSONArray->Remove(index));
	else
	{
		Check(PythonToQuadoo(value, &qv));
		Check(QVMConvertToJSON(&qv, &srv));
		Check(self->pJSONArray->Replace(index, srv));
	}

Cleanup:
	QVMClearVariant(&qv);
	return SUCCEEDED(hr) ? 0 : -1;
}

static PySequenceMethods g_pyQuadooJSONArraySeq =
{
    (lenfunc)JSONArray_len,				// sq_length
    0,									// sq_concat
    0,									// sq_repeat
    (ssizeargfunc)JSONArray_getitem,	// sq_item
    0,									// sq_slice
    (ssizeobjargproc)JSONArray_setitem,	// sq_ass_item
    0, 0, 0, 0
};

static PyObject* PyQuadooJSONArray_Add (PyQuadooJSONArray* self, PyObject* args)
{
	PyObject* pyResult = NULL, *pyValue;
	QuadooVM::QVARIANT qv; qv.eType = QuadooVM::Null;
	TStackRef<IJSONValue> srv;

	PyCheckIf(!PyArg_ParseTuple(args, "O", &pyValue), E_INVALIDARG);
	PyCheck(PythonToQuadoo(pyValue, &qv));
	PyCheck(QVMConvertToJSON(&qv, &srv));
	PyCheck(self->pJSONArray->Add(srv));
	pyResult = Py_NewRef(Py_None);

Cleanup:
	QVMClearVariant(&qv);
	return pyResult;
}

static PyObject* PyQuadooJSONArray_Insert (PyQuadooJSONArray* self, PyObject* args)
{
	PyObject* pyResult = NULL, *pyValue;
	sysint nIndex;
	QuadooVM::QVARIANT qv; qv.eType = QuadooVM::Null;
	TStackRef<IJSONValue> srv;

	PyCheckIf(!PyArg_ParseTuple(args, "nO", &nIndex, &pyValue), E_INVALIDARG);
	PyCheck(PythonToQuadoo(pyValue, &qv));
	PyCheck(QVMConvertToJSON(&qv, &srv));
	PyCheck(self->pJSONArray->Insert(nIndex, srv));
	pyResult = Py_NewRef(Py_None);

Cleanup:
	QVMClearVariant(&qv);
	return pyResult;
}

static PyObject* PyQuadooJSONArray_Remove (PyQuadooJSONArray* self, PyObject* args)
{
	PyObject* pyResult = NULL;
	sysint nIndex;

	PyCheckIf(!PyArg_ParseTuple(args, "n", &nIndex), E_INVALIDARG);
	PyCheck(self->pJSONArray->Remove(nIndex));
	pyResult = Py_NewRef(Py_None);

Cleanup:
	return pyResult;
}

static PyObject* PyQuadooJSONArray_Clear (PyQuadooJSONArray* self, PyObject* args)
{
	self->pJSONArray->Clear();
	Py_RETURN_NONE;
}

static PyMethodDef g_pyQuadooJSONArrayMethods[] =
{
	{ "Add", (PyCFunction)PyQuadooJSONArray_Add, METH_VARARGS, "Add a value" },
	{ "Insert", (PyCFunction)PyQuadooJSONArray_Insert, METH_VARARGS, "Insert a value by index" },
	{ "Remove", (PyCFunction)PyQuadooJSONArray_Remove, METH_VARARGS, "Remove a value by index" },
	{ "Clear", (PyCFunction)PyQuadooJSONArray_Clear, METH_NOARGS, "Clear the array" },
	{ NULL, NULL, 0, NULL }
};

static PyObject* PyQuadooJSONArray_getCount (PyQuadooJSONArray* self, PVOID closure)
{
	return PyLong_FromSsize_t(self->pJSONArray->Count());
}

static PyGetSetDef g_pyQuadooJSONArrayGetSet[] =
{
	{ "count", (getter)PyQuadooJSONArray_getCount, NULL, "Number of elements", NULL },
	{ NULL, NULL, NULL, NULL, NULL }
};

static PyTypeObject g_pyQuadooJSONArray =
{
	PyVarObject_HEAD_INIT(NULL, 0)
	"PyQuadoo.JSONArray",			// tp_name
	sizeof(PyQuadooJSONArray),		// tp_basicsize
	0,								// tp_itemsize
	(destructor)PyQuadooJSONArray_dealloc,	// tp_dealloc
	0, 0, 0, 0, 0,					// standard method slots
	0,
	&g_pyQuadooJSONArraySeq,		// tp_as_sequence
	NULL,							// tp_as_mapping
	0,								// tp_hash
	NULL,							// tp_call
	0,								// tp_str
	NULL,							// tp_getattro
	0,								// tp_setattro
	0,								// tp_as_buffer
	Py_TPFLAGS_DEFAULT,				// tp_flags
	"JSON Array",					// tp_doc
	0, 0, 0, 0, 0, 0,
	g_pyQuadooJSONArrayMethods,		// tp_methods
	0,
	g_pyQuadooJSONArrayGetSet		// tp_getset
};

PyTypeObject* PY_QUADOO_JSONARRAY (VOID)
{
	return &g_pyQuadooJSONArray;
}
