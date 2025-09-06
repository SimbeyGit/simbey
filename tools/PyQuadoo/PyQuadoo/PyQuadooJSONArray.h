#pragma once

#include "Published\JSON.h"
#include "PyQuadoo.h"

struct PyQuadooJSONArray
{
	PyObject_HEAD
	PyObject* pyModule;
	IJSONArray* pJSONArray;
};

PyTypeObject* PY_QUADOO_JSONARRAY (VOID);
