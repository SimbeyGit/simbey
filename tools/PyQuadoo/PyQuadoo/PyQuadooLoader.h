#pragma once

#include "PyQuadoo.h"

struct PyQuadooLoader
{
	PyObject_HEAD
	IQuadooInstanceLoader* pLoader;
	IQuadooObject* pLastConstructorException;
};

PyTypeObject* PY_QUADOO_LOADER (VOID);
