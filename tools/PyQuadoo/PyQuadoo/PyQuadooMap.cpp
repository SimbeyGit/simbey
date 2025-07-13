#include "WinPython.h"
#include "Library\Core\CoreDefs.h"
#include "PyQuadooMap.h"

static VOID PyQuadooMap_dealloc (PyQuadooMap* self)
{
	SafeRelease(self->pMap);
	Py_TYPE(self)->tp_free((PyObject*)self);
}

static Py_ssize_t PyQuadooMap_len (PyQuadooMap* self)
{
	return (Py_ssize_t)self->pMap->Length();
}

static PyObject* PyQuadooMap_getItem (PyQuadooMap* self, PyObject* key)
{
	PyObject* pyResult = NULL;
	QuadooVM::QVARIANT qv; qv.eType = QuadooVM::Null;

	if(PyLong_Check(key))
	{
		sysint nIndex = (sysint)PyLong_AsSsize_t(key);
		PyCheckIf(PyErr_Occurred(), E_FAIL);
		PyCheck(self->pMap->GetItem(nIndex, &qv));
	}
	else
	{
		QuadooVM::QVARIANT qvKey;

		PyCheck(PythonToRSTRING(key, &qvKey.rstrVal));
		qvKey.eType = QuadooVM::String;

		if(FAILED(self->pMap->Find(&qvKey, &qv)))
		{
			QVMClearVariant(&qvKey);
			PyErr_SetString(PyExc_KeyError, "Key not found in map");
			goto Cleanup;
		}

		QVMClearVariant(&qvKey);
	}

	PyCheck(QuadooToPython(&qv, &pyResult));

Cleanup:
	QVMClearVariant(&qv);
	return NULL;
}

static INT PyQuadooMap_setItem (PyQuadooMap* self, PyObject* key, PyObject* value)
{
	HRESULT hr;
	QuadooVM::QVARIANT qv; qv.eType = QuadooVM::Null;

	Check(PythonToQuadoo(value, &qv));

	if(PyLong_Check(key))
	{
		sysint nIndex = (sysint)PyLong_AsSsize_t(key);
		CheckIf(PyErr_Occurred(), E_FAIL);
		Check(self->pMap->SetItem(nIndex, &qv));
	}
	else
	{
		QuadooVM::QVARIANT qvKey;

		Check(PythonToRSTRING(key, &qvKey.rstrVal));
		qvKey.eType = QuadooVM::String;

		hr = self->pMap->Add(&qvKey, &qv);
		QVMClearVariant(&qvKey);
	}

Cleanup:
	QVMClearVariant(&qv);
	return SUCCEEDED(hr) ? 0 : -1;
}

static PyMappingMethods g_pyQuadooMapMapping =
{
	(lenfunc)PyQuadooMap_len,			// mp_length
	(binaryfunc)PyQuadooMap_getItem,	// mp_subscript (e.g., obj["key"])
	(objobjargproc)PyQuadooMap_setItem	// mp_ass_subscript (e.g., obj["key"] = value)
};

static PyObject* PyQuadooMap_getattro (PyQuadooMap* self, PyObject* attrName)
{
	PyObject* pyResult = NULL;
	QuadooVM::QVARIANT qvKey; qvKey.eType = QuadooVM::Null;
	QuadooVM::QVARIANT qv; qv.eType = QuadooVM::Null;

	PyCheck(PythonToRSTRING(attrName, &qvKey.rstrVal));
	qvKey.eType = QuadooVM::String;

	if(SUCCEEDED(self->pMap->Find(&qvKey, &qv)))
		PyCheck(QuadooToPython(&qv, &pyResult));
	else
		pyResult = Py_NewRef(Py_None);

Cleanup:
	QVMClearVariant(&qv);
	QVMClearVariant(&qvKey);
	return pyResult;
}

