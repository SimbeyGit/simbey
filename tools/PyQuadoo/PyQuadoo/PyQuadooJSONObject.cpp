#include "WinPython.h"
#include "Library\Core\CoreDefs.h"
#include "PyQuadooJSONObjectNames.h"
#include "PyQuadooJSONObject.h"

static VOID PyQuadooJSONObject_dealloc (PyQuadooJSONObject* self)
{
	SafeRelease(self->pJSONObject);
	Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* PyQuadooJSONObject_GetName (PyQuadooJSONObject* self, PyObject* args)
{
	INT nIndex;
	RSTRING rstrName = NULL;
	PyObject* pyResult = NULL;
	CRString rsName;

	PyCheckIf(!PyArg_ParseTuple(args, "n", &nIndex), E_INVALIDARG);
	PyCheck(self->pJSONObject->GetValueName(nIndex, &rsName));
	pyResult = PyUnicode_FromWideChar(RStrToWide(*rsName), rsName.Length());
	PyCheckAlloc(pyResult);

Cleanup:
	return pyResult;
}

static PyObject* PyQuadooJSONObject_SetName (PyQuadooJSONObject* self, PyObject* args)
{
	INT nIndex;
	PyObject* pyResult = NULL, *pyName;
	CRString rsName;

	PyCheckIf(!PyArg_ParseTuple(args, "nU", &nIndex, &pyName), E_INVALIDARG);
	PyCheck(PythonToRSTRING(pyName, &rsName));
	PyCheck(self->pJSONObject->SetValueName(nIndex, rsName));
	pyResult = Py_NewRef(Py_None);

Cleanup:
	return pyResult;
}

static PyMethodDef g_pyQuadooJSONObjectMethods[] =
{
	{ "GetName", (PyCFunction)PyQuadooJSONObject_GetName, METH_VARARGS, "Get key name by index" },
	{ "SetName", (PyCFunction)PyQuadooJSONObject_SetName, METH_VARARGS, "Set key name by index" },
	{ NULL, NULL, 0, NULL }
};

static Py_ssize_t PyQuadooJSONObject_len (PyQuadooJSONObject* self)
{
	return (Py_ssize_t)self->pJSONObject->Count();
}

static PyObject* PyQuadooJSONObject_getItem (PyQuadooJSONObject* self, PyObject* key)
{
	PyObject* pyResult = NULL;
	RSTRING rstrKey = NULL;
	TStackRef<IJSONValue> srv;
	QuadooVM::QVARIANT qv; qv.eType = QuadooVM::Null;

	if(PyLong_Check(key))
	{
		sysint nIndex = (sysint)PyLong_AsSsize_t(key);
		PyCheckIf(PyErr_Occurred(), E_FAIL);
		PyCheck(self->pJSONObject->GetValueByIndex(nIndex, &srv));
	}
	else
	{
		PyCheck(PythonToRSTRING(key, &rstrKey));

		if(FAILED(self->pJSONObject->FindValue(rstrKey, &srv)))
		{
			PyErr_SetString(PyExc_KeyError, "Key not found in object");
			goto Cleanup;
		}
	}
	PyCheck(QVMConvertFromJSON(srv, &qv));
	PyCheck(QuadooToPython(&qv, &pyResult));

Cleanup:
	QVMClearVariant(&qv);
	RStrRelease(rstrKey);
	return pyResult;
}

static INT PyQuadooJSONObject_setItem (PyQuadooJSONObject* self, PyObject* key, PyObject* value)
{
	HRESULT hr;
	RSTRING rstrKey = NULL;
	QuadooVM::QVARIANT qv; qv.eType = QuadooVM::Null;
	TStackRef<IJSONValue> srv;

	if(NULL == value)
	{
		// Remove/delete the key

		if(PyLong_Check(key))
		{
			sysint nIndex = (sysint)PyLong_AsSsize_t(key);
			CheckIf(PyErr_Occurred(), E_FAIL);
			Check(self->pJSONObject->GetValueName(nIndex, &rstrKey));
		}
		else
			Check(PythonToRSTRING(key, &rstrKey));

		Check(self->pJSONObject->RemoveValue(rstrKey));
	}
	else
	{
		Check(PythonToQuadoo(value, &qv));
		Check(QVMConvertToJSON(&qv, &srv));

		if(PyLong_Check(key))
		{
			sysint nIndex = (sysint)PyLong_AsSsize_t(key);
			CheckIf(PyErr_Occurred(), E_FAIL);
			Check(self->pJSONObject->SetValueByIndex(nIndex, srv));
		}
		else
		{
			Check(PythonToRSTRING(key, &rstrKey));
			Check(self->pJSONObject->AddValue(rstrKey, srv));
		}
	}

Cleanup:
	if(FAILED(hr))
		SetHResultError(hr);

	QVMClearVariant(&qv);
	RStrRelease(rstrKey);
	return SUCCEEDED(hr) ? 0 : -1;
}

static PyMappingMethods g_pyQuadooJSONObjectMapping =
{
	(lenfunc)PyQuadooJSONObject_len,			// mp_length
	(binaryfunc)PyQuadooJSONObject_getItem,		// mp_subscript (e.g., obj["key"])
	(objobjargproc)PyQuadooJSONObject_setItem	// mp_ass_subscript (e.g., obj["key"] = value)
};

static PyObject* PyQuadooJSONObject_getattro (PyQuadooJSONObject* self, PyObject* attrName)
{
	PyObject* pyResult = NULL;

	if(!PyUnicode_Check(attrName))
	{
		PyErr_SetString(PyExc_TypeError, "Attribute name must be a string");
		goto Cleanup;
	}

	if(PyUnicode_CompareWithASCIIString(attrName, "names") == 0)
	{
		PyQuadooJSONObjectNames* pNames = PyObject_New(PyQuadooJSONObjectNames, PY_QUADOO_JSONOBJECT_NAMES());
		PyCheckAlloc(pNames);
		SetInterface(pNames->pJSONObject, self->pJSONObject);
		pyResult = (PyObject*)pNames;
	}
	else
		pyResult = PyObject_GenericGetAttr((PyObject*)self, attrName);

Cleanup:
	return pyResult;
}

static PyObject* PyQuadooJSONObject_getCount (PyQuadooJSONObject* self, PVOID closure)
{
	return PyLong_FromSsize_t(self->pJSONObject->Count());
}

static PyGetSetDef g_pyQuadooJSONObjectGetSet[] =
{
	{ "count", (getter)PyQuadooJSONObject_getCount, NULL, "Number of fields", NULL },
	{ NULL, NULL, NULL, NULL, NULL }
};

static PyTypeObject g_pyQuadooJSONObject =
{
	PyVarObject_HEAD_INIT(NULL, 0)
	"PyQuadoo.JSONObject",			// tp_name
	sizeof(PyQuadooJSONObject),		// tp_basicsize
	0,								// tp_itemsize
	(destructor)PyQuadooJSONObject_dealloc,	// tp_dealloc
	0, 0, 0, 0, 0,					// standard method slots
	0, 0,
	&g_pyQuadooJSONObjectMapping,	// tp_as_mapping
	0,								// tp_hash
	NULL,							// tp_call
	0,								// tp_str
	(getattrofunc)PyQuadooJSONObject_getattro,	// tp_getattro
	0,								// tp_setattro
	0,								// tp_as_buffer
	Py_TPFLAGS_DEFAULT,				// tp_flags
	"JSON Object",					// tp_doc
	0, 0, 0, 0, 0, 0,
	g_pyQuadooJSONObjectMethods,	// tp_methods
	0,
	g_pyQuadooJSONObjectGetSet		// tp_getset
};

PyTypeObject* PY_QUADOO_JSONOBJECT (VOID)
{
	return &g_pyQuadooJSONObject;
}
