#pragma once

#include "Library\Core\BaseUnknown.h"
#include "PyQuadoo.h"

class CPyPrintTarget :
	public CBaseUnknown,
	public IQuadooPrintTarget
{
private:
	PyObject* m_pyPrint, *m_pyPrintLn;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooPrintTarget)
	END_UNK_MAP

public:
	CPyPrintTarget (PyObject* pyPrint, PyObject* pyPrintLn);
	~CPyPrintTarget ();

	// IQuadooPrintTarget
	virtual HRESULT STDMETHODCALLTYPE Print (RSTRING rstrText);
	virtual HRESULT STDMETHODCALLTYPE PrintLn (RSTRING rstrText);

private:
	HRESULT CallTarget (PyObject* pyTarget, RSTRING rstrText);
};
