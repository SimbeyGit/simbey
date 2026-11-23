#include "StatusBar.h"
#include "MenuRuntime.h"

namespace QF
{
	CStatusImageObserver::CStatusImageObserver() : bar(NULL)
	{
	}
	STDMETHODIMP_(VOID) CStatusImageObserver::OnImageListChanged(IQuadooImageList*)
	{
		if(bar)
		{
			TStackRef<CStatusBar> keepAlive(bar);
			QuadooForm::ComponentState state;
			if(SUCCEEDED(keepAlive->GetState(&state)) && state < QuadooForm::Destroying)
			{
				keepAlive->RefreshImages();
			}
		}
	}
	VOID CStatusBar::DetachImages()
	{
		if(m_imageObserver)
		{
			m_imageObserver->bar = NULL;
		}
		if(m_imageList && m_imageObserver)
		{
			m_imageList->RemoveImageListObserver(m_imageObserver);
		}
		m_imageObserver.Release();
		m_imageList.Release();
		m_images.Release();
	}
	HRESULT CStatusBar::RebuildPanels()
	{
		HRESULT hr = S_OK;
		TStackRef<CStatusBar> keepAlive(this);
		TStackRef<IQuadooImageListSnapshot> snapshot;
		TArray<HICON> icons;
		INT edges[256];
		LONGLONG edge = 0;
		HIMAGELIST handle = NULL;
		BOOL entered = FALSE;
		RECT client;
		CheckIfIgnore(!m_hwnd || !m_panels,S_OK);
		CheckIf(m_imageBusy,QuadooForm::EInvalidState);
		m_imageBusy = TRUE;
		entered = TRUE;
		if(m_imageList)
		{
			Check(m_imageList->Acquire(m_dpi,&snapshot));
			CheckIf(!snapshot,E_UNEXPECTED);
			Check(snapshot->GetHandle(&handle));
			CheckIf(!handle,E_UNEXPECTED);
			Check(CheckAlive());
		}
		CheckIf(!GetClientRect(m_hwnd,&client),HrEnsureLastError());
		for(sysint n = 0; n < m_items.Length(); n++)
		{
			HICON icon = NULL;
			edge += MulDiv(m_items[n]->width,m_dpi,96);
			edges[n] = static_cast<INT>(min(edge,static_cast<LONGLONG>(INT_MAX)));
			if(handle && m_items[n]->image >= 0 && m_items[n]->image < ImageList_GetImageCount(handle))
			{
				icon = ImageList_GetIcon(handle,m_items[n]->image,ILD_NORMAL);
				CheckIf(!icon,E_OUTOFMEMORY);
			}
			hr = icons.Append(icon);
			if(FAILED(hr) && icon)
			{
				DestroyIcon(icon);
			}
			Check(hr);
		}
		// Native status bars retain HICONs without copying or destroying them.
		// Prepare every icon before changing any part, then release the old set.
		if(m_items.Length())
		{
			edges[m_items.Length() - 1] = -1;
			CheckIf(!SendMessageW(m_hwnd,SB_SETPARTS,m_items.Length(),reinterpret_cast<LPARAM>(edges)),E_OUTOFMEMORY);
			SendMessageW(m_hwnd,SB_SIMPLE,FALSE,0);
			for(sysint n = 0; n < m_items.Length(); n++)
			{
				SendMessageW(m_hwnd,SB_SETICON,n,reinterpret_cast<LPARAM>(icons[n]));
				SendMessageW(m_hwnd,SB_SETTEXTW,n,reinterpret_cast<LPARAM>(m_items[n]->text.c_wstr()));
			}
		}
		else
		{
			edges[0] = -1;
			CheckIf(!SendMessageW(m_hwnd,SB_SETPARTS,1,reinterpret_cast<LPARAM>(edges)),E_OUTOFMEMORY);
			SendMessageW(m_hwnd,SB_SETICON,0,0);
			SendMessageW(m_hwnd,SB_SETTEXTW,0,reinterpret_cast<LPARAM>(L""));
		}
		m_icons.Swap(icons);
		InvalidateRect(m_hwnd,NULL,TRUE);
	Cleanup:
		for(sysint n = 0; n < icons.Length(); n++)
		{
			if(icons[n])
			{
				DestroyIcon(icons[n]);
			}
		}
		if(entered)
		{
			m_imageBusy = FALSE;
		}
		return hr;
	}
	HRESULT CStatusBar::ResolveImages()
	{
		HRESULT hr;
		TStackRef<CStatusBar> keepAlive(this);
		TStackRef<IJSONValue> value;
		TStackRef<IQuadooControlContainer> root;
		TStackRef<IQuadooComponentHost> host;
		TStackRef<IQuadooComponentCollection> components;
		TStackRef<IQuadooComponent> component;
		TStackRef<IQuadooImageList> source;
		TStackRef<CStatusImageObserver> observer;
		TStackRef<IQuadooComponent> previousComponent(m_images);
		TStackRef<IQuadooImageList> previous(m_imageList);
		TStackRef<CStatusImageObserver> previousObserver(m_imageObserver);
		BOOL subscribed = FALSE;
		BOOL entered = FALSE;
		CheckIf(m_imageBusy,QuadooForm::EInvalidState);
		m_imageBusy = TRUE;
		entered = TRUE;
		Check(StoredValue(L"Images",&value));
		if(value)
		{
			GUID id;
			Check(value->GetStringAsGuid(id));
			Check(GetNamespace(Control(),&root));
			Check(root->QueryInterface(&host));
			Check(host->GetComponents(&components));
			Check(FindComponentID(components,id,&component));
			Check(component->QueryInterface(&source));
			observer.Attach(__new CStatusImageObserver);
			CheckAlloc(observer);
			subscribed = TRUE;
			Check(source->AddImageListObserver(observer));
			Check(CheckAlive());
		}
		m_images = component;
		m_imageList = source;
		m_imageBusy = FALSE;
		hr = RebuildPanels();
		m_imageBusy = TRUE;
		if(FAILED(hr))
		{
			if(m_state < QuadooForm::Destroying)
			{
				m_images = previousComponent;
				m_imageList = previous;
			}
			goto Cleanup;
		}
		if(previousObserver)
		{
			previousObserver->bar = NULL;
		}
		if(previous && previousObserver)
		{
			previous->RemoveImageListObserver(previousObserver);
		}
		Check(CheckAlive());
		m_imageObserver = observer;
		if(observer)
		{
			observer->bar = this;
		}
	Cleanup:
		if(FAILED(hr) && subscribed && source && observer)
		{
			source->RemoveImageListObserver(observer);
		}
		if(entered)
		{
			m_imageBusy = FALSE;
		}
		return hr;
	}
	HRESULT CStatusBar::RefreshImages()
	{
		HRESULT hr;
		TStackRef<CStatusBar> keepAlive(this);
		hr = RebuildPanels();
		if(hr == QuadooForm::EDestroyed && m_state < QuadooForm::Destroying)
		{
			DetachImages();
			Check(SetNamedValue(L"Images",QuadooProperty::ObjectReference,NULL));
		}
		ReportCallback(hr,L"Images");
	Cleanup:
		return hr;
	}
	STDMETHODIMP CStatusBar::GetImages(IQuadooComponent** images)
	{
		HRESULT hr;
		CheckIf(!images,E_POINTER);
		*images = NULL;
		Check(CheckAlive());
		*images = m_images;
		if(*images)
		{
			(*images)->AddRef();
		}
	Cleanup:
		return hr;
	}
	STDMETHODIMP CStatusBar::SetImages(IQuadooComponent* images)
	{
		HRESULT hr;
		TStackRef<IJSONValue> value;
		Check(CheckAlive());
		if(images)
		{
			GUID id;
			CRString text;
			TStackRef<IQuadooControlContainer> root;
			TStackRef<IQuadooComponentHost> host;
			TStackRef<IQuadooComponentCollection> components;
			TStackRef<IQuadooComponent> candidate;
			Check(images->GetID(&id));
			Check(GetNamespace(Control(),&root));
			Check(root->QueryInterface(&host));
			Check(host->GetComponents(&components));
			Check(FindComponentID(components,id,&candidate));
			CheckIf(!SameObject(candidate,images),E_INVALIDARG);
			Check(GuidString(id,&text));
			Check(JSONCreateString(text,&value));
		}
		Check(SetNamedValue(L"Images",QuadooProperty::ObjectReference,value));
	Cleanup:
		return hr;
	}
}
