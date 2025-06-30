#include "WinPython.h"
#include "Library\Core\CoreDefs.h"
#include "Library\Core\RStrMap.h"
#include "Library\Core\MemoryStream.h"
#include "Library\Util\Formatting.h"
#include "Published\QuadooObject.inc"
#include "PyQuadooAttribute.h"
#include "PyQuadooObject.h"
#include "PyQuadooVM.h"
#include "PyQuadooLoader.h"
#include "PyQuadooArray.h"

typedef HRESULT (__stdcall* PFNGETCLASSOBJECT)(REFCLSID, REFIID, LPVOID);

// Global/static cache if desired
static PyObject* g_pyDecimalType = NULL;

class CNativeModule
{
private:
	HMODULE m_hModule;

public:
	CNativeModule (HMODULE hModule) : m_hModule(hModule)
	{
	}

	~CNativeModule ()
	{
		FreeLibrary(m_hModule);
	}

	static HRESULT Create (RSTRING rstrModule, __deref_out CNativeModule** ppModule)
	{
		HRESULT hr;
		HMODULE hModule = LoadLibrary(RStrToWide(rstrModule));

		CheckIfGetLastError(NULL == hModule);

		*ppModule = __new CNativeModule(hModule);
		CheckAlloc(*ppModule);
		hModule = NULL;

		hr = S_OK;

	Cleanup:
		if(hModule)
			FreeLibrary(hModule);
		return hr;
	}

	HRESULT Load (const GUID& guidClass, __deref_out PyObject** ppyObject)
	{
		HRESULT hr;
		PFNGETCLASSOBJECT pfnGetClassObject;
		TStackRef<IClassFactory> srFactory;
		TStackRef<IQuadooObject> srObject;
		PyQuadooObject* pyObject;

		Check(TGetFunction(m_hModule, "DllGetClassObject", &pfnGetClassObject));
		Check(pfnGetClassObject(guidClass, IID_PPV_ARGS(&srFactory)));
		Check(srFactory->CreateInstance(NULL, IID_PPV_ARGS(&srObject)));

		pyObject = PyObject_New(PyQuadooObject, PY_QUADOO_OBJECT());
		CheckAlloc(pyObject);

		pyObject->pObject = srObject.Detach();
		*ppyObject = (PyObject*)pyObject;

	Cleanup:
		return hr;
	}
};

TRStrMap<CNativeModule*> g_mapModules;

HRESULT ConvertDecimalToString (LONGLONG llDecimal, INT cPlaces, PSTR pszDecimal, INT cchMaxDecimal, __out INT* pcchDecimal)
{
	HRESULT hr;
	INT nNegative = 0, cLength, cDecimal;
	CHAR *pszPtr = pszDecimal, szTemp[32];

	if(llDecimal < 0)
	{
		*pszPtr = '-';
		pszPtr++;
		llDecimal = -llDecimal;
		nNegative = 1;
	}
	Check(Formatting::TInt64ToAsc(llDecimal, szTemp, ARRAYSIZE(szTemp), 10, &cLength));
	if(cLength <= 4)
	{
		INT i = 2, c;
		pszPtr[0] = '0';
		pszPtr[1] = '.';
		for(c = cLength; c < 4; c++)
			pszPtr[i++] = '0';
		Check(TStrCchCpy(pszPtr + i, cchMaxDecimal - static_cast<INT>((pszPtr + i) - pszDecimal), szTemp));
		cDecimal = 1;
		cLength = 6;
	}
	else
	{
		INT c = cLength - 4;
		Check(TStrCchCpyN(pszPtr, cchMaxDecimal - static_cast<INT>(pszPtr - pszDecimal), szTemp, c));
		pszPtr[c] = '.';
		pszPtr[c+1] = szTemp[cLength - 4];
		pszPtr[c+2] = szTemp[cLength - 3];
		pszPtr[c+3] = szTemp[cLength - 2];
		pszPtr[c+4] = szTemp[cLength - 1];
		cDecimal = c;
		cLength++;
	}
	if(cPlaces == 0)
	{
		pszPtr[cDecimal] = '\0';
		cLength -= 5;
	}
	else
	{
		pszPtr[cDecimal + cPlaces + 1] = '\0';
		cLength -= (4 - cPlaces);
	}

	*pcchDecimal = nNegative + cLength;

Cleanup:
	return hr;
}

bool EnsureDecimalType (VOID)
{
	if(!g_pyDecimalType)
	{
		PyObject* pyDecimalModule = PyImport_ImportModule("decimal");
		if(!pyDecimalModule)
			return false;

		g_pyDecimalType = PyObject_GetAttrString(pyDecimalModule, "Decimal");
		Py_DECREF(pyDecimalModule);

		if(!g_pyDecimalType)
			return false;
	}
	return true;
}

