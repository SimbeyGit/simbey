#include "Runtime.h"
#include "ComponentRuntime.h"
#include "NativeImageList.h"
#include "..\Imaging\ImageListData.h"

namespace QF
{
	static const EnumSpec depths[] = { {L"Color4",4},{L"Color8",8},{L"Color16",16},{L"Color24",24},{L"Color32",32} };
	static const DWORD Stored = QuadooForm::PropertySerializable;
	static const PropertySpec properties[] =
	{
		{ L"Width",QuadooProperty::Int,Stored,16,1 },
		{ L"Height",QuadooProperty::Int,Stored,16,2 },
		{ L"ColorDepth",QuadooProperty::Enum,Stored,24,3,depths,ARRAYSIZE(depths) },
		{ L"Masked",QuadooProperty::Boolean,Stored,1,4 },
		{ L"ImageCount",QuadooProperty::Int,QuadooForm::PropertyReadOnly,0,5 },
		{ L"Images",QuadooProperty::Collection,Stored,0,6 }
	};
	static const TypeSpec imageListType = { L"ImageList",&NonvisualType,properties,ARRAYSIZE(properties),NULL,0,IDI_PALETTE_IMAGELIST };
	class CImageList : public CNonvisual, public IQuadooImageList
	{
		struct Cache
		{
			UINT dpi;
			IQuadooImageListSnapshot* snapshot;
		};
		TArray<Cache> m_cache;
		TStackRef<IJSONObject> m_nativeDefinition;
		TArray<IQuadooImageListObserver*> m_imageObservers;
		BOOL m_imagesNotifying;
		HRESULT NativeDefinition(IJSONObject** definition)
		{
			HRESULT hr;
			TStackRef<IJSONObject> result;
			static PCWSTR const names[] = {L"Width",L"Height",L"ColorDepth",L"Masked",L"Images"};
			Check(JSONCreateObject(&result));
			for(UINT n = 0; n < ARRAYSIZE(names); n++)
			{
				TStackRef<IJSONValue> value;
				Check(GetValueJSON(Find(names[n]),&value));
				Check(result->AddValueW(names[n],value));
			}
			*definition = result.Detach();
		Cleanup:
			return hr;
		}
		HRESULT Build(UINT dpi, IQuadooImageListSnapshot** snapshot)
		{
			HRESULT hr;
			TStackRef<QFI::CImageListData> data;
			UINT index = Find(L"Images");
			CheckIf(index >= static_cast<UINT>(m_values.Length()),E_UNEXPECTED);
			Check(QFI::CImageListData::Create(m_values[index].value,Integer(L"Width"),Integer(L"Height"),
				Integer(L"ColorDepth"),Boolean(L"Masked"),&data));
			Check(BuildNativeImageList(data,dpi,m_factory,m_live,snapshot));
		Cleanup:
			return hr;
		}
		VOID ClearCache()
		{
			for(sysint n = 0; n < m_cache.Length(); n++)
			{
				m_cache[n].snapshot->Release();
			}
			m_cache.Clear();
		}

