#pragma once

#include "PyQuadoo.h"

struct PyQuadooVM
{
	PyObject_HEAD
	IQuadooVM* pVM;
};

PyTypeObject* PY_QUADOO_VM (VOID);