HRESULT PythonToRSTRING (PyObject* pyValue, __deref_out RSTRING* prstrValue)
{
	HRESULT hr;
	Py_ssize_t cchValue;
	PWSTR pwzValue = PyUnicode_AsWideCharString(pyValue, &cchValue);

	if(pwzValue)
	{
#ifdef	_WIN64
		if(cchValue <= INT_MAX)
		{
#endif
		hr = RStrCreateW(static_cast<INT>(cchValue), pwzValue, prstrValue);
#ifdef	_WIN64
		}
		else
			hr = DISP_E_OVERFLOW;
#endif
		PyMem_Free(pwzValue);

		if(FAILED(hr))
			PyErr_SetString(PyExc_RuntimeError, "Failed to allocate RSTRING");
	}
	else
	{
		PyErr_SetString(PyExc_TypeError, "Could not convert value to Unicode string");
		hr = DISP_E_BADVARTYPE;
	}

	return hr;
}

HRESULT QuadooToPython (const QuadooVM::QVARIANT* pqv, __deref_out PyObject** ppyValue)
{
	HRESULT hr;

	switch(pqv->eType)
	{
	case QuadooVM::Null:
		*ppyValue = Py_None;
		Py_INCREF(Py_None);
		break;

	case QuadooVM::I4:
		*ppyValue = PyLong_FromLong(pqv->lVal);
		CheckAlloc(*ppyValue);
		break;

	case QuadooVM::I8:
		*ppyValue = PyLong_FromLongLong(pqv->llVal);
		CheckAlloc(*ppyValue);
		break;

	case QuadooVM::Float:
		*ppyValue = PyFloat_FromDouble((DOUBLE)pqv->fltVal);
		CheckAlloc(*ppyValue);
		break;

	case QuadooVM::Double:
		*ppyValue = PyFloat_FromDouble(pqv->dblVal);
		CheckAlloc(*ppyValue);
		break;

	case QuadooVM::Currency:
		{
			CHAR szDecimal[64];
			INT cchDecimal;

			CheckIf(!EnsureDecimalType(), DISP_E_BADVARTYPE);
			Check(ConvertDecimalToString(pqv->llVal, 4, szDecimal, ARRAYSIZE(szDecimal), &cchDecimal));

			PyObject* pyString = PyUnicode_FromStringAndSize(szDecimal, cchDecimal);
			CheckAlloc(pyString);

			*ppyValue = PyObject_CallFunctionObjArgs(g_pyDecimalType, pyString, NULL);
			Py_DECREF(pyString);

			CheckAlloc(*ppyValue);
		}
		break;

	case QuadooVM::String:
		*ppyValue = PyUnicode_FromWideChar(RStrToWide(pqv->rstrVal), RStrLen(pqv->rstrVal));
		CheckAlloc(*ppyValue);
		break;

	case QuadooVM::Object:
		{
			PyQuadooObject* pyObject = PyObject_New(PyQuadooObject, PY_QUADOO_OBJECT());
			CheckAlloc(pyObject);
			SetInterface(pyObject->pObject, pqv->pObject);
			*ppyValue = (PyObject*)pyObject;
		}
		break;

	default:
		Check(DISP_E_BADVARTYPE);
	}

	hr = S_OK;

Cleanup:
	return hr;
}

