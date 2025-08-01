#pragma once

#include "PyQuadoo.h"

struct PyQuadooAttribute
{
	PyObject_HEAD
	IQuadooObject* pObject;
	RSTRING rstrName;
};

PyTypeObject* PY_QUADOO_ATTRIBUTE (VOID);
