#include <windows.h>

#if _MSC_VER < 1600
	#include <intrin.h>
#endif

#define	PY_SSIZE_T_CLEAN
#pragma warning(disable: 4190)
#include <Python.h>
#pragma warning(default: 4190)
