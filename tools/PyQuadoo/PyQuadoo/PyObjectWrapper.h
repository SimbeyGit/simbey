#pragma once

#include "Library\Core\BaseUnknown.h"
#include "PyQuadoo.h"

class __declspec(uuid("14658D7C-14B1-46ba-98CE-8468CFA7A8CB")) CPyObjectWrapper :
	public CBaseUnknown,
	public CQuadooObjectImpl
{
private:
	PyObject* m_pyModule;
	PyObject* m_pyObject;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(CPyObjectWrapper)
		UNK_INTERFACE(IQuadooObject)
	END_UNK_MAP

public:
	CPyObjectWrapper (PyObject* pyModule, PyObject* pyObject);
	~CPyObjectWrapper ();

	inline PyObject* GetPyObject (VOID) { return m_pyObject; }

	// IQuadooObject
	virtual HRESULT STDMETHODCALLTYPE Invoke (__in_opt IQuadooVM* pVM, RSTRING rstrMethod, QuadooVM::QVPARAMS* pqvParams, __out QuadooVM::QVARIANT* pqvResult);
	virtual HRESULT STDMETHODCALLTYPE GetProperty (__in_opt IQuadooVM* pVM, RSTRING rstrProperty, __out QuadooVM::QVARIANT* pqvResult);
	virtual HRESULT STDMETHODCALLTYPE GetIndexedProperty (__in_opt IQuadooVM* pVM, RSTRING rstrProperty, QuadooVM::QVARIANT* pqvIndex, __out QuadooVM::QVARIANT* pqvResult);
	virtual HRESULT STDMETHODCALLTYPE SetProperty (__in_opt IQuadooVM* pVM, RSTRING rstrProperty, QuadooVM::QVARIANT* pqv);
	virtual HRESULT STDMETHODCALLTYPE SetIndexedProperty (__in_opt IQuadooVM* pVM, RSTRING rstrProperty, QuadooVM::QVARIANT* pqvIndex, QuadooVM::QVARIANT* pqv);
	virtual HRESULT STDMETHODCALLTYPE DeleteProperty (RSTRING rstrProperty, __out_opt QuadooVM::QVARIANT* pqv);

private:
	HRESULT GetAttribute (RSTRING rstrAttribute, __deref_out PyObject** ppyObject);
};
