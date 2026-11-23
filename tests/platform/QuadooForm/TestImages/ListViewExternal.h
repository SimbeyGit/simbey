#pragma once
#include "..\..\..\..\platform\QuadooForm\Published\QuadooForm.h"

// A test consumer/source adapter built entirely from public capabilities. It
// borrows metadata/data from another public component, never an internal class.
class CListExternalSource : public CBaseUnknown, public IQuadooComponent,
	public IQuadooNonvisualComponent, public IQuadooConstructionParticipant,
	public IQuadooImageList, public IQuadooMenu, public IQuadooPopupMenu
{
	IQuadooComponentCollection* m_owner;
	TStackRef<IQuadooComponent> m_metadata;
	TStackRef<IQuadooImageList> m_images;
	TArray<IQuadooImageListObserver*> m_observers;
	GUID m_id;
	QuadooForm::ComponentState m_state;
public:
	HRESULT acquireResult;
	HRESULT subscribeResult;
	BOOL subscribeThenFail;
	UINT popups;
	POINT point;
	IQuadooListColumns* edit;
	IQuadooListItems* editItems;
	HRESULT editResult;
	IQuadooComponent* destroyControl;
	HRESULT destroyResult;
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooComponent)
		UNK_INTERFACE(IQuadooNonvisualComponent)
		UNK_INTERFACE(IQuadooConstructionParticipant)
		UNK_INTERFACE(IQuadooImageList)
		UNK_INTERFACE(IQuadooMenu)
		UNK_INTERFACE(IQuadooPopupMenu)
	END_UNK_MAP
	CListExternalSource(IQuadooComponentCollection* owner, IQuadooComponent* metadata, IQuadooImageList* images) :
		m_owner(owner), m_metadata(metadata), m_images(images), m_state(QuadooForm::Ready),
		acquireResult(S_OK), subscribeResult(S_OK), subscribeThenFail(FALSE), popups(0), edit(NULL), editItems(NULL),
		editResult(S_OK), destroyControl(NULL), destroyResult(S_OK)
	{
		m_id = GUID_NULL;
		point.x = 0;
		point.y = 0;
	}
	~CListExternalSource()
	{
		for(sysint n = 0; n < m_observers.Length(); n++)
		{
			m_observers[n]->Release();
		}
	}
	HRESULT Initialize()
	{
		return CoCreateGuid(&m_id);
	}
	UINT Observers() const
	{
		return static_cast<UINT>(m_observers.Length());
	}
	STDMETHOD(GetID)(GUID* id)
	{
		HRESULT hr = S_OK;
		CheckIf(!id,E_POINTER);
		*id = m_id;
	Cleanup:
		return hr;
	}
	STDMETHOD(GetName)(RSTRING* name)
	{
		return RStrCreateW(LSP(L"ExternalListSource"),name);
	}
	STDMETHOD(SetName)(RSTRING name)
	{
		return E_ACCESSDENIED;
	}
	STDMETHOD(GetTypeName)(RSTRING* type)
	{
		return RStrCreateW(LSP(L"ExternalListSource"),type);
	}
	STDMETHOD(GetProperties)(IQuadooProperties** properties)
	{
		return m_metadata->GetProperties(properties);
	}
	STDMETHOD(GetState)(QuadooForm::ComponentState* state)
	{
		HRESULT hr = S_OK;
		CheckIf(!state,E_POINTER);
		*state = m_state;
	Cleanup:
		return hr;
	}
	STDMETHOD(GetOwner)(IQuadooComponentCollection** owner)
	{
		HRESULT hr = S_OK;
		CheckIf(!owner,E_POINTER);
		*owner = NULL;
		CheckIf(!m_owner,QuadooForm::EDestroyed);
		*owner = m_owner;
		(*owner)->AddRef();
	Cleanup:
		return hr;
	}
	STDMETHOD(BindEvents)(IQuadooConstructionContext* context)
	{
		return S_OK;
	}
	STDMETHOD(InitializeControl)(VOID)
	{
		return S_OK;
	}
	STDMETHOD(ActivateControl)(VOID)
	{
		return S_OK;
	}
	STDMETHOD(AbortConstruction)(VOID)
	{
		return Destroy();
	}
	STDMETHOD(Destroy)(VOID)
	{
		HRESULT hr = S_OK;
		TStackRef<CListExternalSource> keepAlive(this);
		if(m_state != QuadooForm::Destroyed)
		{
			m_state = QuadooForm::Destroying;
			while(m_observers.Length())
			{
				TStackRef<IQuadooImageListObserver> observer;
				observer.Attach(m_observers[0]);
				m_observers.Remove(0,NULL);
				observer->OnImageListChanged(this);
			}
			if(m_owner)
			{
				hr = m_owner->UnregisterComponent(this);
				m_owner = NULL;
			}
			m_state = QuadooForm::Destroyed;
		}
		return hr;
	}
	STDMETHOD(Acquire)(UINT dpi, IQuadooImageListSnapshot** snapshot)
	{
		HRESULT hr;
		CheckIf(!snapshot,E_POINTER);
		*snapshot = NULL;
		CheckIf(m_state >= QuadooForm::Destroying,QuadooForm::EDestroyed);
		Check(acquireResult);
		if(destroyControl)
		{
			destroyResult = destroyControl->Destroy();
		}
		Check(m_images->Acquire(dpi,snapshot));
	Cleanup:
		return hr;
	}
	STDMETHOD(AddImageListObserver)(IQuadooImageListObserver* observer)
	{
		HRESULT hr;
		CheckIf(!observer,E_POINTER);
		if(!subscribeThenFail)
		{
			Check(subscribeResult);
		}
		Check(m_observers.Append(observer));
		observer->AddRef();
		hr = subscribeResult;
	Cleanup:
		return hr;
	}
	STDMETHOD(RemoveImageListObserver)(IQuadooImageListObserver* observer)
	{
		HRESULT hr = S_FALSE;
		for(sysint n = 0; n < m_observers.Length(); n++)
		{
			if(m_observers[n] == observer)
			{
				m_observers.Remove(n,NULL);
				observer->Release();
				hr = S_OK;
				break;
			}
		}
		return hr;
	}
	STDMETHOD(IsPopup)(BOOL* popup)
	{
		HRESULT hr = S_OK;
		CheckIf(!popup,E_POINTER);
		*popup = TRUE;
	Cleanup:
		return hr;
	}
	STDMETHOD(Popup)(HWND owner, INT x, INT y)
	{
		popups++;
		point.x = x;
		point.y = y;
		if(edit)
		{
			editResult = edit->Clear();
		}
		if(editItems)
		{
			editResult = editItems->Clear();
		}
		return S_FALSE;
	}
};
