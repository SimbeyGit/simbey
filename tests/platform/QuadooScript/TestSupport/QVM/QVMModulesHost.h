#pragma once

#include "QVMBaseHost.h"

class CHostedModules
{
public:
	struct SPEC
	{
		RSTRING rstrName;
		RSTRING rstrPath;
	};

	TArray<SPEC> m_aModules;

public:
	CHostedModules ();
	~CHostedModules ();

	HRESULT AddModule (PCWSTR pcwzName, PCWSTR pcwzPath);
};

class CQVMModulesHost : public CQVMBaseHost
{
protected:
	IQuadooArray* m_pArgs;
	RSTRING m_rstrHostPathTranslated;

public:
	CQVMModulesHost ();
	virtual ~CQVMModulesHost ();

	virtual HRESULT Initialize (CHostedModules* pModules, RSTRING rstrSource, INT cArgs, WCHAR* pwzArgs[]) = 0;

	HRESULT LoadHostedModules (CHostedModules* pModules, IQuadooVM* pVM);

	// IQuadooObject
	virtual HRESULT STDMETHODCALLTYPE Invoke (__in_opt IQuadooVM* pVM, RSTRING rstrMethod, QuadooVM::QVPARAMS* pqvParams, __out QuadooVM::QVARIANT* pqvResult);
	virtual HRESULT STDMETHODCALLTYPE GetProperty (__in_opt IQuadooVM* pVM, RSTRING rstrProperty, __out QuadooVM::QVARIANT* pqvResult);

	// CQVMBaseHost
	virtual HMODULE GetHostModule (VOID);

protected:
	HRESULT SetArgs (RSTRING rstrSource, INT cArgs, WCHAR* pwzArgs[], INT nArgStart);
	HRESULT InitializeHostPath (RSTRING rstrSource);

	sysint FindParam (RSTRING rstrParam);
	HRESULT FindParamValues (sysint nFirst, __out QuadooVM::QVARIANT* pqvResult);

	static HRESULT RegisterModule (PCWSTR pcwzType, BOOL fRegister, QuadooVM::QVPARAMS* pqvParams, QuadooVM::QVARIANT* pqvResult);
};
