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
	return NULL;
}

static INT PyQuadooMap_setItem (PyQuadooMap* self, PyObject* key, PyObject* value)
{
	return 0;
}

static PyMappingMethods g_pyQuadooMapMapping =
{
	(lenfunc)PyQuadooMap_len,			// mp_length
	(binaryfunc)PyQuadooMap_getItem,	// mp_subscript (e.g., obj["key"])
	(objobjargproc)PyQuadooMap_setItem	// mp_ass_subscript (e.g., obj["key"] = value)
};

static PyObject* PyQuadooMap_getattro (PyQuadooMap* self, PyObject* attrName)
{
	return NULL;
}

static PyObject* PyQuadooMap_setattro (PyQuadooMap* self, PyObject* attrName, PyObject* pValue)
{
	return NULL;
}

static PyMethodDef g_pyQuadooObjectMethods[] =
{
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
