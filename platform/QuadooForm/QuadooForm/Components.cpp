#include "Runtime.h"

namespace QF
{
HRESULT ValidateVisualComponentIdentity(IQuadooControlContainer* root, RSTRING name, REFGUID id, BOOL frameAlias)
{
	HRESULT hr;
	TStackRef<IQuadooComponentHost> host; TStackRef<IQuadooComponentCollection> components; UINT count;
	Check(QueryOptional(root,&host));
	if(host)
	{
		Check(host->GetComponents(&components)); Check(components->GetCount(&count));
		for(UINT n = 0; n < count; n++)
		{
			TStackRef<IQuadooComponent> component; CRString current; GUID currentID;
			Check(components->GetAt(n,&component)); Check(component->GetName(&current)); Check(component->GetID(&currentID));
			CheckIf(SameString(name,current),QuadooForm::EDuplicateName);
			CheckIf(!frameAlias && IsEqualGUID(id,currentID),QuadooForm::EDuplicateID);
		}
	}
	hr = S_OK;
Cleanup:
	return hr;
}
class CComponentCollection : public CBaseUnknown, public IQuadooComponentCollection
{
	DWORD m_thread;
	IQuadooControlContainer* m_root; // Weak; detached before the root releases us.
	TArray<IQuadooComponent*> m_items;
	BOOL m_destroying, m_registering;
	HRESULT CheckAlive() { return m_thread != GetCurrentThreadId() ? QuadooForm::EWrongThread : !m_root ? QuadooForm::EDestroyed : S_OK; }
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooComponentCollection)
	END_UNK_MAP
	CComponentCollection(IQuadooControlContainer* root) : m_thread(GetCurrentThreadId()), m_root(root), m_destroying(FALSE), m_registering(FALSE) {}
	~CComponentCollection() { Shutdown(); }
	HRESULT CanDestroy() { return m_thread != GetCurrentThreadId() ? QuadooForm::EWrongThread : m_registering ? QuadooForm::EInvalidState : S_OK; }
	HRESULT Shutdown(BOOL abort = FALSE)
	{
		HRESULT hr = S_OK;
		CheckIf(m_thread != GetCurrentThreadId(),QuadooForm::EWrongThread);
		CheckIf(m_registering,QuadooForm::EInvalidState);
		if(!m_destroying)
		{
			m_destroying = TRUE;
			while(m_items.Length())
			{
				TStackRef<IQuadooComponent> component(m_items[m_items.Length() - 1]);
				HRESULT destroy;
				TStackRef<IQuadooConstructionParticipant> participant;
				if(abort && SUCCEEDED(component->QueryInterface(__uuidof(IQuadooConstructionParticipant),reinterpret_cast<PVOID*>(&participant)))) destroy = participant->AbortConstruction();
				else destroy = component->Destroy();
				if(FAILED(destroy) && SUCCEEDED(hr)) hr = destroy;
				// A foreign implementation may not unregister itself on failure.
				for(sysint n = m_items.Length(); n > 0; n--) if(SameObject(m_items[n - 1],component))
				{ m_items[n - 1]->Release(); m_items.Remove(n - 1,NULL); }
			}
			m_root = NULL;
		}
Cleanup:
		return hr;
	}
	STDMETHOD(GetRoot)(IQuadooControlContainer** root)
	{
		HRESULT hr;
		CheckIf(!root,E_POINTER); *root = NULL;
		Check(CheckAlive()); *root = m_root; (*root)->AddRef();
	Cleanup:
		return hr;
	}
	STDMETHOD(GetCount)(UINT* count)
	{
		HRESULT hr;
		CheckIf(!count,E_POINTER); *count = 0;
		Check(CheckAlive()); *count = static_cast<UINT>(m_items.Length());
	Cleanup:
		return hr;
	}
	STDMETHOD(GetAt)(UINT index, IQuadooComponent** component)
	{
		HRESULT hr;
		CheckIf(!component,E_POINTER); *component = NULL;
		Check(CheckAlive());
		CheckIf(index >= static_cast<UINT>(m_items.Length()),HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
		*component = m_items[index]; (*component)->AddRef();
	Cleanup:
		return hr;
	}
	STDMETHOD(FindComponent)(RSTRING name, IQuadooComponent** component)
	{
		HRESULT hr;
		TStackRef<IQuadooControl> control;
		CheckIf(!component,E_POINTER); *component = NULL;
		Check(CheckAlive());
		CheckIf(!name || !RStrIsWide(name),E_INVALIDARG);
		for(sysint n = 0; n < m_items.Length(); n++)
		{
			CRString current; Check(m_items[n]->GetName(&current));
			if(SameString(current,name)) { *component = m_items[n]; (*component)->AddRef(); hr = S_OK; goto Cleanup; }
		}
		Check(m_root->FindControl(name,&control));
		Check(control->QueryInterface(__uuidof(IQuadooComponent),reinterpret_cast<PVOID*>(component)));
	Cleanup:
		return hr;
	}
	STDMETHOD(ValidateIdentity)(IQuadooComponent* component, RSTRING name, REFGUID id)
	{
		HRESULT hr;
		TRStrMap<IQuadooControl*> names(RStrCompareRStr); TArray<GUID> ids; IQuadooControl* match;
		Check(CheckAlive());
		CheckIf(!name || !RStrIsWide(name) || !RStrLen(name) || RStrLen(name) > 256 || IsEqualGUID(id,GUID_NULL),QuadooForm::EInvalidDefinition);
		for(INT n = 0; n < RStrLen(name); n++) { CheckIf(!RStrToWide(name)[n],QuadooForm::EInvalidDefinition); }
		Check(IndexTree(m_root,names,ids));
		if(SUCCEEDED(names.Find(name,&match))) { CheckIf(!component || !SameObject(component,match),QuadooForm::EDuplicateName); }
		for(sysint n = 0; n < ids.Length(); n++) if(IsEqualGUID(id,ids[n]))
		{
			// A visual rename keeps the identity already indexed in that tree.
			GUID current = GUID_NULL; TStackRef<IQuadooControl> visual;
			if(component && SUCCEEDED(component->QueryInterface(__uuidof(IQuadooControl),reinterpret_cast<PVOID*>(&visual)))) visual->GetID(&current);
			CheckIf(!IsEqualGUID(current,id),QuadooForm::EDuplicateID);
		}
		for(sysint n = 0; n < m_items.Length(); n++) if(!component || !SameObject(component,m_items[n]))
		{
			CRString current; GUID currentID;
			Check(m_items[n]->GetName(&current));
			Check(m_items[n]->GetID(&currentID));
			CheckIf(SameString(name,current),QuadooForm::EDuplicateName);
			CheckIf(IsEqualGUID(id,currentID),QuadooForm::EDuplicateID);
		}
		hr = S_OK;
	Cleanup:
		return hr;
	}
	STDMETHOD(RegisterComponent)(IQuadooComponent* component)
	{
		HRESULT hr;
		TStackRef<IQuadooControlContainer> keepAlive(m_root);
		TStackRef<IQuadooNonvisualComponent> nonvisual; TStackRef<IQuadooComponentCollection> owner;
		TStackRef<IQuadooConstructionParticipant> participant; CRString name; GUID id; QuadooForm::ComponentState state;
		BOOL entered = FALSE;
		Check(CheckAlive());
		CheckIf(!component,E_INVALIDARG);
		CheckIf(m_destroying || m_registering,QuadooForm::EInvalidState);
		m_registering = TRUE; entered = TRUE;
		Check(component->QueryInterface(__uuidof(IQuadooNonvisualComponent),reinterpret_cast<PVOID*>(&nonvisual)));
		CheckIf(!nonvisual,E_UNEXPECTED);
		Check(nonvisual->GetOwner(&owner));
		CheckIf(!SameObject(owner,static_cast<IQuadooComponentCollection*>(this)),E_INVALIDARG);
		Check(component->GetState(&state));
		CheckIf(state != QuadooForm::Constructing && state != QuadooForm::Ready,QuadooForm::EInvalidState);
		Check(component->QueryInterface(__uuidof(IQuadooConstructionParticipant),reinterpret_cast<PVOID*>(&participant)));
		CheckIf(!participant,E_UNEXPECTED);
		Check(ValidateComponentMetadata(component));
		Check(ValidateUniqueComponent(m_root,component));
		Check(component->GetName(&name));
		Check(component->GetID(&id));
		Check(ValidateIdentity(component,name,id));
		for(sysint n = 0; n < m_items.Length(); n++) { CheckIf(SameObject(component,m_items[n]),HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)); }
		Check(m_items.Append(component)); component->AddRef();
	Cleanup:
		if(entered) m_registering = FALSE;
		return hr;
	}
	STDMETHOD(UnregisterComponent)(IQuadooComponent* component)
	{
		HRESULT hr;
		Check(CheckAlive());
		CheckIf(!component,E_INVALIDARG);
		CheckIf(m_registering,QuadooForm::EInvalidState);
		for(sysint n = 0; n < m_items.Length(); n++) if(SameObject(component,m_items[n]))
		{ IQuadooComponent* previous = m_items[n]; m_items.Remove(n,NULL); previous->Release(); hr = S_OK; goto Cleanup; }
		hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
	Cleanup:
		return hr;
	}
};
HRESULT CreateComponentCollection(IQuadooControlContainer* root, IQuadooComponentCollection** collection)
{
	HRESULT hr = S_OK;
	CheckIf(!root || !collection,E_INVALIDARG); *collection = __new CComponentCollection(root);
	CheckAlloc(*collection);
Cleanup:
	return hr;
}
HRESULT DestroyComponentCollection(IQuadooComponentCollection* collection, BOOL abort)
{
	return static_cast<CComponentCollection*>(collection)->Shutdown(abort);
}
HRESULT CanDestroyComponentCollection(IQuadooComponentCollection* collection) { return static_cast<CComponentCollection*>(collection)->CanDestroy(); }

