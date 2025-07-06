#pragma once

#include "Published\JSON.h"
#include "PyQuadoo.h"

struct PyQuadooJSONObject
{
	PyObject_HEAD
	IJSONObject* pJSONObject;
};

PyTypeObject* PY_QUADOO_JSONOBJECT (VOID);
