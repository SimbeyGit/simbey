#pragma once

#include "PyQuadoo.h"

struct PyQuadooMap
{
	PyObject_HEAD
	IQuadooMap* pMap;
};

PyTypeObject* PY_QUADOO_MAP (VOID);