HRESULT DispatchComponentMessage(IQuadooControlContainer* root, HWND window, UINT message, WPARAM wp, LPARAM lp, BOOL* handled, LRESULT* result)
{
	HRESULT hr;
	TStackRef<IQuadooControlContainer> keepAlive(root); TStackRef<IQuadooComponentHost> host; TStackRef<IQuadooComponentCollection> collection;
	TArray<IQuadooComponent*> items; UINT count;
	Check(QueryOptional(root,&host));
	if(host)
	{
		Check(host->GetComponents(&collection)); Check(collection->GetCount(&count));
		CheckIf(count > 4096,QuadooForm::EInvalidDefinition);
		for(UINT n = 0; n < count; n++) { TStackRef<IQuadooComponent> component; Check(collection->GetAt(n,&component)); Check(items.Append(component)); component.Detach(); }
		for(sysint n = 0; n < items.Length(); n++)
		{
			TStackRef<IQuadooMessageHandler> handler; QuadooForm::ComponentState state;
			Check(root->GetState(&state)); if(state != QuadooForm::Ready) break;
			Check(QueryOptional(items[n],&handler));
			if(handler) { Check(handler->HandleMessage(window,message,wp,lp,handled,result)); if(*handled) break; }
		}
	}
	hr = S_OK;
Cleanup:
	for(sysint n = 0; n < items.Length(); n++) items[n]->Release();
	return hr;
}

