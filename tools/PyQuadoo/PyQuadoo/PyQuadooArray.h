#pragma once

#include "Library\Core\BaseUnknown.h"
#include "PyQuadoo.h"

class CPyQuadooArray :
	public CBaseUnknown,
	public IQuadooArray
{
private:
	PyObject* m_pyList;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooArray)
		UNK_INTERFACE(IQuadooContainer)
	END_UNK_MAP

public:
	CPyQuadooArray (PyObject* pyList);
	~CPyQuadooArray ();

	// IQuadooContainer
	virtual sysint STDMETHODCALLTYPE Length (VOID);
	virtual HRESULT STDMETHODCALLTYPE SetItem (sysint nItem, const QuadooVM::QVARIANT* pqv);
	virtual HRESULT STDMETHODCALLTYPE GetItem (sysint nItem, __out QuadooVM::QVARIANT* pqv);
	virtual HRESULT STDMETHODCALLTYPE RemoveItem (sysint nItem, __out_opt QuadooVM::QVARIANT* pqv);
	virtual HRESULT STDMETHODCALLTYPE Compact (VOID);
	virtual VOID STDMETHODCALLTYPE Clear (VOID);

	// IQuadooArray
	virtual HRESULT STDMETHODCALLTYPE InsertAt (const QuadooVM::QVARIANT* pqv, sysint nInsert);
	virtual HRESULT STDMETHODCALLTYPE Append (const QuadooVM::QVARIANT* pqv);
	virtual HRESULT STDMETHODCALLTYPE Splice (sysint nInsertAt, sysint cRemove, const QuadooVM::QVARIANT* pqv);
	virtual HRESULT STDMETHODCALLTYPE Slice (sysint nBegin, sysint nEnd, __out QuadooVM::QVARIANT* pqv);
	virtual HRESULT STDMETHODCALLTYPE Swap (sysint nItemA, sysint nItemB);
	virtual HRESULT STDMETHODCALLTYPE Sort (INT (WINAPI* pfnCallback)(QuadooVM::QVARIANT* pqvLeft, QuadooVM::QVARIANT* pqvRight, PVOID pvParam), PVOID pvParam);

private:
	static INT WINAPI _SortPython (PyObject** ppyLeft, PyObject** ppyRight, PVOID pvParam);
};
