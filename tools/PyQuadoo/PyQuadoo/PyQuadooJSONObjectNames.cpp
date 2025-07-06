#include "WinPython.h"
#include "Library\Core\CoreDefs.h"
#include "PyQuadooJSONObjectNames.h"

static VOID PyQuadooJSONObjectNames_dealloc (PyQuadooJSONObjectNames* self)
{
	SafeRelease(self->pJSONObject);
	Py_TYPE(self)->tp_free((PyObject*)self);
}

static Py_ssize_t NamesView_len (PyQuadooJSONObjectNames* self)
{
    return self->pJSONObject->Count();
}

static PyObject* NamesView_getitem (PyQuadooJSONObjectNames* self, Py_ssize_t index)
{
	PyObject* pyResult = NULL;
	RSTRING rstrName;

	if(SUCCEEDED(self->pJSONObject->GetValueName(index, &rstrName)))
	{
		pyResult = PyUnicode_FromWideChar(RStrToWide(rstrName), RStrLen(rstrName));
		RStrRelease(rstrName);
	}
	else
		PyErr_SetString(PyExc_IndexError, "Index out of range");

	return pyResult;
}

static int NamesView_setitem (PyQuadooJSONObjectNames* self, Py_ssize_t index, PyObject* value)
{
	HRESULT hr;
	RSTRING rstrName = NULL;

	Check(PythonToRSTRING(value, &rstrName));
	Check(self->pJSONObject->SetValueName(index, rstrName));

Cleanup:
	RStrRelease(rstrName);
	return SUCCEEDED(hr) ? 0 : -1;
}

static PySequenceMethods g_pyQuadooJSONObjectNamesSeq =
{
    (lenfunc)NamesView_len,				// sq_length
    0,									// sq_concat
    0,									// sq_repeat
    (ssizeargfunc)NamesView_getitem,	// sq_item
    0,									// sq_slice
    (ssizeobjargproc)NamesView_setitem,	// sq_ass_item
    0, 0, 0, 0
};

static PyTypeObject g_pyQuadooJSONObjectNames =
{
	PyVarObject_HEAD_INIT(NULL, 0)
	"PyQuadoo.JSONObjectNames",			// tp_name
	sizeof(PyQuadooJSONObjectNames),	// tp_basicsize
	0,									// tp_itemsize
	(destructor)PyQuadooJSONObjectNames_dealloc,	// tp_dealloc
	0, 0, 0, 0, 0, 0,					// standard method slots
	&g_pyQuadooJSONObjectNamesSeq,		// tp_as_sequence
	NULL,								// tp_as_mapping
	0,									// tp_hash
	NULL,								// tp_call
	0,									// tp_str
	0,									// tp_getattro
	0,									// tp_setattro
	0,									// tp_as_buffer
	Py_TPFLAGS_DEFAULT,					// tp_flags
	"JSON Object Names"					// tp_doc
};

PyTypeObject* PY_QUADOO_JSONOBJECT_NAMES (VOID)
{
	return &g_pyQuadooJSONObjectNames;
}
