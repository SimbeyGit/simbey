#pragma once

#include "ApplicationRuntime.h"
#include "Library\Core\Map.h"

namespace AQVM
{
	class CInstanceLink : public CBaseUnknown
	{
	public:
		CFormInstance* instance;
		BOOL asyncOpen;
		TArray<IQuadooObject*> treeObjects;
		TMap<IUnknown*,IQuadooObject*> listObjects;
		UINT listCursor;
		BEGIN_UNK_MAP
			UNK_INTERFACE_PATH(IUnknown,CBaseUnknown)
		END_UNK_MAP
		CInstanceLink();
		~CInstanceLink();
	};

	struct AssignedProperty
	{
		CRString name;
		QuadooVM::QVARIANT previous;
		TStackRef<IQuadooObject> assigned;
		AssignedProperty();
		~AssignedProperty();
	};

	class CComponentObject : public CBaseUnknown, public CQuadooObjectImpl, public IComponentObject
	{
	protected:
		TStackRef<CInstanceLink> m_link;
		TStackRef<IQuadooComponent> m_component;
		TStackRef<IQuadooObject> m_srNativeHandler;

	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooObject)
			UNK_INTERFACE(IComponentObject)
		END_UNK_MAP

		CComponentObject(CInstanceLink* link, IQuadooComponent* component);
		virtual ~CComponentObject();
		VOID AttachComponent(IQuadooComponent* component);
		STDMETHOD(GetComponent)(IQuadooComponent** component);
		STDMETHOD(Invoke)(IQuadooVM* vm, RSTRING method, QuadooVM::QVPARAMS* parameters, QuadooVM::QVARIANT* result);
		STDMETHOD(GetProperty)(IQuadooVM* vm, RSTRING name, QuadooVM::QVARIANT* result);
		STDMETHOD(GetIndexedProperty)(IQuadooVM* vm, RSTRING name, QuadooVM::QVARIANT* index, QuadooVM::QVARIANT* result);
		STDMETHOD(SetProperty)(IQuadooVM* vm, RSTRING name, QuadooVM::QVARIANT* value);
		STDMETHOD(SetIndexedProperty)(IQuadooVM* vm, RSTRING name, QuadooVM::QVARIANT* index, QuadooVM::QVARIANT* value);
		STDMETHOD(GetMemberVariable)(DWORD index, QuadooVM::QVARIANT** value);
		STDMETHOD(GetDefaultValue)(QuadooVM::QVARIANT* result);
		STDMETHOD(DeleteProperty)(RSTRING name, QuadooVM::QVARIANT* result);
		HRESULT Alive();
		HRESULT Wrap(IQuadooComponent* component, QuadooVM::QVARIANT* result);
		HRESULT ResolveMenuReference(IJSONValue* value, QuadooVM::QVARIANT* result);
	};

	class CContainerObject : public CComponentObject
	{
	public:
		CContainerObject(CInstanceLink* link, IQuadooComponent* component);
		STDMETHOD(Invoke)(IQuadooVM* vm, RSTRING method, QuadooVM::QVPARAMS* parameters, QuadooVM::QVARIANT* result);
		STDMETHOD(GetProperty)(IQuadooVM* vm, RSTRING name, QuadooVM::QVARIANT* result);
	};

	class CFormObject : public CContainerObject
	{
	public:
		CFormObject(CInstanceLink* link);
		STDMETHOD(Invoke)(IQuadooVM* vm, RSTRING method, QuadooVM::QVPARAMS* parameters, QuadooVM::QVARIANT* result);
	};

	class CFrameObject : public CContainerObject
	{
	public:
		CFrameObject(CInstanceLink* link);
	};

	class CFormInstance : public CBaseUnknown, public IQuadooEventsBinder, public IQuadooMessageHandler
	{
	private:
		CApplicationRuntime* m_runtime;
		TStackRef<IQuadooObject> m_script;
		TStackRef<IQuadooInterface> m_methods;
		TStackRef<IQuadooDelegate> m_userDestroy;
		TStackRef<CInstanceLink> m_link;
		TArray<CComponentObject*> m_objects;
		TArray<AssignedProperty*> m_assigned;
		CContainerObject* m_root;
		CRString m_destroyHandler;
		BOOL m_committed;
		BOOL m_detached;

	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooEventsBinder)
			UNK_INTERFACE(IQuadooMessageHandler)
		END_UNK_MAP

		CFormInstance(CApplicationRuntime* runtime, IQuadooObject* script);
		~CFormInstance();
		HRESULT Initialize(IJSONObject* unit, IJSONObject* definition);
		HRESULT Wrap(IQuadooComponent* component, IQuadooObject** object);
		HRESULT Populate(IQuadooControl* control, BOOL root, UINT depth = 0);
		HRESULT Assign(RSTRING name, IQuadooObject* object);
		HRESULT Resolve(RSTRING name, IQuadooDelegate** delegate);
		HRESULT InvokeResolved(ULONG ordinal, QuadooVM::QVPARAMS* parameters, QuadooVM::QVARIANT* result);
		HRESULT SetDestroyHandler(RSTRING name);
		RSTRING DestroyHandler();
		HRESULT InvokeDestroy(IQuadooEventArgs* args);
		HRESULT Commit();
		BOOL Committed();
		VOID Detach();
		BOOL Destroyed();
		IQuadooObject* Script();
		CContainerObject* Root();
		CApplicationRuntime* Runtime();
		STDMETHOD(BindEvent)(RSTRING name, IQuadooDelegate** delegate);
		STDMETHOD(PostBind)(IQuadooControl* control);
		STDMETHOD(HandleMessage)(HWND window, UINT message, WPARAM wp, LPARAM lp, BOOL* handled, LRESULT* result);
	};

	BOOL SameIdentity(IUnknown* first, IUnknown* second);
	HRESULT VariantInteger(const QuadooVM::QVARIANT* value, LONGLONG* number);
	HRESULT NewComponentObject(CInstanceLink* link, IQuadooComponent* component, CComponentObject** object);
}
