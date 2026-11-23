#include "Designer.h"

namespace QFD
{
	static HRESULT EventDescriptor(CDesignerDocument* document, IQuadooComponent* component, RSTRING name, IQuadooEventDescriptor** descriptor)
	{
		HRESULT hr;
		CRString type;
		TStackRef<IQuadooControlFactory> factory;
		TStackRef<IQuadooEventDescriptors> events;
		Check(component->GetTypeName(&type));
		Check(document->Registry()->FindFactory(type,&factory));
		Check(factory->GetEvents(&events));
		Check(events->Find(name,descriptor));
	Cleanup:
		return hr;
	}

	static HRESULT CompatibleEvent(CDesignerDocument* document, IQuadooComponent* primary, IQuadooComponent* other, RSTRING name, BOOL* compatible)
	{
		HRESULT hr;
		TStackRef<IQuadooEventDescriptor> first;
		TStackRef<IQuadooEventDescriptor> second;
		UINT count;
		UINT otherCount;
		DWORD flags;
		DWORD otherFlags;
		*compatible = FALSE;
		Check(EventDescriptor(document,primary,name,&first));
		Check(EventDescriptor(document,other,name,&second));
		Check(first->GetArgumentCount(&count));
		Check(second->GetArgumentCount(&otherCount));
		Check(first->GetFlags(&flags));
		Check(second->GetFlags(&otherFlags));
		CheckIfIgnore(count != otherCount || flags != otherFlags,S_OK);
		CheckIf(count > 4096,E_INVALIDARG);
		for(UINT n = 0; n < count; n++)
		{
			CRString argument;
			CRString otherArgument;
			QuadooForm::EventValueType type;
			QuadooForm::EventValueType otherType;
			BOOL writable;
			BOOL otherWritable;
			Check(first->GetArgument(n,&argument,&type,&writable));
			Check(second->GetArgument(n,&otherArgument,&otherType,&otherWritable));
			CheckIfIgnore(type != otherType || writable != otherWritable || !(argument == static_cast<RSTRING>(otherArgument)),S_OK);
		}
		*compatible = TRUE;
	Cleanup:
		return hr;
	}

	static HRESULT CompatibleProperty(IQuadooPropertyDescriptor* first, IQuadooPropertyDescriptor* other, BOOL* compatible)
	{
		HRESULT hr;
		QuadooProperty::Type a;
		QuadooProperty::Type b;
		UINT count;
		UINT otherCount;
		*compatible = FALSE;
		Check(first->GetType(&a));
		Check(other->GetType(&b));
		CheckIfIgnore(a != b,S_OK);
		Check(first->GetEnumCount(&count));
		Check(other->GetEnumCount(&otherCount));
		CheckIfIgnore(count != otherCount,S_OK);
		CheckIf(count > 4096,E_INVALIDARG);
		for(UINT n = 0; n < count; n++)
		{
			CRString name;
			CRString otherName;
			LONGLONG value;
			LONGLONG otherValue;
			Check(first->GetEnumValue(n,&name,&value));
			Check(other->GetEnumValue(n,&otherName,&otherValue));
			CheckIfIgnore(value != otherValue || !(name == static_cast<RSTRING>(otherName)),S_OK);
		}
		*compatible = TRUE;
	Cleanup:
		return hr;
	}

	CSelectionProperties::CSelectionProperties() : m_document(NULL)
	{
	}

	CSelectionProperties::~CSelectionProperties()
	{
		for(sysint n = 0; n < m_entries.Length(); n++)
		{
			m_entries[n].descriptor->Release();
		}
	}

