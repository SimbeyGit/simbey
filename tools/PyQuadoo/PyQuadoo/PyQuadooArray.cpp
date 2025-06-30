#include "WinPython.h"
#include "Library\Core\CoreDefs.h"
#include "Library\Sorting.h"
#include "PyQuadooArray.h"

struct SORT_DATA
{
	INT (WINAPI* pfnCallback)(QuadooVM::QVARIANT* pqvLeft, QuadooVM::QVARIANT* pqvRight, PVOID pvParam);
	PVOID pvParam;
};

CPyQuadooArray::CPyQuadooArray (PyObject* pyList) :
	m_pyList(pyList)
{
	Py_XINCREF(m_pyList);
}

CPyQuadooArray::~CPyQuadooArray ()
{
	Py_XDECREF(m_pyList);
}

// IQuadooContainer

sysint STDMETHODCALLTYPE CPyQuadooArray::Length (VOID)
{
	return static_cast<sysint>(PyList_Size(m_pyList));
}

HRESULT STDMETHODCALLTYPE CPyQuadooArray::SetItem (sysint nItem, const QuadooVM::QVARIANT* pqv)
{
	HRESULT hr;
	PyObject* pyObject = NULL;

	Check(QuadooToPython(pqv, &pyObject));
	CheckIf(0 != PyList_SetItem(m_pyList, nItem, pyObject), DISP_E_BADINDEX);
	pyObject = NULL;	// Now owned by the list

Cleanup:
	if(pyObject)
		Py_XDECREF(pyObject);
	return hr;
}

HRESULT STDMETHODCALLTYPE CPyQuadooArray::GetItem (sysint nItem, __out QuadooVM::QVARIANT* pqv)
{
	HRESULT hr;
	PyObject* pyObject = PyList_GetItem(m_pyList, nItem);

	CheckIf(NULL == pyObject, DISP_E_BADINDEX);
	Check(PythonToQuadoo(pyObject, pqv));

Cleanup:
	return hr;
}

HRESULT STDMETHODCALLTYPE CPyQuadooArray::RemoveItem (sysint nItem, __out_opt QuadooVM::QVARIANT* pqv)
{
	HRESULT hr;
	Py_ssize_t nSize = PyList_Size(m_pyList);

	CheckIf(nItem < 0 || nItem >= nSize, DISP_E_BADINDEX);

	if(pqv)
		Check(GetItem(nItem, pqv));

	CheckIf(0 != PySequence_DelItem(m_pyList, nItem), E_FAIL);
	hr = S_OK;

Cleanup:
	return hr;
}

HRESULT STDMETHODCALLTYPE CPyQuadooArray::Compact (VOID)
{
	return S_FALSE;
}

VOID STDMETHODCALLTYPE CPyQuadooArray::Clear (VOID)
{
	PyList_SetSlice(m_pyList, 0, PyList_Size(m_pyList), NULL);
}

// IQuadooArray

HRESULT STDMETHODCALLTYPE CPyQuadooArray::InsertAt (const QuadooVM::QVARIANT* pqv, sysint nInsert)
{
	HRESULT hr;
	PyObject* pyObject = NULL;

	if(nInsert < 0)
		nInsert = 0;
	else
	{
		Py_ssize_t nSize = PyList_Size(m_pyList);
		if(nInsert > nSize)
			nInsert = nSize;
	}

	Check(QuadooToPython(pqv, &pyObject));
	CheckIf(0 != PyList_Insert(m_pyList, nInsert, pyObject), E_FAIL);

Cleanup:
	Py_XDECREF(pyObject);	// PyList_Insert() did not steal our reference
	return hr;
}

HRESULT STDMETHODCALLTYPE CPyQuadooArray::Append (const QuadooVM::QVARIANT* pqv)
{
	HRESULT hr;
	PyObject* pyObject = NULL;

	Check(QuadooToPython(pqv, &pyObject));
	CheckIf(0 != PyList_Append(m_pyList, pyObject), DISP_E_BADINDEX);
	pyObject = NULL;

Cleanup:
	if(pyObject)
		Py_XDECREF(pyObject);
	return hr;
}

HRESULT STDMETHODCALLTYPE CPyQuadooArray::Splice (sysint nInsertAt, sysint cRemove, const QuadooVM::QVARIANT* pqv)
{
	HRESULT hr;
	Py_ssize_t nSize = PyList_Size(m_pyList);
	QuadooVM::QVARIANT qv; qv.eType = QuadooVM::Null;
	PyObject* pyItem = NULL;

	if(nInsertAt < 0)
		nInsertAt = 0;
	else if(nInsertAt > nSize)
		nInsertAt = nSize;

	if(cRemove < 0)
		cRemove = 0;
	else if(cRemove > nSize)
		cRemove = nSize;

	CheckIf(QuadooVM::Array != pqv->eType, DISP_E_BADVARTYPE);

	if(0 < cRemove)
	{
		// Remove the slice: del m_pyList[nInsertAt : nInsertAt + cRemove]
		CheckIf(PyList_SetSlice(m_pyList, nInsertAt, nInsertAt + cRemove, NULL) < 0, E_FAIL);
	}

	sysint cItems = pqv->pArray->Length();
	for(sysint i = 0; i < cItems; i++)
	{
		Check(pqv->pArray->GetItem(i, &qv));
		Check(QuadooToPython(&qv, &pyItem));
		QVMClearVariant(&qv);

		CheckIf(PyList_Insert(m_pyList, nInsertAt + i, pyItem) < 0, E_FAIL);
		Py_DECREF(pyItem);
		pyItem = NULL;
	}

	hr = S_OK;

Cleanup:
	Py_DECREF(pyItem);
	QVMClearVariant(&qv);
	return hr;
}

