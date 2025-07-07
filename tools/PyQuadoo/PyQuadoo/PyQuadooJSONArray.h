#pragma once

#include "Published\JSON.h"
#include "PyQuadoo.h"

struct PyQuadooJSONArray
{
	PyObject_HEAD
	IJSONArray* pJSONArray;
};

PyTypeObject* PY_QUADOO_JSONARRAY (VOID);
