#pragma once

#include "PyQuadoo.h"

struct PyQuadooObject
{
	PyObject_HEAD
	PyObject* pyModule;
	IQuadooObject* pObject;
};

PyTypeObject* PY_QUADOO_OBJECT (VOID);
