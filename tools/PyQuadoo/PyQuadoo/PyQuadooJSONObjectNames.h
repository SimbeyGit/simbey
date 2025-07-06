#pragma once

#include "Published\JSON.h"
#include "PyQuadoo.h"

struct PyQuadooJSONObjectNames
{
	PyObject_HEAD
	IJSONObject* pJSONObject;
};

PyTypeObject* PY_QUADOO_JSONOBJECT_NAMES (VOID);