	HRESULT CSelectionProperties::Initialize(CDesignerDocument* document, const GUID* ids, UINT count)
	{
		HRESULT hr;
		TStackRef<IQuadooComponent> primary;
		TStackRef<IQuadooProperties> properties;
		TStackRef<IQuadooPropertyDescriptors> descriptors;
		UINT total;
		CheckIf(!document || !ids || !count || m_document,E_INVALIDARG);
		m_document = document;
		for(UINT n = 0; n < count; n++)
		{
			Check(m_ids.Append(ids[n]));
		}
		Check(document->FindComponent(ids[0],&primary));
		Check(primary->GetProperties(&properties));
		Check(properties->GetDescriptors(&descriptors));
		Check(descriptors->GetCount(&total));
		CheckIf(total > 4096,E_INVALIDARG);
		for(UINT n = 0; n < total; n++)
		{
			TStackRef<IQuadooPropertyDescriptor> descriptor;
			CRString name;
			Entry entry;
			BOOL shared = TRUE;
			Check(descriptors->GetAt(n,&descriptor));
			Check(descriptor->GetName(&name));
			Check(descriptor->GetFlags(&entry.flags));
			for(UINT c = 0; c < count; c++)
			{
				TStackRef<IQuadooComponent> component;
				TStackRef<IQuadooProperties> values;
				TStackRef<IQuadooPropertyDescriptors> metadata;
				TStackRef<IQuadooPropertyDescriptor> candidate;
				TStackRef<IQuadooDesignCapabilities> capabilities;
				TStackRef<IQuadooDesignPlaceholder> placeholder;
				QuadooProperty::Type type;
				UINT index;
				DWORD flags;
				Check(document->FindComponent(ids[c],&component));
				hr = Property(component,name.c_wstr(),&values,&index);
				if(hr == DISP_E_MEMBERNOTFOUND)
				{
					shared = FALSE;
					break;
				}
				Check(hr);
				Check(values->GetDescriptors(&metadata));
				Check(metadata->GetAt(index,&candidate));
				Check(CompatibleProperty(descriptor,candidate,&shared));
				if(!shared)
				{
					break;
				}
				Check(candidate->GetFlags(&flags));
				entry.flags |= flags;
				Check(candidate->GetType(&type));
				if(count > 1 && type == QuadooProperty::Collection && name == RSTRING_CAST(L"Columns"))
				{
					TStackRef<IQuadooListView> list;
					if(SUCCEEDED(component->QueryInterface(__uuidof(IQuadooListView),reinterpret_cast<PVOID*>(&list))))
					{
						entry.flags |= QuadooForm::PropertyReadOnly;
					}
				}
				if(name == RSTRING_CAST(L"Align"))
				{
					TStackRef<IQuadooViewportDock> dock;
					if(SUCCEEDED(component->QueryInterface(&dock)))
					{
						entry.flags |= QuadooForm::PropertyReadOnly;
					}
				}
				if(c && type == QuadooProperty::Event)
				{
					Check(CompatibleEvent(document,primary,component,name,&shared));
					if(!shared)
					{
						break;
					}
				}
				if(SUCCEEDED(component->QueryInterface(__uuidof(IQuadooDesignPlaceholder),reinterpret_cast<PVOID*>(&placeholder))) &&
					(type == QuadooProperty::Collection || type == QuadooProperty::Rect || type == QuadooProperty::Point || type == QuadooProperty::Size))
				{
					entry.flags |= QuadooForm::PropertyReadOnly;
				}
				if(name == RSTRING_CAST(L"Width") || name == RSTRING_CAST(L"Height") || name == RSTRING_CAST(L"Left") || name == RSTRING_CAST(L"Top"))
				{
					DWORD capability = name == RSTRING_CAST(L"Width") ? QuadooForm::DesignResizeWidth :
						name == RSTRING_CAST(L"Height") ? QuadooForm::DesignResizeHeight :
						name == RSTRING_CAST(L"Left") ? QuadooForm::DesignMoveX : QuadooForm::DesignMoveY;
					if(SUCCEEDED(component->QueryInterface(__uuidof(IQuadooDesignCapabilities),reinterpret_cast<PVOID*>(&capabilities))))
					{
						Check(capabilities->GetDesignCapabilities(&flags));
						if(!(flags & capability))
						{
							entry.flags |= QuadooForm::PropertyReadOnly;
						}
					}
				}
			}
			if(shared)
			{
				// Names identify individual components and cannot be assigned in bulk.
				if(count > 1 && name == RSTRING_CAST(L"Name"))
				{
					entry.flags |= QuadooForm::PropertyReadOnly;
				}
				entry.index = n;
				entry.descriptor = descriptor;
				Check(m_entries.Append(entry));
				descriptor.Detach();
			}
		}
		hr = S_OK;
	Cleanup:
		return hr;
	}

