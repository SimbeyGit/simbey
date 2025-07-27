#pragma once

#include "Library\Core\BaseUnknown.h"
#include "PyQuadoo.h"

class CPyInputSource :
	public CBaseUnknown,
	public IQuadooInputSource
{
private:
	PyObject* m_pyRead;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooInputSource)
	END_UNK_MAP

public:
	CPyInputSource (PyObject* pyRead);
	~CPyInputSource ();

	// IQuadooInputSource
	virtual HRESULT STDMETHODCALLTYPE Read (__inout QuadooVM::QVARIANT* pqvRead);
};