HRESULT CompleteComponent(IQuadooConstructionContext* context, IQuadooComponent* component)
{
	HRESULT hr; TStackRef<IQuadooConstructionParticipant> participant;
	Check(component->QueryInterface(__uuidof(IQuadooConstructionParticipant),reinterpret_cast<PVOID*>(&participant)));
	Check(participant->BindEvents(context));
	Check(participant->InitializeControl());
	Check(participant->ActivateControl());
Cleanup:
	return hr;
}
}

HRESULT WINAPI QuadooCreateComponent(IQuadooConstructionContext* context, IQuadooComponentCollection* owner, IJSONObject* definition, IQuadooComponent** result)
{
	HRESULT hr; TStackRef<IQuadooComponent> component;
	TArray<IQuadooComponent*> previous; UINT count; BOOL captured = FALSE;
	CheckIf(!result,E_POINTER); *result = NULL;
	CheckIf(!context || !owner || !definition,E_INVALIDARG);
	Check(owner->GetCount(&count));
	for(UINT n = 0; n < count; n++) { TStackRef<IQuadooComponent> item; Check(owner->GetAt(n,&item)); Check(previous.Append(item)); item.Detach(); }
	captured = TRUE;
	Check(QF::BuildComponent(context,owner,definition,&component));
	{
		TStackRef<IQuadooControlContainer> root;
		Check(owner->GetRoot(&root));
		Check(QF::ValidateMenuGraph(root));
	}
	Check(QF::CompleteComponent(context,component));
	*result = component.Detach();
Cleanup:
	if(FAILED(hr) && component) { TStackRef<IQuadooConstructionParticipant> participant; if(SUCCEEDED(component->QueryInterface(__uuidof(IQuadooConstructionParticipant),reinterpret_cast<PVOID*>(&participant)))) participant->AbortConstruction(); else component->Destroy(); }
	if(FAILED(hr) && captured && SUCCEEDED(owner->GetCount(&count)))
		for(UINT n = count; n > 0; n--)
		{
			TStackRef<IQuadooComponent> item;
			if(SUCCEEDED(owner->GetAt(n - 1,&item)))
			{
				BOOL existed = FALSE; for(sysint p = 0; p < previous.Length(); p++) if(QF::SameObject(previous[p],item)) existed = TRUE;
				if(!existed) { TStackRef<IQuadooConstructionParticipant> abort; if(SUCCEEDED(item->QueryInterface(__uuidof(IQuadooConstructionParticipant),reinterpret_cast<PVOID*>(&abort)))) abort->AbortConstruction(); else item->Destroy(); }
			}
		}
	for(sysint n = 0; n < previous.Length(); n++) previous[n]->Release();
	return hr;
}
