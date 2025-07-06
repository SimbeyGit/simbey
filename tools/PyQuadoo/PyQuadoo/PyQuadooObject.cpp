#include "WinPython.h"
#include "Library\Core\CoreDefs.h"
#include "PyQuadooAttribute.h"
#include "PyQuadooObject.h"

static VOID PyQuadooObject_dealloc (PyQuadooObject* self)
{
	if(self->pObject)
		QVMReleaseObject(self->pObject);

	Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* PyQuadooObject_getattro (PyQuadooObject* self, PyObject* attrName)
{
	CRString rsName;

	if(!PyUnicode_Check(attrName))
	{
		PyErr_SetString(PyExc_TypeError, "Attribute name must be a string");
		return NULL;
	}

	if(FAILED(PythonToRSTRING(attrName, &rsName)))
		return NULL;

	PyQuadooAttribute* pyInvoke = PyObject_New(PyQuadooAttribute, PY_QUADOO_ATTRIBUTE());
	if(NULL == pyInvoke)
		return PyErr_NoMemory();

	SetInterface(pyInvoke->pObject, self->pObject);
	pyInvoke->rstrName = rsName.Detach();
	return (PyObject*)pyInvoke;
}

static PyMethodDef g_pyQuadooObjectMethods[] =
{
	{ NULL, NULL, 0, NULL }
};

static PyTypeObject g_pyQuadooObject =
{
	PyVarObject_HEAD_INIT(NULL, 0)
	"PyQuadoo.Object",				// tp_name
	sizeof(PyTypeObject),			// tp_basicsize
	0,								// tp_itemsize
	(destructor)PyQuadooObject_dealloc,	// tp_dealloc
	0, 0, 0, 0, 0,					// standard method slots
	0, 0, 0, 0, 0,					//
	0,								// tp_str
	(getattrofunc)PyQuadooObject_getattro,	// tp_getattro
	0,								// tp_setattro
	0,								// tp_as_buffer
	Py_TPFLAGS_DEFAULT,				// tp_flags
	"QuadooScript Object",			// tp_doc
	0, 0, 0, 0, 0, 0,
	g_pyQuadooObjectMethods
};

PyTypeObject* PY_QUADOO_OBJECT (VOID)
{
	return &g_pyQuadooObject;
}
