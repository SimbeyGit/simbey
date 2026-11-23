#pragma once

#include "ApplicationResources.h"
#include "..\Diagnostics.h"
#include "..\Published\QuadooForm.h"
#include "..\..\QuadooScript\Published\QuadooVM.h"
#include "Library\Core\Array.h"

namespace AQVM
{
	interface IDefinitionSource
	{
		virtual HRESULT Lookup(RSTRING className, IJSONObject** definition) = 0;
	};
	class CFormInstance;
	class CComponentObject;
	class CApplicationLink;
	class CAsyncCalls;
	class CInstanceLink;
	struct ApplicationRoot;

	// Private adapter capability. No published QuadooForm/VM ABI is changed.
	interface __declspec(uuid("4285365C-C43F-4FA8-949D-E68F4209E4A1")) IComponentObject : IUnknown
	{
		STDMETHOD(GetComponent)(IQuadooComponent** component) = 0;
	};
	// Optional application-host component bridge, not a QuadooForm public ABI.
	interface __declspec(uuid("ABE90DE2-1A89-4AFA-9E75-6A45178868CB")) IScriptComponent : IUnknown
	{
		STDMETHOD(GetScriptObject)(IQuadooObject** object) = 0;
	};

	class CApplicationRuntime : public CBaseUnknown, public IQuadooConstructionReport, public QuadooPrivate::IDiagnosticReport
	{
	private:
		HINSTANCE m_module;
		IDefinitionSource* m_definitions;
		DWORD m_thread;
		TStackRef<IJSONObject> m_catalog;
		TStackRef<IQuadooControlRegistry> m_registry;
		TArray<CFormInstance*> m_instances;
		TArray<ApplicationRoot*> m_roots;
		CApplicationLink* m_lifetimeLink;
		CAsyncCalls* m_async;
		HWND m_dispatch;
		BOOL m_checkRetry;
		BOOL m_pumping;
		BOOL m_stopping;
		BOOL m_entryActive;
		BOOL m_started;
		BOOL m_cleaning;
		BOOL m_shuttingDown;
		BOOL m_checkPending;
		BOOL m_exitCommitted;
		UINT m_creationDepth;
		UINT m_callbackDepth;
		UINT m_leases;
		ULONGLONG m_generation;
		TStackRef<IQuadooInterface> m_lifetimeCallbacks;
		ULONG m_collectOrdinal;
		HRESULT m_hrError;
		CRString m_diagnostic;
		HRESULT InitializeLifetime();
		VOID ShutdownLifetime();
		VOID QueueLifetimeCheck();
		VOID CheckLifetime();
		BOOL HasLifetime();
		BOOL HasCompletedRoots();
		VOID CollectReleasedObjects();
		static LRESULT CALLBACK LifetimeWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooConstructionReport)
			UNK_INTERFACE(QuadooPrivate::IDiagnosticReport)
		END_UNK_MAP

		CApplicationRuntime();
		~CApplicationRuntime();
		HRESULT Initialize(HINSTANCE module, IJSONObject* catalog, IQuadooControlRegistry* registry = NULL,
			IDefinitionSource* definitions = NULL);
		HRESULT LoadDefinition(RSTRING className, IQuadooObject* script, IQuadooObject** loader);
		HRESULT Create(IJSONObject* unit, IJSONObject* definition, IQuadooObject* script,
			IQuadooObject* owner, IQuadooObject** root);
		HRESULT Unwrap(IQuadooObject* object, IQuadooComponent** component);
		HRESULT ShowForm(IQuadooObject* script, INT command);
		HRESULT SetLifetimeCallbacks(IQuadooObject* callbacks);
		HRESULT BeginScriptEntry();
		VOID EndScriptEntry();
		HRESULT BeginMessageLoop(BOOL* run);
		VOID EndMessageLoop();
		HRESULT EnterCallback();
		VOID LeaveCallback();
		HRESULT RegisterRoot(IQuadooComponent* component, BOOL committed = FALSE);
		HRESULT CommitRoot(IQuadooComponent* component);
		VOID UnregisterRoot(IQuadooComponent* component);
		VOID RootDestroyed(IQuadooComponent* component);
		HRESULT RegisterForm(IQuadooForm* form);
		HRESULT AcquireLifetimeLease(IUnknown** lease);
		VOID ReleaseLifetimeLease();
		HRESULT Preprocess(MSG* message, BOOL* handled);
		VOID Sweep();
		VOID Shutdown();
		HRESULT CheckThread();
		HRESULT CheckAsync();
		HRESULT QueueAsync(CInstanceLink* link, IQuadooComponent* component, IQuadooObject* callback);
		HRESULT QueueAsyncWait(CInstanceLink* link, IQuadooComponent* component, IQuadooObject* source, IQuadooObject* callback);
		HRESULT GetAsyncWaitNotification(HWND* window, UINT* message);
		VOID CancelAsync(CInstanceLink* link);
		HRESULT HandleContainerMessage(HWND window, UINT message, WPARAM wp, LPARAM lp, BOOL* handled, LRESULT* result);
		HRESULT Error() const;
		RSTRING Diagnostic() const;
		STDMETHOD(OnError)(HRESULT error, RSTRING path, RSTRING detail);
		STDMETHOD(ReportDiagnostic)(HRESULT error, RSTRING path, RSTRING detail);
	};
}