HRESULT STDMETHODCALLTYPE CPyQuadooArray::Slice (sysint nBegin, sysint nEnd, __out QuadooVM::QVARIANT* pqv)
{
	HRESULT hr;
	Py_ssize_t nSize = PyList_Size(m_pyList);

	if(nBegin < 0)
		nBegin = 0;
	if(nEnd > nSize)
		nEnd = nSize;
	if(nEnd < nBegin)
		nEnd = nBegin;

	PyObject* pySlice = PyList_GetSlice(m_pyList, nBegin, nEnd);
	CheckIf(NULL == pySlice, E_FAIL);

	pqv->pArray = __new CPyQuadooArray(pySlice);
	CheckAlloc(pqv->pArray);
	hr = S_OK;

Cleanup:
	Py_DECREF(pySlice);
	return hr;
}

HRESULT STDMETHODCALLTYPE CPyQuadooArray::Swap (sysint nItemA, sysint nItemB)
{
	HRESULT hr;
	Py_ssize_t nSize = PyList_Size(m_pyList);

	CheckIf(nItemA < 0 || nItemA >= nSize || nItemB < 0 || nItemB >= nSize, E_INVALIDARG);
	CheckIfIgnore(nItemA == nItemB, S_FALSE);

	// Get borrowed references
	PyObject* itemA = PyList_GetItem(m_pyList, nItemA);
	PyObject* itemB = PyList_GetItem(m_pyList, nItemB);
	CheckIf(NULL == itemA || NULL == itemB, E_FAIL);

	// PyList_SetItem steals a reference, so INCREF beforehand
	Py_INCREF(itemB);
	Py_INCREF(itemA);

	if(PyList_SetItem(m_pyList, nItemA, itemB) < 0)
	{
		Py_DECREF(itemA);
		hr = E_FAIL;
		goto Cleanup;
	}

	if(PyList_SetItem(m_pyList, nItemB, itemA) < 0)
	{
		// We already swapped itemA out; avoid leak
		PyList_SetItem(m_pyList, nItemA, itemA); // undo
		Py_DECREF(itemB); // undo
		hr = E_FAIL;
		goto Cleanup;
	}

	hr = S_OK;

Cleanup:
	return hr;
}

HRESULT STDMETHODCALLTYPE CPyQuadooArray::Sort (INT (WINAPI* pfnCallback)(QuadooVM::QVARIANT* pqvLeft, QuadooVM::QVARIANT* pqvRight, PVOID pvParam), PVOID pvParam)
{
	HRESULT hr;
	Py_ssize_t nSize = PyList_Size(m_pyList);
	PyObject** rgPyObjects = __new PyObject*[nSize];
	SORT_DATA SortData;

	CheckAlloc(rgPyObjects);

	for(Py_ssize_t i = 0; i < nSize; i++)
	{
		rgPyObjects[i] = PyList_GetItem(m_pyList, i);
		Assert(rgPyObjects[i]);
		Py_INCREF(rgPyObjects[i]);
	}

	SortData.pfnCallback = pfnCallback;
	SortData.pvParam = pvParam;

	Sorting::TQuickSort(rgPyObjects, nSize, _SortPython, &SortData);
	for(Py_ssize_t i = 0; i < nSize; i++)
	{
		SideAssert(0 != PyList_SetItem(m_pyList, i, rgPyObjects[i]));
		rgPyObjects[i] = NULL;	// Now owned by the list
	}

	hr = S_OK;

Cleanup:
	if(rgPyObjects)
	{
		for(Py_ssize_t i = 0; i < nSize; i++)
			Py_DECREF(rgPyObjects[i]);
		__delete_array rgPyObjects;
	}
	return hr;
}

INT WINAPI CPyQuadooArray::_SortPython (PyObject** ppyLeft, PyObject** ppyRight, PVOID pvParam)
{
	INT nResult;
	QuadooVM::QVARIANT qvLeft, qvRight;

	qvLeft.eType = QuadooVM::Null;
	qvRight.eType = QuadooVM::Null;

	if(SUCCEEDED(PythonToQuadoo(*ppyLeft, &qvLeft)) &&
		SUCCEEDED(PythonToQuadoo(*ppyRight, &qvRight)))
	{
		nResult = reinterpret_cast<SORT_DATA*>(pvParam)->pfnCallback(&qvLeft, &qvRight, reinterpret_cast<SORT_DATA*>(pvParam)->pvParam);
	}
	else
		nResult = static_cast<INT>(*ppyLeft - *ppyRight);

	QVMClearVariant(&qvLeft);
	QVMClearVariant(&qvRight);
	return nResult;
}
