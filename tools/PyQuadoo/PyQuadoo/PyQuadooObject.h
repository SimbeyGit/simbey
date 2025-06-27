#pragma once

#include "PyQuadoo.h"

struct PyQuadooObject
{
	PyObject_HEAD
	IQuadooObject* pObject;
};

PyTypeObject* PY_QUADOO_OBJECT (VOID);
