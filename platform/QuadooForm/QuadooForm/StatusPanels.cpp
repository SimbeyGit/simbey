#include "StatusBar.h"

namespace QF
{
	static const PropertySpec panelProperties[] =
	{
		{L"Text",QuadooProperty::String,QuadooForm::PropertySerializable,0,0},
		{L"Width",QuadooProperty::Int,QuadooForm::PropertySerializable,100,1,NULL,0,NULL,0,1000000,TRUE},
		{L"Icon",QuadooProperty::Int,QuadooForm::PropertySerializable,-1,2,NULL,0,NULL,-1,INT_MAX,TRUE}
	};
	static HRESULT EntryMetadata(BOOL panel, IQuadooPropertyDescriptors** descriptors)
	{
		HRESULT hr = S_OK;
		TArray<const PropertySpec*> properties;
		TArray<const EventSpec*> events;
		for(UINT n = 0; panel && n < ARRAYSIZE(panelProperties); n++)
		{
			Check(properties.Append(panelProperties + n));
		}
		Check(CreateDescriptors(properties,events,descriptors));
	Cleanup:
		return hr;
	}
	CStatusLink::CStatusLink(CFactory* factory) : m_factory(factory), thread(GetCurrentThreadId()), bar(NULL)
	{
		m_factory->AddInstance();
	}
	CStatusLink::~CStatusLink()
	{
		m_factory->RemoveInstance();
	}
	HRESULT CStatusLink::Alive()
	{
		HRESULT hr;
		CheckIf(thread != GetCurrentThreadId(),QuadooForm::EWrongThread);
		CheckIf(!bar,QuadooForm::EDestroyed);
		QuadooForm::ComponentState state;
		Check(bar->GetState(&state));
		CheckIf(state >= QuadooForm::Destroying,QuadooForm::EDestroyed);
	Cleanup:
		return hr;
	}
	CStatusEntry::CStatusEntry(CStatusLink* link) : m_link(link), m_attached(TRUE)
	{
		m_id = GUID_NULL;
	}
	VOID CStatusEntry::Detach()
	{
		m_attached = FALSE;
	}
	HRESULT CStatusEntry::Alive()
	{
		HRESULT hr;
		Check(m_link->Alive());
		CheckIf(!m_attached,QuadooForm::EDestroyed);
	Cleanup:
		return hr;
	}
	STDMETHODIMP CStatusEntry::GetID(GUID* id)
	{
		HRESULT hr = S_OK;
		CheckIf(!id,E_POINTER);
		*id = GUID_NULL;
		CheckIf(m_link->thread != GetCurrentThreadId(),QuadooForm::EWrongThread);
		*id = m_id;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CStatusEntry::GetName(RSTRING* name)
	{
		HRESULT hr;
		CheckIf(!name,E_POINTER);
		*name = NULL;
		Check(Alive());
		RStrSet(*name,EntryType());
	Cleanup:
		return hr;
	}
	STDMETHODIMP CStatusEntry::SetName(RSTRING)
	{
		return E_ACCESSDENIED;
	}
	STDMETHODIMP CStatusEntry::GetTypeName(RSTRING* name)
	{
		HRESULT hr;
		CheckIf(!name,E_POINTER);
		*name = NULL;
		Check(Alive());
		RStrSet(*name,EntryType());
	Cleanup:
		return hr;
	}
	STDMETHODIMP CStatusEntry::GetProperties(IQuadooProperties** properties)
	{
		HRESULT hr;
		CheckIf(!properties,E_POINTER);
		*properties = NULL;
		Check(Alive());
		*properties = this;
		AddRef();
	Cleanup:
		return hr;
	}
	STDMETHODIMP CStatusEntry::GetState(QuadooForm::ComponentState* state)
	{
		HRESULT hr = S_OK;
		CheckIf(!state,E_POINTER);
		*state = QuadooForm::Destroyed;
		CheckIf(m_link->thread != GetCurrentThreadId(),QuadooForm::EWrongThread);
		if(m_attached && m_link->bar)
		{
			Check(m_link->bar->GetState(state));
		}
	Cleanup:
		return hr;
	}
	STDMETHODIMP CStatusEntry::Destroy()
	{
		return E_ACCESSDENIED;
	}
	STDMETHODIMP CStatusEntry::GetDescriptors(IQuadooPropertyDescriptors** descriptors)
	{
		HRESULT hr;
		CheckIf(!descriptors,E_POINTER);
		*descriptors = NULL;
		Check(Alive());
		Check(EntryDescriptors(descriptors));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CStatusEntry::GetValueJSON(UINT index, IJSONValue** value)
	{
		HRESULT hr;
		CheckIf(!value,E_POINTER);
		*value = NULL;
		Check(Alive());
		Check(Read(index,value));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CStatusEntry::SetValueJSON(UINT index, IJSONValue* value)
	{
		HRESULT hr;
		TStackRef<CStatusEntry> keepAlive(this);
		TStackRef<CStatusBar> bar;
		Check(Alive());
		bar = m_link->bar;
		CheckIf(bar->m_imageBusy,QuadooForm::EInvalidState);
		Check(Write(index,value));
	Cleanup:
		return hr;
	}
	CStatusPanel::CStatusPanel(CStatusLink* link) : CStatusEntry(link), width(100), image(-1)
	{
		text = RSTRING_CAST(L"");
	}
	RSTRING CStatusPanel::EntryType() const
	{
		return RSTRING_CAST(L"StatusPanel");
	}
	HRESULT CStatusPanel::EntryDescriptors(IQuadooPropertyDescriptors** descriptors)
	{
		return EntryMetadata(TRUE,descriptors);
	}
	HRESULT CStatusPanel::Initialize(IJSONObject* definition)
	{
		HRESULT hr;
		TStackRef<IJSONValue> id;
		if(definition && definition->HasField(RSTRING_CAST(L"id")))
		{
			Check(definition->FindNonNullValueW(L"id",&id));
			Check(id->GetStringAsGuid(m_id));
		}
		else
		{
			Check(CoCreateGuid(&m_id));
		}
		CheckIf(IsEqualGUID(m_id,GUID_NULL),E_INVALIDARG);
		if(definition)
		{
			text = static_cast<RSTRING>(NULL);
			Check(ReadString(definition,L"Text",L"",&text));
			Check(ReadInt(definition,L"Width",100,&width));
			Check(ReadInt(definition,L"Icon",-1,&image));
		}
		CheckIf(width < 0 || width > 1000000 || image < -1 || text.Length() > 32767,E_INVALIDARG);
	Cleanup:
		return hr;
	}
	HRESULT CStatusPanel::Serialize(IJSONObject** definition)
	{
		HRESULT hr;
		TStackRef<IJSONObject> object;
		CRString id;
		Check(JSONCreateObject(&object));
		Check(GuidString(m_id,&id));
		Check(PutString(object,L"id",id));
		Check(PutString(object,L"Text",text));
		Check(PutInt(object,L"Width",width));
		Check(PutInt(object,L"Icon",image));
		*definition = object.Detach();
	Cleanup:
		return hr;
	}
	HRESULT CStatusPanel::Read(UINT index, IJSONValue** value)
	{
		HRESULT hr;
		CheckIf(index >= ARRAYSIZE(panelProperties),DISP_E_BADINDEX);
		if(!index)
		{
			Check(JSONCreateString(text,value));
		}
		else
		{
			Check(JSONCreateInteger(index == 1 ? width : image,value));
		}
	Cleanup:
		return hr;
	}
	HRESULT CStatusPanel::Write(UINT index, IJSONValue* value)
	{
		HRESULT hr;
		TStackRef<IJSONValue> normalized;
		CRString previous = text;
		INT oldWidth = width;
		INT oldImage = image;
		BOOL committed = FALSE;
		CheckIf(index >= ARRAYSIZE(panelProperties),DISP_E_BADINDEX);
		Check(NormalizeValue(panelProperties + index,value,&normalized));
		if(!index)
		{
			CRString next;
			Check(normalized->GetString(&next));
			CheckIf(next.Length() > 32767,E_INVALIDARG);
			text = next;
		}
		else if(index == 1)
		{
			Check(normalized->GetInteger(&width));
		}
		else
		{
			Check(normalized->GetInteger(&image));
		}
		hr = m_link->bar->ChangedPanels(&committed);
		if(FAILED(hr) && !committed && m_link->bar && m_attached)
		{
			text = previous;
			width = oldWidth;
			image = oldImage;
			m_link->bar->RebuildPanels();
		}
	Cleanup:
		return hr;
	}
	STDMETHODIMP CStatusPanel::GetIndex(UINT* index)
	{
		HRESULT hr;
		CheckIf(!index,E_POINTER);
		*index = 0;
		Check(Alive());
		Check(m_link->bar->Locate(this,index));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CStatusPanel::GetName(RSTRING* name)
	{
		HRESULT hr;
		UINT index;
		CheckIf(!name,E_POINTER);
		*name = NULL;
		Check(GetIndex(&index));
		Check(RStrFormatW(name,L"[%u] %r",index,static_cast<RSTRING>(text)));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CStatusPanel::GetText(RSTRING* value)
	{
		return GetValueString(0,value);
	}
	STDMETHODIMP CStatusPanel::SetText(RSTRING value)
	{
		return SetValueString(0,value);
	}
	STDMETHODIMP CStatusPanel::GetWidth(INT* value)
	{
		return GetValueInt(1,value);
	}
	STDMETHODIMP CStatusPanel::SetWidth(INT value)
	{
		return SetValueInt(1,value);
	}
	STDMETHODIMP CStatusPanel::GetIcon(INT* value)
	{
		return GetValueInt(2,value);
	}
	STDMETHODIMP CStatusPanel::SetIcon(INT value)
	{
		return SetValueInt(2,value);
	}
	STDMETHODIMP CStatusPanel::GetOutlineParentID(GUID* id)
	{
		HRESULT hr;
		Check(Alive());
		Check(m_link->bar->m_panels->GetID(id));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CStatusPanel::GetOutlineCount(UINT* count)
	{
		HRESULT hr;
		CheckIf(!count,E_POINTER);
		*count = 0;
		Check(Alive());
	Cleanup:
		return hr;
	}
	STDMETHODIMP CStatusPanel::GetOutlineItem(UINT, IQuadooComponent** component)
	{
		HRESULT hr;
		CheckIf(!component,E_POINTER);
		*component = NULL;
		Check(Alive());
		hr = DISP_E_BADINDEX;
	Cleanup:
		return hr;
	}
	CStatusPanels::CStatusPanels(CStatusLink* link, REFGUID owner) : CStatusEntry(link)
	{
		// Deterministic group identity survives JSON reload without another field.
		m_id = owner;
		m_id.Data1 ^= 0x53544154;
		m_id.Data4[7] ^= 0x50;
	}
	RSTRING CStatusPanels::EntryType() const
	{
		return RSTRING_CAST(L"Panels");
	}
	HRESULT CStatusPanels::EntryDescriptors(IQuadooPropertyDescriptors** descriptors)
	{
		return EntryMetadata(FALSE,descriptors);
	}
	HRESULT CStatusPanels::Read(UINT, IJSONValue**)
	{
		return DISP_E_BADINDEX;
	}
	HRESULT CStatusPanels::Write(UINT, IJSONValue*)
	{
		return DISP_E_BADINDEX;
	}
	STDMETHODIMP CStatusPanels::GetCount(UINT* count)
	{
		HRESULT hr;
		CheckIf(!count,E_POINTER);
		*count = 0;
		Check(Alive());
		*count = static_cast<UINT>(m_link->bar->m_items.Length());
	Cleanup:
		return hr;
	}
	STDMETHODIMP CStatusPanels::GetAt(UINT index, IQuadooStatusPanel** panel)
	{
		HRESULT hr;
		CheckIf(!panel,E_POINTER);
		*panel = NULL;
		Check(Alive());
		CheckIf(index >= static_cast<UINT>(m_link->bar->m_items.Length()),DISP_E_BADINDEX);
		*panel = m_link->bar->m_items[index];
		(*panel)->AddRef();
	Cleanup:
		return hr;
	}
	STDMETHODIMP CStatusPanels::Insert(UINT index, IQuadooStatusPanel** panel)
	{
		HRESULT hr;
		TStackRef<CStatusPanel> entry;
		TStackRef<CStatusBar> bar;
		BOOL committed = FALSE;
		CheckIf(!panel,E_POINTER);
		*panel = NULL;
		Check(Alive());
		bar = m_link->bar;
		CheckIf(bar->m_imageBusy,QuadooForm::EInvalidState);
		CheckIf(index > static_cast<UINT>(bar->m_items.Length()),DISP_E_BADINDEX);
		CheckIf(bar->m_items.Length() >= StatusPanelLimit,E_INVALIDARG);
		entry.Attach(__new CStatusPanel(m_link));
		CheckAlloc(entry);
		Check(entry->Initialize(NULL));
		CStatusPanel* pointer = entry;
		Check(bar->m_items.InsertAt(&pointer,index));
		pointer->AddRef();
		hr = bar->ChangedPanels(&committed);
		if(FAILED(hr))
		{
			if(!committed && m_link->bar && SUCCEEDED(bar->Locate(entry,&index)))
			{
				bar->m_items.Remove(index,NULL);
				entry->Detach();
				entry->Release();
				bar->RebuildPanels();
			}
			goto Cleanup;
		}
		Check(entry->GetIndex(&index));
		*panel = entry.Detach();
	Cleanup:
		return hr;
	}
	HRESULT CStatusBar::Locate(IQuadooStatusPanel* panel, UINT* index)
	{
		HRESULT hr = S_OK;
		CheckIf(!panel,E_INVALIDARG);
		for(sysint n = 0; n < m_items.Length(); n++)
		{
			if(SameObject(static_cast<IQuadooStatusPanel*>(m_items[n]),panel))
			{
				*index = static_cast<UINT>(n);
				goto Cleanup;
			}
		}
		hr = E_INVALIDARG;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CStatusPanels::Delete(IQuadooStatusPanel* panel)
	{
		HRESULT hr;
		UINT index;
		BOOL committed = FALSE;
		TStackRef<CStatusBar> bar;
		TStackRef<CStatusPanel> entry;
		Check(Alive());
		bar = m_link->bar;
		CheckIf(bar->m_imageBusy,QuadooForm::EInvalidState);
		Check(bar->Locate(panel,&index));
		entry.Attach(bar->m_items[index]);
		bar->m_items.Remove(index,NULL);
		entry->Detach();
		hr = bar->ChangedPanels(&committed);
		if(FAILED(hr) && !committed && m_link->bar)
		{
			CStatusPanel* pointer = entry;
			if(SUCCEEDED(bar->m_items.InsertAt(&pointer,index)))
			{
				entry->AddRef();
				entry->m_attached = TRUE;
				bar->RebuildPanels();
			}
		}
	Cleanup:
		return hr;
	}
	STDMETHODIMP CStatusPanels::Move(IQuadooStatusPanel* panel, UINT index)
	{
		HRESULT hr;
		UINT previous;
		TStackRef<CStatusBar> bar;
		BOOL committed = FALSE;
		Check(Alive());
		bar = m_link->bar;
		CheckIf(bar->m_imageBusy,QuadooForm::EInvalidState);
		CheckIf(index >= static_cast<UINT>(bar->m_items.Length()),DISP_E_BADINDEX);
		Check(bar->Locate(panel,&previous));
		if(index != previous)
		{
			CStatusPanel* entry = bar->m_items[previous];
			bar->m_items.Remove(previous,NULL);
			// Capacity cannot shrink during Remove; this insertion cannot allocate.
			Check(bar->m_items.InsertAt(&entry,index));
			hr = bar->ChangedPanels(&committed);
			if(FAILED(hr) && !committed && m_link->bar)
			{
				bar->m_items.Remove(index,NULL);
				bar->m_items.InsertAt(&entry,previous);
				bar->RebuildPanels();
			}
		}
	Cleanup:
		return hr;
	}
	STDMETHODIMP CStatusPanels::Clear()
	{
		HRESULT hr;
		TStackRef<CStatusBar> bar;
		TArray<CStatusPanel*> previous;
		BOOL committed = FALSE;
		Check(Alive());
		bar = m_link->bar;
		CheckIf(bar->m_imageBusy,QuadooForm::EInvalidState);
		bar->m_items.Swap(previous);
		for(sysint n = 0; n < previous.Length(); n++)
		{
			previous[n]->Detach();
		}
		hr = bar->ChangedPanels(&committed);
		if(FAILED(hr) && !committed && m_link->bar)
		{
			bar->m_items.Swap(previous);
			for(sysint n = 0; n < bar->m_items.Length(); n++)
			{
				bar->m_items[n]->m_attached = TRUE;
			}
			bar->RebuildPanels();
		}
	Cleanup:
		for(sysint n = 0; n < previous.Length(); n++)
		{
			previous[n]->Release();
		}
		return hr;
	}
	STDMETHODIMP CStatusPanels::GetOutlineParentID(GUID* id)
	{
		HRESULT hr;
		Check(Alive());
		Check(m_link->bar->GetID(id));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CStatusPanels::GetOutlineCount(UINT* count)
	{
		return GetCount(count);
	}
	STDMETHODIMP CStatusPanels::GetOutlineItem(UINT index, IQuadooComponent** component)
	{
		HRESULT hr;
		TStackRef<IQuadooStatusPanel> panel;
		CheckIf(!component,E_POINTER);
		*component = NULL;
		Check(GetAt(index,&panel));
		Check(panel->QueryInterface(__uuidof(IQuadooComponent),reinterpret_cast<PVOID*>(component)));
	Cleanup:
		return hr;
	}

	STDMETHODIMP CStatusEntry::GetValueInt(UINT index, INT* value)
	{
		HRESULT hr;
		TStackRef<IJSONValue> json;
		CheckIf(!value,E_POINTER);
		*value = 0;
		Check(GetValueJSON(index,&json));
		Check(json->GetInteger(value));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CStatusEntry::SetValueInt(UINT index, INT value)
	{
		HRESULT hr;
		TStackRef<IJSONValue> json;
		Check(JSONCreateInteger(value,&json));
		Check(SetValueJSON(index,json));
	Cleanup:
		return hr;
	}

	STDMETHODIMP CStatusEntry::GetValueLong(UINT index, LONGLONG* value)
	{
		HRESULT hr;
		TStackRef<IJSONValue> json;
		CheckIf(!value,E_POINTER);
		*value = 0;
		Check(GetValueJSON(index,&json));
		Check(json->GetLongInteger(value));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CStatusEntry::SetValueLong(UINT index, LONGLONG value)
	{
		HRESULT hr;
		TStackRef<IJSONValue> json;
		Check(JSONCreateLongInteger(value,&json));
		Check(SetValueJSON(index,json));
	Cleanup:
		return hr;
	}

	STDMETHODIMP CStatusEntry::GetValueString(UINT index, RSTRING* value)
	{
		HRESULT hr;
		TStackRef<IJSONValue> json;
		CheckIf(!value,E_POINTER);
		*value = NULL;
		Check(GetValueJSON(index,&json));
		Check(json->GetString(value));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CStatusEntry::SetValueString(UINT index, RSTRING value)
	{
		HRESULT hr;
		TStackRef<IJSONValue> json;
		Check(JSONCreateString(value,&json));
		Check(SetValueJSON(index,json));
	Cleanup:
		return hr;
	}

	STDMETHODIMP CStatusEntry::GetValueBool(UINT index, BOOL* value)
	{
		HRESULT hr;
		TStackRef<IJSONValue> json;
		CheckIf(!value,E_POINTER);
		*value = FALSE;
		Check(GetValueJSON(index,&json));
		bool number = false;
		Check(json->GetBoolean(&number));
		*value = number;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CStatusEntry::SetValueBool(UINT index, BOOL value)
	{
		HRESULT hr;
		TStackRef<IJSONValue> json;
		Check(JSONCreateBool(value != FALSE,&json));
		Check(SetValueJSON(index,json));
	Cleanup:
		return hr;
	}

	STDMETHODIMP CStatusEntry::GetValueFloat(UINT index, DOUBLE* value)
	{
		HRESULT hr;
		TStackRef<IJSONValue> json;
		CheckIf(!value,E_POINTER);
		*value = 0.0;
		Check(GetValueJSON(index,&json));
		Check(json->GetDouble(value));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CStatusEntry::SetValueFloat(UINT index, DOUBLE value)
	{
		HRESULT hr;
		TStackRef<IJSONValue> json;
		Check(JSONCreateDouble(value,&json));
		Check(SetValueJSON(index,json));
	Cleanup:
		return hr;
	}

	STDMETHODIMP CStatusEntry::GetValueEvent(UINT, RSTRING* value)
	{
		if(value)
		{
			*value = NULL;
		}
		return value ? DISP_E_TYPEMISMATCH : E_POINTER;
	}
	STDMETHODIMP CStatusEntry::SetValueEvent(UINT, RSTRING)
	{
		return DISP_E_TYPEMISMATCH;
	}
}