	UINT CSelectionProperties::Count() const
	{
		return static_cast<UINT>(m_entries.Length());
	}

	HRESULT CSelectionProperties::GetAt(UINT row, UINT* index, IQuadooPropertyDescriptor** descriptor, DWORD* flags)
	{
		if(!index || !descriptor || !flags)
		{
			return E_POINTER;
		}
		*descriptor = NULL;
		if(row >= Count())
		{
			return E_INVALIDARG;
		}
		*index = m_entries[row].index;
		*flags = m_entries[row].flags;
		*descriptor = m_entries[row].descriptor;
		(*descriptor)->AddRef();
		return S_OK;
	}

	HRESULT CSelectionProperties::Find(RSTRING name, UINT* index, DWORD* flags)
	{
		HRESULT hr = DISP_E_MEMBERNOTFOUND;
		CheckIf(!index || !flags,E_POINTER);
		for(sysint n = 0; n < m_entries.Length(); n++)
		{
			CRString candidate;
			Check(m_entries[n].descriptor->GetName(&candidate));
			if(candidate == name)
			{
				*index = m_entries[n].index;
				*flags = m_entries[n].flags;
				hr = S_OK;
				goto Cleanup;
			}
		}
		hr = DISP_E_MEMBERNOTFOUND;
	Cleanup:
		return hr;
	}

	HRESULT CSelectionProperties::GetDescriptor(UINT index, IQuadooPropertyDescriptor** descriptor)
	{
		if(!descriptor)
		{
			return E_POINTER;
		}
		*descriptor = NULL;
		for(sysint n = 0; n < m_entries.Length(); n++)
		{
			if(m_entries[n].index == index)
			{
				*descriptor = m_entries[n].descriptor;
				(*descriptor)->AddRef();
				return S_OK;
			}
		}
		return DISP_E_MEMBERNOTFOUND;
	}

	HRESULT CSelectionProperties::GetValue(UINT index, IJSONValue** value, BOOL* mixed)
	{
		HRESULT hr = DISP_E_MEMBERNOTFOUND;
		CheckIf(!value || !mixed,E_POINTER);
		*value = NULL;
		*mixed = FALSE;
		for(sysint n = 0; n < m_entries.Length(); n++)
		{
			if(m_entries[n].index == index)
			{
				CRString name;
				Check(m_entries[n].descriptor->GetName(&name));
				for(sysint c = 0; c < m_ids.Length(); c++)
				{
					TStackRef<IQuadooComponent> component;
					TStackRef<IJSONValue> current;
					Check(m_document->FindComponent(m_ids[c],&component));
					Check(ReadProperty(component,name.c_wstr(),&current));
					if(!c)
					{
						*value = current.Detach();
					}
					else if(JSONCompare(*value,current))
					{
						*mixed = TRUE;
					}
				}
				hr = S_OK;
				break;
			}
		}
	Cleanup:
		return hr;
	}

