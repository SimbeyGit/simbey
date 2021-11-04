#pragma once

#include "Library\Core\BaseUnknown.h"
#include "Published\QuadooVM.h"

class CQSDemo :
	public TBaseUnknown<CQSDemo>,
	public CQuadooObjectImpl
{
public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooObject)
	END_UNK_MAP

public:
	CQSDemo ();
	~CQSDemo ();

	// IQuadooObject
	virtual HRESULT STDMETHODCALLTYPE Invoke (__in_opt IQuadooVM* pVM, RSTRING rstrMethod, QuadooVM::QVPARAMS* pqvParams, __out QuadooVM::QVARIANT* pqvResult);
};