HRESULT PythonToQuadoo (PyObject* pyValue, __out QuadooVM::QVARIANT* pqv)
{
	HRESULT hr = S_OK;

	if(PyLong_Check(pyValue))
	{
		// Automatically choose between I4 and I8
		LONGLONG llVal = PyLong_AsLongLong(pyValue);

		if(llVal >= INT_MIN && llVal <= INT_MAX)
		{
			pqv->eType = QuadooVM::I4;
			pqv->lVal = (LONG)llVal;
		}
		else
		{
			pqv->eType = QuadooVM::I8;
			pqv->llVal = llVal;
		}
	}
	else if(PyFloat_Check(pyValue))
	{
		pqv->eType = QuadooVM::Double;
		pqv->dblVal = PyFloat_AsDouble(pyValue);
	}
	else if(PyBool_Check(pyValue))
	{
		pqv->eType = QuadooVM::Bool;
		pqv->fVal = (pyValue == Py_True);
	}
	else if(PyUnicode_Check(pyValue))
	{
		Check(PythonToRSTRING(pyValue, &pqv->rstrVal));
		pqv->eType = QuadooVM::String;
	}
	else if(PyObject_TypeCheck(pyValue, PY_QUADOO_OBJECT()))
	{
		PyQuadooObject* pyObject = (PyQuadooObject*)pyValue;
		pqv->eType = QuadooVM::Object;
		SetInterface(pqv->pObject, pyObject->pObject);
	}
	else if(PyList_Check(pyValue))
	{
		pqv->pArray = __new CPyQuadooArray(pyValue);
		CheckAlloc(pqv->pArray);
		pqv->eType = QuadooVM::Array;
	}
	else if(pyValue == Py_None)
		pqv->eType = QuadooVM::Null;
	else if(EnsureDecimalType() && PyObject_IsInstance(pyValue, g_pyDecimalType))
	{
		PyObject* pyString = PyObject_Str(pyValue);
		CheckIf(NULL == pyString, DISP_E_BADVARTYPE);

		hr = PythonToRSTRING(pyString, &pqv->rstrVal);
		if(SUCCEEDED(hr))
		{
			pqv->eType = QuadooVM::String;

			hr = QVMConvertToCurrency(pqv);
			if(FAILED(hr))
				QVMClearVariant(pqv);
		}

		Py_DECREF(pyString);
	}
	else
		hr = DISP_E_BADVARTYPE;

Cleanup:
	return hr;
}

VOID SetHResultError (HRESULT hr)
{
	CHAR szError[64];
	SideAssertHr(Formatting::TPrintF(szError, ARRAYSIZE(szError), NULL, "HResult Failure: 0x%.8X", hr));
	PyErr_SetString(PyExc_RuntimeError, szError);
}

class CPyCompileStatus : public IQuadooCompilerStatus
{
public:
	virtual VOID STDMETHODCALLTYPE OnCompilerAddFile (PCWSTR pcwzFile, INT cchFile) {}
	virtual VOID STDMETHODCALLTYPE OnCompilerStatus (PCWSTR pcwzStatus) {}
	virtual VOID STDMETHODCALLTYPE OnCompilerError (HRESULT hrCode, INT nLine, PCWSTR pcwzFile, PCWSTR pcwzError) {}
	virtual STDMETHODIMP OnCompilerResolvePath (PCWSTR pcwzPath, __out_ecount(cchMaxAbsolutePath) PWSTR pwzAbsolutePath, INT cchMaxAbsolutePath)
	{
		return E_NOTIMPL;
	}
};

static PyObject* PyCompileScript (PyObject* self, PyObject* args)
{
	PyObject* pyResult = NULL;
	CPyCompileStatus status;
	PyObject* pyFileName = NULL;
	CMemoryStream stmByteCode;
	PWSTR pwzFileName = NULL;

	PyCheckIf(!PyArg_ParseTuple(args, "U", &pyFileName), E_INVALIDARG);

	pwzFileName = PyUnicode_AsWideCharString(pyFileName, NULL);
	PyCheckIf(NULL == pwzFileName, DISP_E_BADVARTYPE);

	PyCheck(QuadooParseToStream(pwzFileName, QUADOO_COMPILE_LINE_NUMBER_MAP, &stmByteCode, NULL, &status));

	pyResult = (PyObject*)PyBytes_FromStringAndSize(stmByteCode.TGetReadPtr<CHAR>(), stmByteCode.DataRemaining());

Cleanup:
	PyMem_Free(pwzFileName);
	return pyResult;
}

static PyObject* PyCompileText (PyObject* self, PyObject* args)
{
	PyObject* pyResult = NULL;
	CPyCompileStatus status;
	PyObject* pyScript = NULL;
	Py_ssize_t cchScript;
	CMemoryStream stmByteCode;
	PWSTR pwzScript = NULL;

	PyCheckIf(!PyArg_ParseTuple(args, "U", &pyScript), E_INVALIDARG);

	pwzScript = PyUnicode_AsWideCharString(pyScript, &cchScript);
	PyCheckIf(NULL == pwzScript, DISP_E_BADVARTYPE);

#ifdef	_WIN64
	PyCheckIf(cchScript > INT_MAX, DISP_E_OVERFLOW);
#endif

	PyCheck(QuadooParseTextToStream(NULL, 0, pwzScript, static_cast<INT>(cchScript), QUADOO_COMPILE_LINE_NUMBER_MAP, &stmByteCode, NULL, &status));
	pyResult = (PyObject*)PyBytes_FromStringAndSize(stmByteCode.TGetReadPtr<CHAR>(), stmByteCode.DataRemaining());

Cleanup:
	PyMem_Free(pwzScript);
	return pyResult;
}

