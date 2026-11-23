#pragma once

#include "Library\Core\MemoryStream.h"
#include "Published\JSON.h"
#include "Published\QuadooParser.h"
#include "..\QVM\QVMModulesHost.h"

class CQVMMessagePump;

class CQVMWinHost : public CQVMModulesHost
{
private:
	CQVMMessagePump* m_pActivePump;	// Not reference counted

public:
	CQVMWinHost ();
	virtual ~CQVMWinHost ();

	virtual HRESULT Initialize (CHostedModules* pModules, RSTRING rstrSource, INT cArgs, WCHAR* pwzArgs[]);

	// IQuadooObject
	virtual HRESULT STDMETHODCALLTYPE Invoke (__in_opt IQuadooVM* pVM, RSTRING rstrMethod, QuadooVM::QVPARAMS* pqvParams, __out QuadooVM::QVARIANT* pqvResult);
	virtual HRESULT STDMETHODCALLTYPE GetProperty (__in_opt IQuadooVM* pVM, RSTRING rstrProperty, __out QuadooVM::QVARIANT* pqvResult);
	virtual HRESULT STDMETHODCALLTYPE SetProperty (__in_opt IQuadooVM* pVM, RSTRING rstrProperty, QuadooVM::QVARIANT* pqv);

	// IQVMHost
	virtual HRESULT STDMETHODCALLTYPE WriteData (const VOID* pcvData, DWORD cbData, __out DWORD* pcbWritten);
	virtual HRESULT STDMETHODCALLTYPE WriteText (PCWSTR pcwzText, INT cchText);
	virtual HRESULT STDMETHODCALLTYPE ReadText (PSTR pszText, INT cchText, __out DWORD* pcchText);
	virtual HRESULT STDMETHODCALLTYPE GetRequestData (__out RSTRING* prstrContentType, __out RSTRING* prstrQueryString, __out RSTRING* prstrCookies, __deref_out_opt ILockableStream** ppData);
	virtual UINT STDMETHODCALLTYPE GetCodePage (VOID);
	virtual HRESULT STDMETHODCALLTYPE SetCodePage (UINT nCodePage);
	virtual HRESULT STDMETHODCALLTYPE ReportException (HRESULT hrInvoke, IQuadooObject* pException);

	virtual HRESULT MapPath (RSTRING rstrPath, __out RSTRING* prstrAbsolute);

	CQVMMessagePump* SetMessagePump (CQVMMessagePump* pPump);
	HRESULT ChangeNotify (QuadooVM::QVPARAMS* pqvParams);
};
