#pragma once

#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "..\WQVM\QVMWinHost.h"
#include "Published\WQVMInterfaces.h"
#include "..\..\..\..\..\platform\QuadooForm\ScriptAdapter\ApplicationRuntime.h"
#include "PluginComponent.h"

class CAQVMHost :
	public CQVMWinHost,
	public IExternalClassLoader,
	public IQVMMessageHandler
{
private:
	TStackRef<AQVM::CApplicationRuntime> m_runtime;
	TStackRef<IPluginModuleLoader> m_pluginLoader;
	TStackRef<IQuadooComponentRegistry> m_pluginRegistry;
	BOOL m_loaderReady;
	BOOL m_pluginRegistered;
	TStackRef<IJSONArray> m_deployedModules;
	CRString m_moduleDirectory;
	HRESULT ResolveDeployedModule(RSTRING name, CRString& path);
	TStackRef<CMemoryStream> m_asyncSound;
	DWORD m_soundThread;
	BOOL m_soundBusy;
	BOOL m_soundPlaying;
	BOOL m_soundClosed;
	HRESULT PlayHostSound(QuadooVM::QVPARAMS* parameters, QuadooVM::QVARIANT* result, BOOL asynchronous);
	VOID StopApplicationSound();

protected:
	// Native executable helper, also overridden by the non-audible Host tests.
	virtual BOOL CallPlaySound(PCWSTR sound, DWORD flags);

public:
	IMP_UNKNOWN(CQVMWinHost)
	BEGIN_UNK_MAP
		UNK_INTERFACE(IExternalClassLoader)
		UNK_INTERFACE(IQVMMessageHandler)
		UNK_CHAIN_TO(CQVMWinHost)
	END_UNK_MAP

public:
	CAQVMHost();
	~CAQVMHost();
	VOID AttachLoader(IQuadooInstanceLoader* loader);
	HRESULT InitializeApplication (HINSTANCE module, IJSONObject* catalog = NULL, IQuadooControlRegistry* registry = NULL,
		AQVM::IDefinitionSource* definitions = NULL);
	HRESULT LoadPluginModule(RSTRING path, IQuadooObject** object);
	VOID ShutdownApplication ();
	AQVM::CApplicationRuntime* Runtime ();

	// IExternalClassLoader
	STDMETHOD(LoadExternalClass)(RSTRING className, IQuadooObject* script, IQuadooObject** object);

	// IQuadooObject
	STDMETHOD(Invoke)(IQuadooVM* vm, RSTRING method, QuadooVM::QVPARAMS* parameters, QuadooVM::QVARIANT* result);

	// IQVMMessageHandler
	STDMETHOD_(BOOL,OnHandleMessage)(const MSG* message);
};
