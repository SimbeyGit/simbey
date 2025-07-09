#include "WinPython.h"
#include "Library\Core\CoreDefs.h"
#include "Library\Core\MemoryStream.h"
#include "PyQuadooJSONObject.h"
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

	if(SUCCEEDED(self->pJSONArray->GetValue(index, &srv)))
		PyCheck(JSONToPython(srv, &pyResult));
	else
		PyErr_SetString(PyExc_IndexError, "Index out of range");

Cleanup:
	return pyResult;
}

static int JSONArray_setitem (PyQuadooJSONArray* self, Py_ssize_t index, PyObject* value)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;

	if(NULL == value)
		Check(self->pJSONArray->Remove(index));
	else
	{
		Check(PythonToJSON(value, &srv));
		Check(self->pJSONArray->Replace(index, srv));
	}

Cleanup:
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

static PyObject* PyQuadooJSONArray_str (PyQuadooJSONArray* self)
{
	PyObject* pyResult = NULL;
	CMemoryStream stmJSON;

	PyCheck(JSONSerializeArray(self->pJSONArray, &stmJSON));
	pyResult = PyUnicode_FromWideChar(stmJSON.TGetReadPtr<WCHAR>(), stmJSON.TDataRemaining<WCHAR>());

Cleanup:
	return pyResult;
}

static PyObject* PyQuadooJSONArray_Add (PyQuadooJSONArray* self, PyObject* args)
{
	PyObject* pyResult = NULL, *pyValue;
	TStackRef<IJSONValue> srv;

	PyCheckIf(!PyArg_ParseTuple(args, "O", &pyValue), E_INVALIDARG);
	PyCheck(PythonToJSON(pyValue, &srv));
	PyCheck(self->pJSONArray->Add(srv));
	pyResult = Py_NewRef(Py_None);

Cleanup:
	return pyResult;
}

static PyObject* PyQuadooJSONArray_Insert (PyQuadooJSONArray* self, PyObject* args)
{
	PyObject* pyResult = NULL, *pyValue;
	sysint nIndex;
	TStackRef<IJSONValue> srv;

	PyCheckIf(!PyArg_ParseTuple(args, "nO", &nIndex, &pyValue), E_INVALIDARG);
	PyCheck(PythonToJSON(pyValue, &srv));
	PyCheck(self->pJSONArray->Insert(nIndex, srv));
	pyResult = Py_NewRef(Py_None);

Cleanup:
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

static PyObject* PyQuadooJSONArray_FindObject (PyQuadooJSONArray* self, PyObject* args)
{
	PyObject* pyResult = NULL;
	PyObject* pyField, *pyValue;
	CRString rsField, rsValue;
	TStackRef<IJSONObject> srObject;
	PyQuadooJSONObject* pyJSONObject = NULL;
	sysint nIndex;

	PyCheckIf(!PyArg_ParseTuple(args, "UU", &pyField, &pyValue), E_INVALIDARG);
	PyCheck(PythonToRSTRING(pyField, &rsField));
	PyCheck(PythonToRSTRING(pyValue, &rsValue));
	if(SUCCEEDED(JSONFindArrayObject(self->pJSONArray, rsField, rsValue, &srObject, &nIndex)))
	{
		pyJSONObject = PyObject_New(PyQuadooJSONObject, PY_QUADOO_JSONOBJECT());
		PyCheckAlloc(pyJSONObject);
		pyJSONObject->pJSONObject = srObject.Detach();

		pyResult = Py_BuildValue("Oi", pyJSONObject, (int)nIndex);
		PyCheckAlloc(pyResult);
		pyJSONObject = NULL;	// The reference is now owned by pyResult
	}
	else
		pyResult = Py_NewRef(Py_None);

Cleanup:
	Py_XDECREF(pyJSONObject);
	return pyResult;
}

static PyObject* PyQuadooJSONArray_FindString (PyQuadooJSONArray* self, PyObject* args)
{
	PyObject* pyResult = NULL;
	PyObject* pyFind;
	CRString rsFind;
	sysint nIndex;

	PyCheckIf(!PyArg_ParseTuple(args, "U", &pyFind), E_INVALIDARG);
	PyCheck(PythonToRSTRING(pyFind, &rsFind));
	if(SUCCEEDED(JSONFindArrayString(self->pJSONArray, rsFind, &nIndex)))
		pyResult = PyLong_FromSsize_t(nIndex);
	else
		pyResult = Py_NewRef(Py_None);

Cleanup:
	return pyResult;
}

static PyMethodDef g_pyQuadooJSONArrayMethods[] =
{
	{ "Add", (PyCFunction)PyQuadooJSONArray_Add, METH_VARARGS, "Add a value" },
	{ "Insert", (PyCFunction)PyQuadooJSONArray_Insert, METH_VARARGS, "Insert a value by index" },
	{ "Remove", (PyCFunction)PyQuadooJSONArray_Remove, METH_VARARGS, "Remove a value by index" },
	{ "Clear", (PyCFunction)PyQuadooJSONArray_Clear, METH_NOARGS, "Clear the array" },
	{ "FindObject", (PyCFunction)PyQuadooJSONArray_FindObject, METH_VARARGS, "Find an object matching the specified field:value" },
	{ "FindString", (PyCFunction)PyQuadooJSONArray_FindString, METH_VARARGS, "Find the array index containing the specified string" },
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
	(reprfunc)PyQuadooJSONArray_str,		// tp_str
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
