#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\ActiveX\DLLServer.h"
#include "Library\Util\Shell.h"
#include "QSDemoModule.h"

CQSDemo::CQSDemo ()
{
	DLLAddRef();
}

CQSDemo::~CQSDemo ()
{
	DLLRelease();
}

// IQuadooObject

HRESULT STDMETHODCALLTYPE CQSDemo::Invoke (__in_opt IQuadooVM* pVM, RSTRING rstrMethod, QuadooVM::QVPARAMS* pqvParams, __out QuadooVM::QVARIANT* pqvResult)
{
	HRESULT hr;
	PCWSTR pcwzMethod = RStrToWide(rstrMethod);

	if(0 == TStrCmpAssert(pcwzMethod, L"Navigate"))
	{
		LONG_PTR nResult;
		Check(HrShellExecute(GetDesktopWindow(), L"open", L"http://www.quadooscript.com/", NULL, NULL, SW_NORMAL, nResult));
		SetSysIntToVariant(nResult, pqvResult);
	}
	else
		hr = DISP_E_MEMBERNOTFOUND;

Cleanup:
	return hr;
}

HRESULT STDMETHODCALLTYPE CQSDemo::GetProperty (__in_opt IQuadooVM* pVM, RSTRING rstrProperty, __out QuadooVM::QVARIANT* pqvResult)
{
	HRESULT hr;
	PCWSTR pcwzProperty = RStrToWide(rstrProperty);

	if(0 == TStrCmpAssert(pcwzProperty, L"TheAnswer"))
	{
		pqvResult->lVal = 42;
		pqvResult->eType = QuadooVM::I4;
		hr = S_OK;
	}
	else
		hr = DISP_E_UNKNOWNNAME;

	return hr;
}