static int PyQuadooMap_setattro (PyQuadooMap* self, PyObject* attrName, PyObject* pValue)
{
	HRESULT hr;
	QuadooVM::QVARIANT qvKey; qvKey.eType = QuadooVM::Null;
	QuadooVM::QVARIANT qv; qv.eType = QuadooVM::Null;

	Check(PythonToRSTRING(attrName, &qvKey.rstrVal));
	qvKey.eType = QuadooVM::String;

	if(NULL == pValue)
	{
		sysint idxItem;
		CheckIfIgnore(!self->pMap->IndexOf(&qvKey, &idxItem), S_FALSE);
		Check(self->pMap->RemoveItem(idxItem, NULL));
	}
	else
	{
		Check(PythonToQuadoo(pValue, &qv));
		Check(self->pMap->Add(&qvKey, &qv));
	}

Cleanup:
	QVMClearVariant(&qv);
	QVMClearVariant(&qvKey);
	return SUCCEEDED(hr) ? 0 : -1;
}

static PyObject* PyQuadooMap_GetKey (PyQuadooMap* self, PyObject* args)
{
	PyObject* pyResult = NULL;
	sysint nIndex;
	QuadooVM::QVARIANT qvKey; qvKey.eType = QuadooVM::Null;

	PyCheckIf(!PyArg_ParseTuple(args, "n", &nIndex), E_INVALIDARG);
	PyCheck(self->pMap->GetKey(nIndex, &qvKey));
	PyCheck(QuadooToPython(&qvKey, &pyResult));

Cleanup:
	QVMClearVariant(&qvKey);
	return pyResult;
}

static PyObject* PyQuadooMap_Find (PyQuadooMap* self, PyObject* args)
{
	PyObject* pyResult = NULL;
	PyObject* pyKey = PyTuple_GetItem(args, 0);
	QuadooVM::QVARIANT qvKey; qvKey.eType = QuadooVM::Null;
	QuadooVM::QVARIANT qv; qv.eType = QuadooVM::Null;

	PyCheckIf(NULL == pyKey, E_INVALIDARG);
	PyCheck(PythonToQuadoo(pyKey, &qvKey));
	PyCheck(self->pMap->Find(&qvKey, &qv));
	PyCheck(QuadooToPython(&qv, &pyResult));

Cleanup:
	QVMClearVariant(&qv);
	QVMClearVariant(&qvKey);
	return pyResult;
}

static PyObject* PyQuadooMap_Add (PyQuadooMap* self, PyObject* args)
{
	PyObject* pyResult = NULL;
	PyObject* pyKey = PyTuple_GetItem(args, 0);
	PyObject* pyValue = PyTuple_GetItem(args, 1);
	QuadooVM::QVARIANT qvKey; qvKey.eType = QuadooVM::Null;
	QuadooVM::QVARIANT qv; qv.eType = QuadooVM::Null;

	PyCheckIf(NULL == pyKey || NULL == pyValue, E_INVALIDARG);
	PyCheck(PythonToQuadoo(pyKey, &qvKey));
	PyCheck(PythonToQuadoo(pyValue, &qv));
	PyCheck(self->pMap->Add(&qvKey, &qv));
	pyResult = Py_NewRef(Py_None);

Cleanup:
	QVMClearVariant(&qv);
	QVMClearVariant(&qvKey);
	return pyResult;
}

static PyObject* PyQuadooMap_MultiAdd (PyQuadooMap* self, PyObject* args)
{
	PyObject* pyResult = NULL;
	PyObject* pyKey = PyTuple_GetItem(args, 0);
	PyObject* pyValue = PyTuple_GetItem(args, 1);
	QuadooVM::QVARIANT qvKey; qvKey.eType = QuadooVM::Null;
	QuadooVM::QVARIANT qv; qv.eType = QuadooVM::Null;

	PyCheckIf(NULL == pyKey || NULL == pyValue, E_INVALIDARG);
	PyCheck(PythonToQuadoo(pyKey, &qvKey));
	PyCheck(PythonToQuadoo(pyValue, &qv));
	PyCheck(self->pMap->MultiAdd(&qvKey, &qv));
	pyResult = Py_NewRef(Py_None);

Cleanup:
	QVMClearVariant(&qv);
	QVMClearVariant(&qvKey);
	return pyResult;
}

