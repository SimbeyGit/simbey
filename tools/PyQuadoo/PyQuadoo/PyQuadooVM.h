#pragma once

#include "PyQuadoo.h"

struct PyQuadooVM
{
	PyObject_HEAD
	PyObject* pyModule;
	IQuadooVM* pVM;
};

PyTypeObject* PY_QUADOO_VM (VOID);
