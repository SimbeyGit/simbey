#include "ListView.h"
#include "MenuRuntime.h"

namespace QF
{
	static PCWSTR const imageNames[] = {L"LargeImages",L"SmallImages",L"StateImages"};
	static const INT nativeRoles[] = {LVSIL_NORMAL,LVSIL_SMALL,LVSIL_STATE};
	CListImageObserver::CListImageObserver() : view(NULL)
	{
	}
	STDMETHODIMP_(VOID) CListImageObserver::OnImageListChanged(IQuadooImageList*)
	{
		if(view)
		{
			TStackRef<CListView> keepAlive(view);
			view->ImageSourceChanged();
		}
	}
	VOID CListView::ImageSourceChanged()
	{
		ReportCallback(RefreshImages(),L"Images");
	}
	VOID CListView::DetachImages(UINT role)
	{
		ImageBinding& binding = m_images[role];
		if(binding.observer)
		{
			binding.observer->view = NULL;
		}
		if(m_hwnd && binding.snapshot)
		{
			ListView_SetImageList(m_hwnd,NULL,nativeRoles[role]);
		}
		if(binding.source && binding.observer)
		{
			// Disable the weak callback before releasing our subscription. Even a
			// misbehaving external source retaining it cannot retain this control.
			binding.source->RemoveImageListObserver(binding.observer);
		}
		binding.snapshot.Release();
		binding.observer.Release();
		binding.source.Release();
		binding.component.Release();
	}
	HRESULT CListView::ApplyImageHandles()
	{
		IQuadooImageListSnapshot* images[3];
		for(UINT n = 0; n < 3; n++)
		{
			images[n] = m_virtualImages[n];
		}
		return CommitVirtualImages(images);
	}
	HRESULT CListView::AttachImages(UINT role, IQuadooComponent* component)
	{
		HRESULT hr = S_OK;
		TStackRef<CListView> keepAlive(this);
		TStackRef<IQuadooImageList> source;
		TStackRef<IQuadooImageListSnapshot> snapshot;
		TStackRef<CListImageObserver> observer;
		HIMAGELIST handle = NULL;
		HIMAGELIST handles[3];
		BOOL subscribed = FALSE;
		BOOL entered = FALSE;
		IQuadooImageListSnapshot* bases[3];
		IQuadooImageListSnapshot* derived[3] = {NULL,NULL,NULL};
		Check(CheckAlive());
		CheckIf(m_imageChanging || m_imageRefreshing,QuadooForm::EInvalidState);
		CheckIf(role == QuadooForm::ListStateImages && component && Boolean(L"CheckBoxes"),E_INVALIDARG);
		CheckIfIgnore(SameObject(m_images[role].component,component),S_FALSE);
		m_imageChanging = TRUE;
		entered = TRUE;
		if(component)
		{
			QuadooForm::ComponentState state;
			Check(component->GetState(&state));
			CheckIf(state >= QuadooForm::Destroying,QuadooForm::EDestroyed);
			Check(component->QueryInterface(__uuidof(IQuadooImageList),reinterpret_cast<PVOID*>(&source)));
			CheckIf(!source,E_UNEXPECTED);
			Check(source->Acquire(m_dpi,&snapshot));
			CheckIf(!snapshot,E_UNEXPECTED);
			Check(snapshot->GetHandle(&handle));
			CheckIf(!handle,E_UNEXPECTED);
			observer.Attach(__new CListImageObserver);
			CheckAlloc(observer);
			// Attempt cleanup even when a third-party Add registers then fails.
			subscribed = TRUE;
			Check(source->AddImageListObserver(observer));
			Check(component->GetState(&state));
			CheckIf(state >= QuadooForm::Destroying,QuadooForm::EDestroyed);
		}
		for(UINT n = 0; n < 3; n++)
		{
			bases[n] = n == role ? static_cast<IQuadooImageListSnapshot*>(snapshot) : static_cast<IQuadooImageListSnapshot*>(m_images[n].snapshot);
		}
		Check(PrepareVirtualImages(bases,derived));
		Check(ImageHandles(bases,derived,handles));
		Check(CheckAlive());
		DetachImages(role);
		Check(CheckAlive());
		m_images[role].component = component;
		m_images[role].source = source;
		m_images[role].snapshot = snapshot;
		m_images[role].observer = observer;
		if(observer)
		{
			observer->view = this;
		}
		if(m_hwnd)
		{
			Check(CommitVirtualImages(derived,handles));
		}
	Cleanup:
		for(UINT n = 0; n < 3; n++)
		{
			SafeRelease(derived[n]);
		}
		if(FAILED(hr) && subscribed && source && observer)
		{
			source->RemoveImageListObserver(observer);
		}
		if(entered)
		{
			m_imageChanging = FALSE;
		}
		return hr;
	}
	HRESULT CListView::ResolveImages(UINT role)
	{
		HRESULT hr;
		TStackRef<IJSONValue> value;
		TStackRef<IQuadooControlContainer> root;
		TStackRef<IQuadooComponentHost> host;
		TStackRef<IQuadooComponentCollection> components;
		TStackRef<IQuadooComponent> component;
		GUID id;
		Check(StoredValue(imageNames[role],&value));
		if(value)
		{
			Check(value->GetStringAsGuid(id));
			Check(GetNamespace(Control(),&root));
			Check(root->QueryInterface(__uuidof(IQuadooComponentHost),reinterpret_cast<PVOID*>(&host)));
			Check(host->GetComponents(&components));
			Check(FindComponentID(components,id,&component));
		}
		Check(AttachImages(role,component));
	Cleanup:
		return hr;
	}
	HRESULT CListView::RefreshImages()
	{
		HRESULT hr = S_OK;
		TStackRef<CListView> keepAlive(this);
		TStackRef<IQuadooImageListSnapshot> candidates[3];
		TStackRef<IQuadooImageList> sources[3];
		HIMAGELIST handles[3] = {NULL,NULL,NULL};
		BOOL destroyed[3] = {FALSE,FALSE,FALSE};
		BOOL entered = FALSE;
		IQuadooImageListSnapshot* bases[3];
		IQuadooImageListSnapshot* derived[3] = {NULL,NULL,NULL};
		Check(CheckAlive());
		CheckIf(m_imageChanging || m_imageRefreshing,QuadooForm::EInvalidState);
		m_imageRefreshing = TRUE;
		entered = TRUE;
		for(UINT n = 0; n < ARRAYSIZE(sources); n++)
		{
			sources[n] = m_images[n].source;
		}
		for(UINT n = 0; n < ARRAYSIZE(candidates); n++)
		{
			if(sources[n])
			{
				hr = sources[n]->Acquire(m_dpi,&candidates[n]);
				if(hr == QuadooForm::EDestroyed)
				{
					destroyed[n] = TRUE;
					candidates[n].Release();
				}
				else
				{
					Check(hr);
					CheckIf(!candidates[n],E_UNEXPECTED);
					Check(candidates[n]->GetHandle(&handles[n]));
					CheckIf(!handles[n],E_UNEXPECTED);
				}
			}
		}
		for(UINT n = 0; n < 3; n++)
		{
			bases[n] = candidates[n];
		}
		Check(PrepareVirtualImages(bases,derived));
		Check(ImageHandles(bases,derived,handles));
		Check(CheckAlive());
		for(UINT n = 0; n < ARRAYSIZE(candidates); n++)
		{
			if(destroyed[n])
			{
				DetachImages(n);
				UINT index = FindProperty(imageNames[n]);
				CheckIf(index >= static_cast<UINT>(m_values.Length()),DISP_E_MEMBERNOTFOUND);
				SafeRelease(m_values[index].value);
				Check(Dirty(imageNames[n]));
			}
			else
			{
				m_images[n].snapshot = candidates[n];
			}
		}
		Check(CommitVirtualImages(derived,handles));
		Check(FlushNotifications());
	Cleanup:
		for(UINT n = 0; n < 3; n++)
		{
			SafeRelease(derived[n]);
		}
		if(entered)
		{
			m_imageRefreshing = FALSE;
		}
		return hr;
	}
	STDMETHODIMP CListView::GetImages(QuadooForm::ListImageRole role, IQuadooComponent** images)
	{
		HRESULT hr;
		CheckIf(!images,E_POINTER);
		*images = NULL;
		Check(CheckAlive());
		CheckIf(role < QuadooForm::ListLargeImages || role > QuadooForm::ListStateImages,E_INVALIDARG);
		*images = m_images[role].component;
		if(*images)
		{
			(*images)->AddRef();
		}
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListView::SetImages(QuadooForm::ListImageRole role, IQuadooComponent* images)
	{
		HRESULT hr;
		GUID id;
		CRString text;
		TStackRef<IJSONValue> value;
		Check(Mutable());
		CheckIf(role < QuadooForm::ListLargeImages || role > QuadooForm::ListStateImages,E_INVALIDARG);
		if(images)
		{
			TStackRef<IQuadooControlContainer> root;
			TStackRef<IQuadooComponentHost> host;
			TStackRef<IQuadooComponentCollection> components;
			TStackRef<IQuadooComponent> candidate;
			Check(images->GetID(&id));
			Check(GetNamespace(Control(),&root));
			Check(root->QueryInterface(__uuidof(IQuadooComponentHost),reinterpret_cast<PVOID*>(&host)));
			Check(host->GetComponents(&components));
			Check(FindComponentID(components,id,&candidate));
			CheckIf(!SameObject(candidate,images),E_INVALIDARG);
			Check(GuidString(id,&text));
			Check(JSONCreateString(text,&value));
		}
		Check(SetNamedValue(imageNames[role],QuadooProperty::ObjectReference,value));
	Cleanup:
		return hr;
	}
}