static PyObject* PyCreateLoader (PyObject* self, PyObject* args)
{
	PyObject* pyResult = NULL;
	PyQuadooLoader* pyLoader;
	TStackRef<IQuadooInstanceLoader> srLoader;

	PyCheck(QVMCreateLoader(NULL, &srLoader));

	pyLoader = PyObject_New(PyQuadooLoader, PY_QUADOO_LOADER());
	PyCheckAlloc(pyLoader);

	pyLoader->pLoader = srLoader.Detach();
	pyLoader->pLastConstructorException = NULL;
	pyResult = (PyObject*)pyLoader;

Cleanup:
	return pyResult;
}

static PyObject* PyLoadModule (PyObject* self, PyObject* args)
{
	CNativeModule* pModule;
	Py_ssize_t cArgs = PyTuple_Size(args);
	PyObject* pyResult = NULL, *pyModule;
	CRString rsModule, rsClass;
	GUID guidClass;

	if(1 == cArgs)
	{
		PyCheckIf(!PyArg_ParseTuple(args, "U", &pyModule), E_INVALIDARG);
		guidClass = CLSID_QuadooObject;
	}
	else
	{
		PyObject* pyClass;

		PyCheckIf(!PyArg_ParseTuple(args, "UU", &pyModule, &pyClass), E_INVALIDARG);
		PyCheck(PythonToRSTRING(pyClass, &rsClass));

		if(FAILED(IIDFromString(const_cast<LPOLESTR>(RStrToWide(*rsClass)), &guidClass)))
		{
			// If a GUID wasn't provided, then next check for a program ID.
			PyCheck(CLSIDFromProgID(RStrToWide(*rsClass), &guidClass));
		}
	}

	PyCheck(PythonToRSTRING(pyModule, &rsModule));

	if(FAILED(g_mapModules.Find(rsModule, &pModule)))
	{
		PyCheck(CNativeModule::Create(rsModule, &pModule));
		
		HRESULT hrAdd = g_mapModules.Add(rsModule, pModule);
		if(FAILED(hrAdd))
		{
			__delete pModule;
			PyCheck(hrAdd);
		}
	}

	PyCheck(pModule->Load(guidClass, &pyResult));

Cleanup:
	return pyResult;
}

static PyMethodDef g_rgMethods[] =
{
	{ "CompileScript", PyCompileScript, METH_VARARGS, "Compile a QuadooScript file and return bytecode" },
	{ "CompileText", PyCompileText, METH_VARARGS, "Compile QuadooScript text and return bytecode" },
	{ "CreateLoader", PyCreateLoader, METH_NOARGS, "Return a new QuadooScript VM loader" },
	{ "LoadModule", PyLoadModule, METH_VARARGS, "Load an external QuadooScript module" },
	{ NULL, NULL, 0, NULL }
};

static PyModuleDef g_pyModule =
{
	PyModuleDef_HEAD_INIT,
	"PyQuadoo",
	"Python wrapper for QuadooScript",
	-1,
	g_rgMethods
};

static VOID PyQuadooCleanup (VOID)
{
	g_mapModules.DeleteAll();

	if(g_pyDecimalType)
	{
		Py_DECREF(g_pyDecimalType);
		g_pyDecimalType = NULL;
	}
}

PyMODINIT_FUNC PyInit_PyQuadoo (VOID)
{
	if(PyType_Ready(PY_QUADOO_ATTRIBUTE()) < 0)
		return NULL;
	if(PyType_Ready(PY_QUADOO_OBJECT()) < 0)
		return NULL;
	if(PyType_Ready(PY_QUADOO_VM()) < 0)
		return NULL;
	if(PyType_Ready(PY_QUADOO_LOADER()) < 0)
		return NULL;

	PyObject* pModule = PyModule_Create(&g_pyModule);

	Py_INCREF(PY_QUADOO_ATTRIBUTE());
	PyModule_AddObject(pModule, "Attribute", (PyObject*)PY_QUADOO_ATTRIBUTE());

	Py_INCREF(PY_QUADOO_OBJECT());
	PyModule_AddObject(pModule, "Object", (PyObject*)PY_QUADOO_OBJECT());

	Py_INCREF(PY_QUADOO_VM());
	PyModule_AddObject(pModule, "VM", (PyObject*)PY_QUADOO_VM());

	Py_INCREF(PY_QUADOO_LOADER());
	PyModule_AddObject(pModule, "Loader", (PyObject*)PY_QUADOO_LOADER());

	Py_AtExit(PyQuadooCleanup);

	return pModule;
}
