#pragma once

#include "Library\Core\ILockableStream.h"
#include "Library\Util\RString.h"

interface IQuadooObject;

interface __declspec(uuid("B4C0B27D-99BA-45c4-9488-0A42A1FDD349")) IQVMHost : IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE WriteData (const VOID* pcvData, DWORD cbData, __out DWORD* pcbWritten) = 0;
	virtual HRESULT STDMETHODCALLTYPE WriteText (PCWSTR pcwzText, INT cchText) = 0;
	virtual HRESULT STDMETHODCALLTYPE ReadText (PSTR pszText, INT cchText, __out DWORD* pcchText) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetRequestData (__out RSTRING* prstrContentType, __out RSTRING* prstrQueryString, __out RSTRING* prstrCookies, __deref_out_opt ILockableStream** ppData) = 0;
	virtual UINT STDMETHODCALLTYPE GetCodePage (VOID) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetCodePage (UINT nCodePage) = 0;
	virtual HRESULT STDMETHODCALLTYPE ReportException (HRESULT hrInvoke, IQuadooObject* pException) = 0;
};
