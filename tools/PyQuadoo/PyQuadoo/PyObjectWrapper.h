#pragma once

#include "Library\Core\BaseUnknown.h"
#include "PyQuadoo.h"

class __declspec(uuid("14658D7C-14B1-46ba-98CE-8468CFA7A8CB")) CPyObjectWrapper :
	public CBaseUnknown,
	public CQuadooObjectImpl
{
private:
	PyObject* m_pyObject;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(CPyObjectWrapper)
		UNK_INTERFACE(IQuadooObject)
	END_UNK_MAP

public:
	CPyObjectWrapper (PyObject* pyObject);
	~CPyObjectWrapper ();

	inline PyObject* GetPyObject (VOID) { return m_pyObject; }

	// IQuadooObject
	virtual HRESULT STDMETHODCALLTYPE Invoke (__in_opt IQuadooVM* pVM, RSTRING rstrMethod, QuadooVM::QVPARAMS* pqvParams, __out QuadooVM::QVARIANT* pqvResult);
};