	HRESULT CSelectionProperties::SetValue(UINT index, IJSONValue* value)
	{
		HRESULT hr = DISP_E_MEMBERNOTFOUND;
		for(sysint n = 0; n < m_entries.Length(); n++)
		{
			if(m_entries[n].index == index)
			{
				CRString name;
				CheckIf(m_entries[n].flags & (QuadooForm::PropertyReadOnly | QuadooForm::PropertyRuntimeOnly),E_ACCESSDENIED);
				Check(m_entries[n].descriptor->GetName(&name));
				Check(m_document->SetControlProperties(m_ids.GetItemPtr(0),static_cast<UINT>(m_ids.Length()),name,value));
				break;
			}
		}
	Cleanup:
		return hr;
	}

	UINT CDesignerEditor::SelectionCount() const
	{
		return static_cast<UINT>(m_selection.Length());
	}

	BOOL CDesignerEditor::IsSelected(REFGUID id) const
	{
		for(sysint n = 0; n < m_selection.Length(); n++)
		{
			if(IsEqualGUID(id,m_selection[n]))
			{
				return TRUE;
			}
		}
		return FALSE;
	}

	HRESULT CDesignerEditor::GetSelectionID(UINT index, GUID* id) const
	{
		if(!id)
		{
			return E_POINTER;
		}
		if(index >= SelectionCount())
		{
			return E_INVALIDARG;
		}
		*id = m_selection[index];
		return S_OK;
	}

	HRESULT CDesignerEditor::UpdateSelection(REFGUID id, BOOL add)
	{
		HRESULT hr;
		TStackRef<IQuadooComponent> component;
		TArray<GUID> next;
		Check(m_document.FindComponent(id,&component));
		if(add && m_selection.Length())
		{
			TStackRef<IQuadooControl> current;
			TStackRef<IQuadooControl> control;
			TStackRef<IQuadooControlContainer> parent;
			TStackRef<IQuadooControlContainer> otherParent;
			// Only siblings can be grouped. Roots and nonvisual components stay single.
			CheckIfIgnore(FAILED(Selected(&current)) || FAILED(m_document.Find(id,&control)),S_FALSE);
			Check(current->GetParent(&parent));
			Check(control->GetParent(&otherParent));
			CheckIfIgnore(!parent || !otherParent || !SameObject(parent,otherParent),S_FALSE);
			if(!IsSelected(id))
			{
				Check(m_selection.Append(id));
			}
		}
		else
		{
			Check(next.Append(id));
			m_selection.Swap(next);
		}
		m_selected = id;
		hr = S_OK;
	Cleanup:
		return hr;
	}

	HRESULT CDesignerEditor::NormalizeSelection()
	{
		HRESULT hr = S_OK;
		TArray<GUID> valid;
		for(sysint n = 0; n < m_selection.Length(); n++)
		{
			TStackRef<IQuadooComponent> component;
			if(SUCCEEDED(m_document.FindComponent(m_selection[n],&component)))
			{
				Check(valid.Append(m_selection[n]));
			}
		}
		m_selection.Swap(valid);
		if(!m_document.Root())
		{
			m_selection.Clear();
			m_selected = GUID_NULL;
		}
		else if(!m_selection.Length())
		{
			TStackRef<IQuadooComponent> component;
			if(FAILED(m_document.FindComponent(m_selected,&component)))
			{
				Check(m_document.Root()->GetID(&m_selected));
			}
			Check(m_selection.Append(m_selected));
		}
		else if(!IsSelected(m_selected))
		{
			m_selected = m_selection[m_selection.Length() - 1];
		}
	Cleanup:
		return hr;
	}

	HRESULT CDesignerEditor::SelectionProperties(CSelectionProperties& properties)
	{
		HRESULT hr;
		TArray<GUID> ids;
		Check(ids.Append(m_selected));
		for(sysint n = 0; n < m_selection.Length(); n++)
		{
			if(!IsEqualGUID(m_selection[n],m_selected))
			{
				Check(ids.Append(m_selection[n]));
			}
		}
		Check(properties.Initialize(&m_document,ids.GetItemPtr(0),static_cast<UINT>(ids.Length())));
	Cleanup:
		return hr;
	}
}