	protected:
		virtual HRESULT DestroyObject()
		{
			ClearCache();
			// Destruction must notify without allocating. Detach entries before
			// calling them so unsubscribe/destruction from callbacks is safe.
			while(m_imageObservers.Length())
			{
				IQuadooImageListObserver* observer = m_imageObservers[0];
				m_imageObservers.Remove(0,NULL);
				observer->OnImageListChanged(this);
				observer->Release();
			}
			return S_OK;
		}
		virtual HRESULT ValidateProperty(PCWSTR name, IJSONValue* value)
		{
			HRESULT hr = S_OK;
			CheckIf(m_imagesNotifying,QuadooForm::EInvalidState);
			if(!lstrcmpW(name,L"Width") || !lstrcmpW(name,L"Height"))
			{
				INT number;
				CheckIf(!value,E_INVALIDARG);
				Check(value->GetInteger(&number));
				CheckIf(number < 1 || number > QFI::MaxImageListCell,E_INVALIDARG);
			}
			else if(!lstrcmpW(name,L"Images"))
			{
				TStackRef<QFI::CImageListData> data;
				Check(QFI::CImageListData::Create(value,16,16,32,TRUE,&data));
			}
		Cleanup:
			return hr;
		}
	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooImageList)
			UNK_CHAIN_TO(CNonvisual)
		END_UNK_MAP
		~CImageList()
		{
			ClearCache();
			for(sysint n = 0; n < m_imageObservers.Length(); n++)
			{
				m_imageObservers[n]->Release();
			}
		}
		STDMETHOD(Acquire)(UINT dpi, IQuadooImageListSnapshot** snapshot)
		{
			HRESULT hr;
			Cache entry = {dpi,NULL};
			TStackRef<IJSONObject> definition;
			CheckIf(!snapshot,E_POINTER);
			*snapshot = NULL;
			Check(CheckAlive());
			CheckIf(dpi < 48 || dpi > 768,E_INVALIDARG);
			for(sysint n = 0; n < m_cache.Length(); n++)
			{
				if(m_cache[n].dpi == dpi)
				{
					*snapshot = m_cache[n].snapshot;
					(*snapshot)->AddRef();
					hr = S_OK;
					goto Cleanup;
				}
			}
			Check(NativeDefinition(&definition));
			Check(Build(dpi,&entry.snapshot));
			Check(m_cache.Append(entry));
			m_nativeDefinition = definition;
			if(m_cache.Length() > 8)
			{
				m_cache[0].snapshot->Release();
				m_cache.Remove(0,NULL);
			}
			*snapshot = entry.snapshot;
			(*snapshot)->AddRef();
			entry.snapshot = NULL;
		Cleanup:
			SafeRelease(entry.snapshot);
			return hr;
		}
		STDMETHOD(AddImageListObserver)(IQuadooImageListObserver* observer)
		{
			HRESULT hr;
			Check(CheckAlive());
			CheckIf(!observer,E_INVALIDARG);
			for(sysint n = 0; n < m_imageObservers.Length(); n++)
			{
				CheckIfIgnore(SameObject(m_imageObservers[n],observer),S_FALSE);
			}
			Check(m_imageObservers.Append(observer));
			observer->AddRef();
		Cleanup:
			return hr;
		}
		STDMETHOD(RemoveImageListObserver)(IQuadooImageListObserver* observer)
		{
			HRESULT hr;
			Check(CheckThread());
			for(sysint n = 0; n < m_imageObservers.Length(); n++)
			{
				if(SameObject(m_imageObservers[n],observer))
				{
					IQuadooImageListObserver* old = m_imageObservers[n];
					m_imageObservers.Remove(n,NULL);
					old->Release();
					hr = S_OK;
					goto Cleanup;
				}
			}
			hr = S_FALSE;
		Cleanup:
			return hr;
		}
		static const TypeSpec* Type()
		{
			return &imageListType;
		}
		CImageList(IQuadooConstructionContext* context, IQuadooComponentCollection* owner, IUnknown* factory, UINT* live) : CNonvisual(context,owner,factory,live), m_imagesNotifying(FALSE)
		{
		}
		STDMETHOD(GetValueJSON)(UINT index, IJSONValue** value)
		{
			HRESULT hr;
			TStackRef<IJSONArray> images;
			CheckIf(!value,E_POINTER);
			*value = NULL;
			Check(CheckThread());
			CheckIf(index >= static_cast<UINT>(m_values.Length()),HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
			if(index == Find(L"ImageCount"))
			{
				UINT imageIndex = Find(L"Images");
				CheckIf(imageIndex >= static_cast<UINT>(m_values.Length()),E_UNEXPECTED);
				if(m_values[imageIndex].value)
				{
					Check(m_values[imageIndex].value->GetArray(&images));
				}
				Check(JSONCreateInteger(images ? static_cast<INT>(images->Count()) : 0,value));
			}
			else if(index == Find(L"Images") && !m_values[index].value)
			{
				Check(JSONCreateArray(&images));
				Check(JSONWrapArray(images,value));
			}
			else
			{
				Check(CNonvisual::GetValueJSON(index,value));
			}
		Cleanup:
			return hr;
		}
		virtual HRESULT ChangedProperty(PCWSTR name)
		{
			HRESULT hr = S_OK;
			TArray<Cache> candidate;
			TArray<IQuadooImageListObserver*> observers;
			TStackRef<IJSONObject> definition;
			BOOL notifying = FALSE;
			if(lstrcmpW(name,L"Name"))
			{
				Check(NativeDefinition(&definition));
				CheckIfIgnore(m_nativeDefinition && JSONCompareObjects(definition,m_nativeDefinition) == 0,S_OK);
				for(sysint n = 0; n < m_cache.Length(); n++)
				{
					Cache entry = {m_cache[n].dpi,NULL};
					Check(Build(entry.dpi,&entry.snapshot));
					hr = candidate.Append(entry);
					if(FAILED(hr))
					{
						entry.snapshot->Release();
						goto Cleanup;
					}
				}
				for(sysint n = 0; n < m_imageObservers.Length(); n++)
				{
					Check(observers.Append(m_imageObservers[n]));
					m_imageObservers[n]->AddRef();
				}
				ClearCache();
				m_cache.Swap(candidate);
				m_nativeDefinition = definition;
				m_imagesNotifying = TRUE;
				notifying = TRUE;
				for(sysint n = 0; n < observers.Length(); n++)
				{
					observers[n]->OnImageListChanged(this);
					if(m_state >= QuadooForm::Destroying)
					{
						break;
					}
				}
				m_imagesNotifying = FALSE;
				notifying = FALSE;
			}
			if(!lstrcmpW(name,L"Images"))
			{
				Check(Notify(Find(L"ImageCount")));
			}
		Cleanup:
			if(notifying)
			{
				m_imagesNotifying = FALSE;
			}
			for(sysint n = 0; n < observers.Length(); n++)
			{
				observers[n]->Release();
			}
			for(sysint n = 0; n < candidate.Length(); n++)
			{
				candidate[n].snapshot->Release();
			}
			return hr;
		}
	};
	HRESULT RegisterImageListFactory(IQuadooComponentRegistry* registry)
	{
		HRESULT hr;
		TStackRef<TComponentFactory<CImageList> > factory;
		factory.Attach(__new TComponentFactory<CImageList>);
		CheckAlloc(factory);
		Check(factory->Initialize());
		Check(registry->RegisterComponentFactory(factory));
	Cleanup:
		return hr;
	}
}
