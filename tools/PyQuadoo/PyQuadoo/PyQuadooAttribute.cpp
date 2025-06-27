#include "WinPython.h"
#include "Library\Core\CoreDefs.h"
#include "PyQuadooAttribute.h"

static VOID PyQuadooAttribute_dealloc (PyQuadooAttribute* self)
{
	RStrRelease(self->rstrName);
	if(self->pObject)
		QVMReleaseObject(self->pObject);

	Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* PyQuadooAttribute_call (PyQuadooAttribute* self, PyObject* args, PyObject* kwargs)
{
	PyObject* pyResult = NULL;
	QuadooVM::QVPARAMS qvParams;
	QuadooVM::QVARIANT qvResult; qvResult.eType = QuadooVM::Null;
	Py_ssize_t cArgs = PyTuple_Size(args);

	PyCheckIf(cArgs > 255, DISP_E_OVERFLOW);

	qvParams.cArgs = static_cast<BYTE>(cArgs);
	if(0 < cArgs)
	{
		qvParams.pqvArgs = __new QuadooVM::QVARIANT[qvParams.cArgs];
		PyCheckAlloc(qvParams.pqvArgs);
		ZeroMemory(qvParams.pqvArgs, sizeof(QuadooVM::QVARIANT) * qvParams.cArgs);

		// Arguments must be reversed for QuadooScript.
		BYTE bEnd = qvParams.cArgs - 1;
		for(INT i = 0; i < qvParams.cArgs; i++)
		{
			PyObject* pyArg = PyTuple_GetItem(args, i);
			PyCheck(PythonToQuadoo(pyArg, qvParams.pqvArgs + (bEnd - i)));
		}
	}
	else
		qvParams.pqvArgs = NULL;

	PyCheck(self->pObject->Invoke(NULL, self->rstrName, &qvParams, &qvResult));
	PyCheck(QuadooToPython(&qvResult, &pyResult));

Cleanup:
	if(0 < qvParams.cArgs)
	{
		for(INT i = 0; i < qvParams.cArgs; i++)
			QVMClearVariant(qvParams.pqvArgs + i);
		__delete_array qvParams.pqvArgs;
	}
	return pyResult;
}

static PyObject* PyAttr_Get (PyQuadooAttribute* self, PyObject* args)
{
	PyObject* pyResult = NULL;
	QuadooVM::QVARIANT qv;

	PyCheck(self->pObject->GetProperty(NULL, self->rstrName, &qv));
	PyCheck(QuadooToPython(&qv, &pyResult));

Cleanup:
	QVMClearVariant(&qv);
	return pyResult;
}

static PyObject* PyAttr_Set (PyQuadooAttribute* self, PyObject* args)
{
	PyObject* pyResult = NULL;
	PyObject* pyArg = PyTuple_GetItem(args, 0);
	QuadooVM::QVARIANT qv; qv.eType = QuadooVM::Null;

	PyCheckIf(NULL == pyArg, E_INVALIDARG);

	PyCheck(PythonToQuadoo(pyArg, &qv));
	PyCheck(self->pObject->SetProperty(NULL, self->rstrName, &qv));

	pyResult = Py_NewRef(Py_None);

Cleanup:
	QVMClearVariant(&qv);
	return pyResult;
}

static PyMethodDef g_pyQuadooAttributeMethods[] =
{
	{ "Get", (PyCFunction)PyAttr_Get, METH_NOARGS, "Get the attribute value" },
	{ "Set", (PyCFunction)PyAttr_Set, METH_VARARGS, "Get the attribute value" },
	{ NULL, NULL, 0, NULL }
};

static PyTypeObject g_pyQuadooAttribute =
{
	PyVarObject_HEAD_INIT(NULL, 0)
	"PyQuadoo.Attribute",			// tp_name
	sizeof(PyQuadooAttribute),		// tp_basicsize
	0,								// tp_itemsize
	(destructor)PyQuadooAttribute_dealloc,	// tp_dealloc
	0, 0, 0, 0, 0,					// standard method slots
	0, 0, 0, 0,						//
	(ternaryfunc)PyQuadooAttribute_call,	// tp_call
	0,								// tp_str
	0,								// tp_getattro
	0,								// tp_setattro
	0,								// tp_as_buffer
	Py_TPFLAGS_DEFAULT,				// tp_flags
	"QuadooScript Attribute",		// tp_doc
	0, 0, 0, 0, 0, 0,
	g_pyQuadooAttributeMethods
};

PyTypeObject* PY_QUADOO_ATTRIBUTE (VOID)
{
	return &g_pyQuadooAttribute;
}