static PyObject* PyQuadooMap_Delete (PyQuadooMap* self, PyObject* args)
{
	PyObject* pyResult = NULL;
	PyObject* pyKey = PyTuple_GetItem(args, 0);
	QuadooVM::QVARIANT qvKey; qvKey.eType = QuadooVM::Null;

	PyCheckIf(NULL == pyKey, E_INVALIDARG);
	PyCheck(PythonToQuadoo(pyKey, &qvKey));
	PyCheck(self->pMap->Delete(&qvKey, NULL));
	pyResult = Py_NewRef(Py_None);

Cleanup:
	QVMClearVariant(&qvKey);
	return pyResult;
}

static PyObject* PyQuadooMap_Has (PyQuadooMap* self, PyObject* args)
{
	PyObject* pyResult = NULL;
	PyObject* pyKey = PyTuple_GetItem(args, 0);
	QuadooVM::QVARIANT qvKey; qvKey.eType = QuadooVM::Null;

	PyCheckIf(NULL == pyKey, E_INVALIDARG);
	PyCheck(PythonToQuadoo(pyKey, &qvKey));
	pyResult = self->pMap->HasItem(&qvKey) ? Py_True : Py_False;
	Py_INCREF(pyResult);

Cleanup:
	QVMClearVariant(&qvKey);
	return pyResult;
}

static PyObject* PyQuadooMap_IndexOf (PyQuadooMap* self, PyObject* args)
{
	PyObject* pyResult = NULL;
	PyObject* pyKey = PyTuple_GetItem(args, 0);
	QuadooVM::QVARIANT qvKey; qvKey.eType = QuadooVM::Null;
	sysint idxItem;

	PyCheckIf(NULL == pyKey, E_INVALIDARG);
	PyCheck(PythonToQuadoo(pyKey, &qvKey));
	if(self->pMap->IndexOf(&qvKey, &idxItem))
	{
#ifdef	_WIN64 
		pyResult = PyLong_FromLongLong(idxItem);
#else
		pyResult = PyLong_FromLong(idxItem);
#endif
	}
	else
		pyResult = Py_NewRef(Py_None);

Cleanup:
	QVMClearVariant(&qvKey);
	return pyResult;
}

static PyMethodDef g_pyQuadooObjectMethods[] =
{
	{ "GetKey", (PyCFunction)PyQuadooMap_GetKey, METH_VARARGS, "Get the key at the specified index" },
	{ "Find", (PyCFunction)PyQuadooMap_Find, METH_VARARGS, "Find the value at the specified key" },
	{ "Add", (PyCFunction)PyQuadooMap_Add, METH_VARARGS, "Set the value at the specified key" },
	{ "MultiAdd", (PyCFunction)PyQuadooMap_MultiAdd, METH_VARARGS, "Add the value to an array of values belonging to the specified key" },
	{ "Delete", (PyCFunction)PyQuadooMap_Delete, METH_VARARGS, "Remove the value from the specified key" },
	{ "Has", (PyCFunction)PyQuadooMap_Has, METH_VARARGS, "Check whether the map contains the specified key" },
	{ "IndexOf", (PyCFunction)PyQuadooMap_IndexOf, METH_VARARGS, "Find the index of the specified key" },
	{ NULL, NULL, 0, NULL }
};

static PyTypeObject g_pyQuadooMap =
{
	PyVarObject_HEAD_INIT(NULL, 0)
	"PyQuadoo.Map",					// tp_name
	sizeof(PyQuadooMap),			// tp_basicsize
	0,								// tp_itemsize
	(destructor)PyQuadooMap_dealloc,// tp_dealloc
	0, 0, 0, 0, 0,					// standard method slots
	0, 0,
	&g_pyQuadooMapMapping,			// tp_as_mapping
	0,								// tp_hash
	NULL,							// tp_call
	0,								// tp_str
	(getattrofunc)PyQuadooMap_getattro,	// tp_getattro
	(setattrofunc)PyQuadooMap_setattro,	// tp_setattro
	0,								// tp_as_buffer
	Py_TPFLAGS_DEFAULT,				// tp_flags
	"QuadooScript Map",				// tp_doc
	0, 0, 0, 0, 0, 0,
	g_pyQuadooObjectMethods
};

PyTypeObject* PY_QUADOO_MAP (VOID)
{
	return &g_pyQuadooMap;
}
