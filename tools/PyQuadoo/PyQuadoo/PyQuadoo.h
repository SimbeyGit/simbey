#pragma once

#include "Published\QuadooParser.h"
#include "Published\QuadooVM.h"

HRESULT PythonToRSTRING (PyObject* pyValue, __deref_out RSTRING* prstrValue);
HRESULT QuadooToPython (PyObject* pyModule, const QuadooVM::QVARIANT* pqv, __deref_out PyObject** ppyValue);
HRESULT PythonToQuadoo (PyObject* pyModule, PyObject* pyValue, __out QuadooVM::QVARIANT* pqv);

HRESULT JSONToPython (PyObject* pyModule, __in_opt IJSONValue* pvJSON, __deref_out PyObject** ppyValue);
HRESULT PythonToJSON (PyObject* pyModule, PyObject* pyValue, __deref_out IJSONValue** ppvJSON);

PyObject* GetSysCallPendingCapsule (PyObject* pyModule);

VOID SetHResultError (HRESULT hr);

#define	PyCheck(x) \
	BEGIN_MULTI_LINE_MACRO \
		HRESULT hr = x; \
		if(FAILED(hr)) \
		{ \
			SetHResultError(hr); \
			goto Cleanup; \
		} \
	END_MULTI_LINE_MACRO

#define	PyCheckIf(x, y) \
	BEGIN_MULTI_LINE_MACRO \
		if(x) \
		{ \
			SetHResultError(y); \
			goto Cleanup; \
		} \
	END_MULTI_LINE_MACRO

#define	PyCheckIfTypeMsg(x, msg) \
	BEGIN_MULTI_LINE_MACRO \
		if(x) \
		{ \
			PyErr_SetString(PyExc_TypeError, msg); \
			goto Cleanup; \
		} \
	END_MULTI_LINE_MACRO

#define PyCheckAlloc(x) \
	BEGIN_MULTI_LINE_MACRO \
		if(NULL == x) \
		{ \
			pyResult = PyErr_NoMemory(); \
			goto Cleanup; \
		} \
	END_MULTI_LINE_MACRO
