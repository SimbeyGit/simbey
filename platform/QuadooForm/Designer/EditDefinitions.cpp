#include "Designer.h"

namespace QFD
{
	HRESULT FinishSourceEdit(ISourceEdit* edit, HRESULT result)
	{
		HRESULT hr = edit ? edit->Finish(SUCCEEDED(result)) : S_OK;
		return FAILED(hr) ? hr : result;
	}

	static BOOL EqualText(RSTRING first, RSTRING second, BOOL insensitive = FALSE)
	{
		INT result;
		HRESULT hr = insensitive ? RStrCompareIRStr(first,second,&result) : RStrCompareRStr(first,second,&result);
		return SUCCEEDED(hr) && !result;
	}

	static HRESULT DefinitionArray(IJSONObject* object, PCWSTR field, IJSONArray** array)
	{
		HRESULT hr;
		TStackRef<IJSONValue> value;
		CheckIf(!array,E_POINTER);
		*array = NULL;
		if(object->HasField(RSTRING_CAST(field)))
		{
			Check(object->FindNonNullValueW(field,&value));
			Check(value->GetArray(array));
		}
		else
		{
			Check(JSONCreateArray(array));
			Check(JSONWrapArray(*array,&value));
			Check(object->AddValueW(field,value));
		}
	Cleanup:
		return hr;
	}

	static HRESULT FindDefinition(IJSONObject* root, REFGUID id, IJSONObject** result, IJSONArray** siblings = NULL, sysint* index = NULL, UINT depth = 0)
	{
		HRESULT hr;
		TStackRef<IJSONValue> value;
		GUID current;
		static PCWSTR const fields[] = { L"children",L"components" };
		CheckIf(depth > 128,E_INVALIDARG);
		Check(root->FindNonNullValueW(L"id",&value));
		Check(value->GetStringAsGuid(current));
		if(IsEqualGUID(current,id))
		{
			*result = root; root->AddRef(); hr = S_OK; goto Cleanup;
		}
		for(UINT f = 0; f < ARRAYSIZE(fields); f++) if(root->HasField(RSTRING_CAST(fields[f])))
		{
			TStackRef<IJSONArray> children;
			Check(DefinitionArray(root,fields[f],&children));
			for(sysint n = 0; n < children->Count(); n++)
			{
				TStackRef<IJSONObject> child;
				Check(children->GetObject(n,&child));
				hr = FindDefinition(child,id,result,siblings,index,depth + 1);
				if(hr == S_OK)
				{
					if(siblings && !*siblings) { *siblings = children; children->AddRef(); if(index) *index = n; }
					goto Cleanup;
				}
				CheckIf(hr != HRESULT_FROM_WIN32(ERROR_NOT_FOUND),hr);
			}
		}
		hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);

	Cleanup:
		return hr;
	}

	HRESULT CDesignerDocument::CopyDefinition(REFGUID id, IJSONObject** definition)
	{
		HRESULT hr;
		TStackRef<IJSONObject> root;
		GUID rootID;
		CheckIf(!definition,E_POINTER);
		*definition = NULL;
		CheckIf(!m_root,QuadooForm::EInvalidState);
		Check(m_root->GetID(&rootID));
		CheckIf(IsEqualGUID(rootID,id),E_ACCESSDENIED);
		Check(Serialize(&root));
		Check(FindDefinition(root,id,definition));
	Cleanup:
		return hr;
	}

	HRESULT CDesignerDocument::ApplyComponentProperties(REFGUID id, IJSONObject* properties)
	{
		HRESULT hr;
		TStackRef<IJSONObject> root;
		TStackRef<IJSONObject> target;
		TStackRef<IJSONObject> fields;
		TStackRef<IJSONValue> value;
		CheckIf(!properties,E_POINTER);
		Check(Serialize(&root));
		Check(FindDefinition(root,id,&target));
		if(target->HasField(RSTRING_CAST(L"properties")))
		{
			Check(target->FindNonNullValueW(L"properties",&value));
			Check(value->GetObject(&fields));
			value.Release();
		}
		else
		{
			Check(JSONCreateObject(&fields));
			Check(JSONWrapObject(fields,&value));
			Check(target->AddValueW(L"properties",value));
			value.Release();
		}
		for(sysint n = 0; n < properties->Count(); n++)
		{
			CRString name;
			Check(properties->GetValueName(n,&name));
			Check(properties->GetValueByIndex(n,&value));
			Check(fields->AddValue(name,value));
			value.Release();
		}
		Check(ApplyDefinition(root));
	Cleanup:
		return hr;
	}

	HRESULT CDesignerDocument::ApplyDefinition(IJSONObject* definition, const GUID* changedViews, UINT count)
	{
		HRESULT hr;
		CDesignerDocument candidate;
		TStackRef<ISourceEdit> sourceEdit;
		Check(candidate.Initialize(m_registry));
		Check(candidate.Load(GetParent(m_host),definition,m_path));
		// Persisted page selection and the user's current design view are separate.
		// Restore the latter on the candidate before replacing the live document.
		for(sysint n = 0; n < m_pages.Length(); n++)
		{
			BOOL preserve = TRUE;
			for(UINT c = 0; c < count; c++)
			{
				if(IsEqualGUID(changedViews[c],m_pages[n].tabs))
				{
					preserve = FALSE;
					break;
				}
			}
			if(!preserve)
			{
				continue;
			}
			TStackRef<IQuadooControl> oldControl, newControl, newPage;
			TStackRef<IQuadooTabControl> oldTabs, newTabs;
			TStackRef<IQuadooTabPage> active, page;
			GUID id;
			if(FAILED(Find(m_pages[n].tabs,&oldControl)) || FAILED(candidate.Find(m_pages[n].tabs,&newControl))) continue;
			Check(oldControl->QueryInterface(__uuidof(IQuadooTabControl),reinterpret_cast<PVOID*>(&oldTabs)));
			Check(newControl->QueryInterface(__uuidof(IQuadooTabControl),reinterpret_cast<PVOID*>(&newTabs)));
			Check(oldTabs->GetActivePage(&active));
			if(!active)
			{
				Check(newTabs->SetActivePage(NULL));
				continue;
			}
			Check(active->GetID(&id));
			if(FAILED(candidate.Find(id,&newPage))) continue;
			Check(newPage->QueryInterface(__uuidof(IQuadooTabPage),reinterpret_cast<PVOID*>(&page)));
			Check(newTabs->SetActivePage(page));
		}
		if(m_sourceSite)
		{
			Check(m_sourceSite->PrepareDefinition(definition,&sourceEdit));
		}
		Clear();
		SwapData(m_usage,candidate.m_usage);
		SwapData(m_root,candidate.m_root); SwapData(m_context,candidate.m_context); SwapData(m_host,candidate.m_host);
		m_pages.Swap(candidate.m_pages); m_path = candidate.m_path; m_form = candidate.m_form; m_dirty = TRUE;
		hr = S_OK;
	Cleanup:
		return FinishSourceEdit(sourceEdit,hr);
	}

	HRESULT CDesignerDocument::PrepareInsert(IQuadooComponent* parent, IJSONObject* item, BOOL component, ISourceEdit** edit)
	{
		HRESULT hr;
		TStackRef<IJSONObject> root, target;
		TStackRef<IJSONArray> items;
		TStackRef<IJSONValue> value;
		GUID id;
		WCHAR text[40];
		CRString identity;

		*edit = NULL;
		CheckIfIgnore(!m_sourceSite, S_OK);

		Check(CoCreateGuid(&id));
		CheckIf(!StringFromGUID2(id,text,ARRAYSIZE(text)),E_FAIL);
		Check(RStrCreateW(TStrLenAssert(text),text,&identity));
		Check(PutString(item,L"id",identity));
		Check(Serialize(&root));
		Check(parent->GetID(&id));
		Check(FindDefinition(root,id,&target));
		Check(DefinitionArray(target,component ? L"components" : L"children",&items));
		Check(JSONWrapObject(item,&value));
		Check(items->Add(value));
		Check(m_sourceSite->PrepareDefinition(root,edit));

	Cleanup:
		return hr;
	}

	HRESULT CDesignerDocument::PrepareProperty(REFGUID id, RSTRING name, BOOL event, IJSONValue* value, ISourceEdit** edit)
	{
		HRESULT hr = S_OK;
		TStackRef<IJSONObject> root, item, fields;
		TStackRef<IJSONValue> wrapped;
		*edit = NULL;
		if(!m_sourceSite) goto Cleanup;
		Check(Serialize(&root));
		Check(FindDefinition(root,id,&item));
		if(event)
		{
			if(item->HasField(RSTRING_CAST(L"events")))
			{
				Check(item->FindNonNullValueW(L"events",&wrapped));
				Check(wrapped->GetObject(&fields));
			}
			else
			{
				Check(JSONCreateObject(&fields));
				Check(JSONWrapObject(fields,&wrapped));
				Check(item->AddValueW(L"events",wrapped));
			}
			Check(fields->AddValue(name,value));
		}
		else
		{
			Check(item->AddValueW(L"name",value));
		}
		Check(m_sourceSite->PrepareDefinition(root,edit));
	Cleanup:
		return hr;
	}

	static HRESULT ClearRemovedReferences(CDesignerDocument* document, IJSONObject* object, REFGUID removed, UINT depth = 0)
	{
		HRESULT hr;
		TStackRef<IJSONValue> identity;
		TStackRef<IJSONValue> value;
		TStackRef<IJSONObject> properties;
		TStackRef<IQuadooComponent> component;
		TStackRef<IQuadooProperties> values;
		TStackRef<IQuadooPropertyDescriptors> descriptors;
		GUID id;
		UINT count;
		CheckIf(depth > 128,E_INVALIDARG);
		Check(object->FindNonNullValueW(L"id",&identity));
		Check(identity->GetStringAsGuid(id));
		Check(document->FindComponent(id,&component));
		Check(component->GetProperties(&values));
		Check(values->GetDescriptors(&descriptors));
		Check(descriptors->GetCount(&count));
		if(object->HasField(RSTRING_CAST(L"properties")))
		{
			Check(object->FindNonNullValueW(L"properties",&value));
			Check(value->GetObject(&properties));
			value.Release();
			for(UINT n = 0; n < count; n++)
			{
				TStackRef<IQuadooPropertyDescriptor> descriptor;
				TStackRef<IJSONValue> reference;
				QuadooProperty::Type type;
				CRString name;
				Check(descriptors->GetAt(n,&descriptor));
				Check(descriptor->GetType(&type));
				if(type == QuadooProperty::ObjectReference)
				{
					Check(descriptor->GetName(&name));
					if(properties->HasField(name))
					{
						Check(properties->FindValue(name,&reference));
						GUID target;
						if(reference && SUCCEEDED(reference->GetStringAsGuid(target)) && IsEqualGUID(target,removed))
						{
							Check(properties->AddValue(name,NULL));
						}
					}
				}
			}
		}
		static PCWSTR const fields[] = {L"children",L"components"};
		for(UINT f = 0; f < ARRAYSIZE(fields); f++)
		{
			if(object->HasField(RSTRING_CAST(fields[f])))
			{
				TStackRef<IJSONArray> children;
				Check(DefinitionArray(object,fields[f],&children));
				for(sysint n = 0; n < children->Count(); n++)
				{
					TStackRef<IJSONObject> child;
					Check(children->GetObject(n,&child));
					Check(ClearRemovedReferences(document,child,removed,depth + 1));
				}
			}
		}
		hr = S_OK;
	Cleanup:
		return hr;
	}

	static HRESULT SelectionParent(CDesignerDocument* document, const GUID* ids, UINT count, GUID* parentID)
	{
		HRESULT hr = S_OK;
		CheckIf(!ids || !count || count > 1000,E_INVALIDARG);
		for(UINT n = 0; n < count; n++)
		{
			TStackRef<IQuadooControl> control;
			TStackRef<IQuadooControlContainer> parent;
			GUID id;
			Check(document->Find(ids[n],&control));
			Check(control->GetParent(&parent));
			CheckIf(!parent,E_ACCESSDENIED);
			Check(parent->GetID(&id));
			if(!n)
			{
				*parentID = id;
			}
			else
			{
				CheckIf(!IsEqualGUID(id,*parentID),E_INVALIDARG);
			}
			for(UINT previous = 0; previous < n; previous++)
			{
				CheckIf(IsEqualGUID(ids[n],ids[previous]),E_INVALIDARG);
			}
		}
	Cleanup:
		return hr;
	}

	HRESULT CDesignerDocument::CopyControls(const GUID* ids, UINT count, IJSONValue** definition)
	{
		HRESULT hr;
		TStackRef<IJSONObject> root;
		TStackRef<IJSONObject> parent;
		TStackRef<IJSONArray> children;
		TStackRef<IJSONArray> selected;
		GUID parentID;
		CheckIf(!definition,E_POINTER);
		*definition = NULL;
		CheckIf(!ids || !count,E_INVALIDARG);
		if(count == 1)
		{
			Check(CopySelectionDefinition(ids[0],definition));
			goto Cleanup;
		}
		Check(SelectionParent(this,ids,count,&parentID));
		Check(Serialize(&root));
		Check(FindDefinition(root,parentID,&parent));
		Check(DefinitionArray(parent,L"children",&children));
		Check(JSONCreateArray(&selected));
		// Copy in creation order, regardless of the order in which siblings were clicked.
		for(sysint n = 0; n < children->Count(); n++)
		{
			TStackRef<IJSONObject> child;
			TStackRef<IJSONValue> value;
			GUID id;
			Check(children->GetObject(n,&child));
			Check(child->FindNonNullValueW(L"id",&value));
			Check(value->GetStringAsGuid(id));
			for(UINT c = 0; c < count; c++)
			{
				if(IsEqualGUID(ids[c],id))
				{
					value.Release();
					Check(JSONWrapObject(child,&value));
					Check(selected->Add(value));
					break;
				}
			}
		}
		Check(JSONWrapArray(selected,definition));
	Cleanup:
		return hr;
	}

	static HRESULT CollectRemoved(IJSONObject* definition, TArray<GUID>& ids, UINT depth = 0)
	{
		HRESULT hr;
		TStackRef<IJSONValue> value;
		GUID id;
		CheckIf(depth > 128,E_INVALIDARG);
		Check(definition->FindNonNullValueW(L"id",&value));
		Check(value->GetStringAsGuid(id));
		Check(ids.Append(id));
		if(definition->HasField(RSTRING_CAST(L"children")))
		{
			TStackRef<IJSONArray> children;
			Check(DefinitionArray(definition,L"children",&children));
			for(sysint n = 0; n < children->Count(); n++)
			{
				TStackRef<IJSONObject> child;
				Check(children->GetObject(n,&child));
				Check(CollectRemoved(child,ids,depth + 1));
			}
		}
	Cleanup:
		return hr;
	}

	HRESULT CDesignerDocument::RemoveControls(const GUID* ids, UINT count)
	{
		HRESULT hr;
		TStackRef<IJSONObject> root;
		TArray<GUID> removed;
		GUID parentID;
		Check(SelectionParent(this,ids,count,&parentID));
		Check(Serialize(&root));
		for(UINT n = 0; n < count; n++)
		{
			TStackRef<IJSONObject> item;
			TStackRef<IJSONArray> siblings;
			sysint index = -1;
			Check(FindDefinition(root,ids[n],&item,&siblings,&index));
			CheckIf(!siblings || index < 0,E_INVALIDARG);
			Check(CollectRemoved(item,removed));
			Check(siblings->Remove(index,NULL));
		}
		for(sysint n = 0; n < removed.Length(); n++)
		{
			Check(ClearRemovedReferences(this,root,removed[n]));
		}
		Check(ApplyDefinition(root));
	Cleanup:
		return hr;
	}

	static HRESULT PropertyFields(IJSONObject* item, BOOL event, IJSONObject** fields)
	{
		HRESULT hr;
		TStackRef<IJSONValue> value;
		PCWSTR name = event ? L"events" : L"properties";
		if(item->HasField(RSTRING_CAST(name)))
		{
			Check(item->FindNonNullValueW(name,&value));
			Check(value->GetObject(fields));
		}
		else
		{
			Check(JSONCreateObject(fields));
			Check(JSONWrapObject(*fields,&value));
			Check(item->AddValueW(name,value));
		}
	Cleanup:
		return hr;
	}

	HRESULT CDesignerDocument::SetControlProperties(const GUID* ids, UINT count, RSTRING name, IJSONValue* value)
	{
		HRESULT hr;
		TStackRef<IJSONObject> root;
		BOOL changed = FALSE;
		BOOL activePage = FALSE;
		GUID parentID;
		CheckIf(!ids || !count || !name,E_INVALIDARG);
		activePage = EqualText(name,RSTRING_CAST(L"ActivePageIndex"));
		if(count > 1)
		{
			Check(SelectionParent(this,ids,count,&parentID));
			CheckIf(EqualText(name,RSTRING_CAST(L"Name")),E_ACCESSDENIED);
			Check(Serialize(&root));
		}
		for(UINT n = 0; n < count; n++)
		{
			TStackRef<IQuadooComponent> control;
			TStackRef<IQuadooProperties> properties;
			TStackRef<IQuadooPropertyDescriptors> descriptors;
			TStackRef<IQuadooPropertyDescriptor> descriptor;
			TStackRef<IJSONValue> previous;
			TStackRef<IJSONObject> item;
			TStackRef<IJSONObject> fields;
			QuadooProperty::Type type;
			UINT index;
			DWORD flags;
			Check(FindComponent(ids[n],&control));
			Check(Property(control,RStrToWide(name),&properties,&index));
			if(count == 1)
			{
				hr = SetProperty(control,index,value);
				goto Cleanup;
			}
			Check(properties->GetDescriptors(&descriptors));
			Check(descriptors->GetAt(index,&descriptor));
			Check(descriptor->GetFlags(&flags));
			CheckIf(flags & (QuadooForm::PropertyReadOnly | QuadooForm::PropertyRuntimeOnly),E_ACCESSDENIED);
			Check(descriptor->GetType(&type));
			Check(properties->GetValueJSON(index,&previous));
			changed |= JSONCompare(previous,value) != 0;
			Check(FindDefinition(root,ids[n],&item));
			Check(PropertyFields(item,type == QuadooProperty::Event,&fields));
			if(activePage && fields->HasField(name))
			{
				TStackRef<IJSONValue> saved;
				Check(fields->FindValue(name,&saved));
				changed |= JSONCompare(saved,value) != 0;
			}
			Check(fields->AddValue(name,value));
		}
		CheckIfIgnore(!changed,S_FALSE);
		// Editing the initial page explicitly also changes the visible design page.
		// Other edits retain the user's current page view, as single-control edits do.
		Check(ApplyDefinition(root,activePage ? ids : NULL,activePage ? count : 0));
	Cleanup:
		return hr;
	}

	HRESULT CDesignerDocument::MoveControls(const GUID* ids, UINT count, INT dx, INT dy)
	{
		HRESULT hr;
		TStackRef<IJSONObject> root;
		GUID parentID;
		Check(SelectionParent(this,ids,count,&parentID));
		CheckIfIgnore(!dx && !dy,S_FALSE);
		Check(Serialize(&root));
		for(UINT n = 0; n < count; n++)
		{
			TStackRef<IQuadooControl> control;
			TStackRef<IQuadooDesignCapabilities> capabilities;
			TStackRef<IJSONObject> item;
			TStackRef<IJSONObject> fields;
			QuadooForm::Rect bounds;
			DWORD flags = 0;
			Check(Find(ids[n],&control));
			Check(control->QueryInterface(__uuidof(IQuadooDesignCapabilities),reinterpret_cast<PVOID*>(&capabilities)));
			Check(capabilities->GetDesignCapabilities(&flags));
			CheckIf((dx && !(flags & QuadooForm::DesignMoveX)) || (dy && !(flags & QuadooForm::DesignMoveY)),E_ACCESSDENIED);
			Check(control->GetBounds(&bounds));
			CheckIf(static_cast<LONGLONG>(bounds.Left) + dx < INT_MIN || static_cast<LONGLONG>(bounds.Left) + dx > INT_MAX ||
				static_cast<LONGLONG>(bounds.Top) + dy < INT_MIN || static_cast<LONGLONG>(bounds.Top) + dy > INT_MAX,E_INVALIDARG);
			Check(FindDefinition(root,ids[n],&item));
			Check(PropertyFields(item,FALSE,&fields));
			Check(PutInt(fields,L"Left",bounds.Left + dx));
			Check(PutInt(fields,L"Top",bounds.Top + dy));
		}
		Check(ApplyDefinition(root));
	Cleanup:
		return hr;
	}

	HRESULT CDesignerDocument::ResizeControls(const GUID* ids, UINT count, INT widthDelta, INT heightDelta)
	{
		HRESULT hr;
		TArray<QuadooForm::Rect> resized;
		TStackRef<IJSONObject> root;
		GUID parentID;
		CheckIf(!ids || !count || count > 1000,E_INVALIDARG);
		CheckIfIgnore(!widthDelta && !heightDelta,S_FALSE);
		if(count > 1)
		{
			Check(SelectionParent(this,ids,count,&parentID));
		}
		// Validate the entire selection first. A sizing restriction or limit on
		// one sibling must not leave a partially resized group.
		for(UINT n = 0; n < count; n++)
		{
			TStackRef<IQuadooControl> control;
			TStackRef<IQuadooDesignCapabilities> capabilities;
			QuadooForm::Rect bounds;
			DWORD flags = 0;
			hr = Find(ids[n],&control);
			CheckIfIgnore(hr == E_NOINTERFACE || hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND),S_FALSE);
			Check(hr);
			hr = control->QueryInterface(__uuidof(IQuadooDesignCapabilities),reinterpret_cast<PVOID*>(&capabilities));
			CheckIfIgnore(hr == E_NOINTERFACE,S_FALSE);
			Check(hr);
			Check(capabilities->GetDesignCapabilities(&flags));
			CheckIfIgnore((widthDelta && !(flags & QuadooForm::DesignResizeWidth)) ||
				(heightDelta && !(flags & QuadooForm::DesignResizeHeight)),S_FALSE);
			Check(control->GetBounds(&bounds));
			INT minWidth = max(0,IntProperty(control,L"MinWidth"));
			INT minHeight = max(0,IntProperty(control,L"MinHeight"));
			INT maxWidth = IntProperty(control,L"MaxWidth");
			INT maxHeight = IntProperty(control,L"MaxHeight");
			LONGLONG width = bounds.Width;
			LONGLONG height = bounds.Height;
			// Constraints can clamp the displayed size while retaining a different
			// requested size. Step from that clamped size on the changed axis only.
			if(widthDelta)
			{
				width = max(minWidth,bounds.Width);
				if(maxWidth > 0)
				{
					width = min(width,static_cast<LONGLONG>(maxWidth));
				}
				width += widthDelta;
			}
			if(heightDelta)
			{
				height = max(minHeight,bounds.Height);
				if(maxHeight > 0)
				{
					height = min(height,static_cast<LONGLONG>(maxHeight));
				}
				height += heightDelta;
			}
			CheckIf(width > INT_MAX || height > INT_MAX,E_INVALIDARG);
			CheckIfIgnore(widthDelta && (width < minWidth || (maxWidth && width > maxWidth)),S_FALSE);
			CheckIfIgnore(heightDelta && (height < minHeight || (maxHeight && height > maxHeight)),S_FALSE);
			bounds.Width = static_cast<INT>(width);
			bounds.Height = static_cast<INT>(height);
			Check(resized.Append(bounds));
		}
		if(count == 1)
		{
			TStackRef<IQuadooControl> control;
			Check(Find(ids[0],&control));
			Check(SetBounds(control,resized.GetItemPtr(0)));
		}
		else
		{
			Check(Serialize(&root));
			for(UINT n = 0; n < count; n++)
			{
				TStackRef<IJSONObject> item;
				TStackRef<IJSONObject> fields;
				Check(FindDefinition(root,ids[n],&item));
				Check(PropertyFields(item,FALSE,&fields));
				if(widthDelta)
				{
					Check(PutInt(fields,L"Width",resized[n].Width));
				}
				if(heightDelta)
				{
					Check(PutInt(fields,L"Height",resized[n].Height));
				}
			}
			Check(ApplyDefinition(root));
		}
	Cleanup:
		return hr;
	}

	HRESULT CDesignerDocument::PrepareRemove(REFGUID id, ISourceEdit** edit)
	{
		HRESULT hr = S_OK;
		TStackRef<IJSONObject> root, item;
		TStackRef<IJSONArray> siblings;
		sysint index = -1;
		*edit = NULL;
		if(!m_sourceSite) goto Cleanup;
		Check(Serialize(&root));
		Check(FindDefinition(root,id,&item,&siblings,&index));
		CheckIf(!siblings || index < 0,E_INVALIDARG);
		Check(siblings->Remove(index,NULL));
		Check(ClearRemovedReferences(this,root,id));
		Check(m_sourceSite->PrepareDefinition(root,edit));
	Cleanup:
		return hr;
	}

	HRESULT CDesignerDocument::Reorder(REFGUID id, BOOL front)
	{
		HRESULT hr;
		TStackRef<IQuadooComponent> control;
		TStackRef<IJSONObject> root, definition;
		TStackRef<IJSONArray> siblings;
		TStackRef<IJSONValue> value;
		sysint index = -1;
		Check(FindComponent(id,&control));
		CheckIf(SameObject(control,m_root),E_ACCESSDENIED);
		{
			TStackRef<IQuadooMenuItem> item;
			if(SUCCEEDED(control->QueryInterface(__uuidof(IQuadooMenuItem),reinterpret_cast<PVOID*>(&item))))
			{
				Check(MoveMenuItem(control,front,TRUE));
				goto Cleanup;
			}
		}
		Check(Serialize(&root));
		Check(FindDefinition(root,id,&definition,&siblings,&index));
		CheckIf(!siblings || index < 0,E_UNEXPECTED);
		CheckIfIgnore(index == (front ? siblings->Count() - 1 : 0),S_FALSE);
		Check(siblings->Remove(index,&value));
		Check(siblings->Insert(front ? siblings->Count() : 0,value));
		Check(RestorePages(root));
		Check(ApplyDefinition(root));
	Cleanup:
		return hr;
	}

	struct PasteIdentity { RSTRING oldName, name, oldID, id; };
	class CPasteDefinitions
	{
		CDesignerDocument* m_document;
		TArray<RSTRING> m_names;
		TArray<PasteIdentity> m_identities;
		UINT m_count;
	public:
		CPasteDefinitions(CDesignerDocument* document) : m_document(document), m_count(0) {}
		~CPasteDefinitions()
		{
			for(sysint n = 0; n < m_names.Length(); n++) RStrRelease(m_names[n]);
			for(sysint n = 0; n < m_identities.Length(); n++)
			{ RStrRelease(m_identities[n].oldName); RStrRelease(m_identities[n].name); RStrRelease(m_identities[n].oldID); RStrRelease(m_identities[n].id); }
		}
		BOOL HasName(RSTRING name)
		{
			for(sysint n = 0; n < m_names.Length(); n++) if(EqualText(m_names[n],name)) return TRUE;
			return FALSE;
		}
		HRESULT VisitChildren(IJSONObject* object, BOOL rename, UINT depth)
		{
			HRESULT hr = S_OK;
			static PCWSTR const fields[] = { L"children",L"components" };
			for(UINT f = 0; f < ARRAYSIZE(fields); f++) if(object->HasField(RSTRING_CAST(fields[f])))
			{
				TStackRef<IJSONArray> list;
				Check(DefinitionArray(object,fields[f],&list));
				CheckIf(list->Count() > 10000,E_INVALIDARG);
				for(sysint n = 0; n < list->Count(); n++)
				{
					TStackRef<IJSONObject> child;
					Check(list->GetObject(n,&child));
					Check(Collect(child,rename,depth + 1));
				}
			}
	Cleanup:
			return hr;
		}
		HRESULT Collect(IJSONObject* object, BOOL rename, UINT depth = 0)
		{
			HRESULT hr;
			CRString oldName, name, oldID, id, type;
			PasteIdentity entry;
			TStackRef<IJSONValue> value;
			CheckIf(depth > 128 || ++m_count > 20000,E_INVALIDARG);
			if(object->HasField(RSTRING_CAST(L"name")))
			{
				Check(object->FindNonNullValueW(L"name",&value));
				Check(value->GetString(&oldName));
				value.Release();
			}
			name = oldName;
			if(rename)
			{
				GUID guid;
				WCHAR text[40];
				if(object->HasField(RSTRING_CAST(L"id")))
				{
					Check(object->FindNonNullValueW(L"id",&value));
					Check(value->GetStringAsGuid(guid));
					Check(value->GetString(&oldID));
					value.Release();
				}
				for(sysint n = 0; n < m_identities.Length(); n++)
				{
					// Ambiguous source identities cannot be remapped safely.
					CheckIf((RStrLen(oldName) && EqualText(oldName,m_identities[n].oldName)) ||
						(RStrLen(oldID) && EqualText(oldID,m_identities[n].oldID,TRUE)),E_INVALIDARG);
				}
				Check(object->FindNonNullValueW(L"type",&value));
				Check(value->GetString(&type));
				value.Release();
				if(!RStrLen(name) || HasName(name))
				{
					CRString base;
					UINT length = RStrLen(oldName);
					while(length && oldName.c_wstr()[length - 1] >= L'0' && oldName.c_wstr()[length - 1] <= L'9') length--;
					if(length)
					{
						Check(RStrCreateW(length,oldName.c_wstr(),&base));
					}
					else base = type;
					for(UINT suffix = 1; suffix < 100000; suffix++)
					{
						name = static_cast<RSTRING>(NULL);
						Check(RStrFormatW(&name,L"%ls%u",base.c_wstr(),suffix));
						if(!HasName(name)) break;
						CheckIf(suffix == 99999,E_FAIL);
					}
				}
				Check(CoCreateGuid(&guid));
				CheckIf(!StringFromGUID2(guid,text,ARRAYSIZE(text)),E_FAIL);
				Check(RStrCreateW(TStrLenAssert(text),text,&id));
				Check(PutString(object,L"name",name));
				Check(PutString(object,L"id",id));
				entry.oldName = oldName; entry.name = name; entry.oldID = oldID; entry.id = id;
				Check(m_identities.Append(entry));
				RStrAddRef(oldName); RStrAddRef(name); RStrAddRef(oldID); RStrAddRef(id);
			}
			Check(m_names.Append(name));
			RStrAddRef(name);
			Check(VisitChildren(object,rename,depth));
	Cleanup:
			return hr;
		}
		HRESULT Descriptors(RSTRING type, IQuadooPropertyDescriptors** descriptors, BOOL* visual, IQuadooControlFactory** controlFactory = NULL)
		{
			HRESULT hr;
			TStackRef<IQuadooControlFactory> factory;
			TStackRef<IQuadooComponentRegistry> registry;
			TStackRef<IQuadooComponentFactory> component;
			*visual = FALSE;
			hr = m_document->Registry()->FindFactory(type,&factory);
			if(hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND))
			{
				Check(m_document->Registry()->QueryInterface(__uuidof(IQuadooComponentRegistry),reinterpret_cast<PVOID*>(&registry)));
				Check(registry->FindComponentFactory(type,&component));
				Check(component->GetProperties(descriptors));
			}
			else
			{
				Check(hr);
				*visual = TRUE;
				Check(factory->GetProperties(descriptors));
				if(controlFactory) *controlFactory = factory.Detach();
			}
	Cleanup:
			return hr;
		}
		HRESULT RemapValue(IJSONValue* input, IJSONValue** output, UINT depth = 0)
		{
			HRESULT hr;
			CheckIf(depth > 128,E_INVALIDARG);
			if(input && input->GetType() == JSON::String)
			{
				CRString text;
				Check(input->GetString(&text));
				for(sysint n = 0; n < m_identities.Length(); n++)
				{
					PasteIdentity& entry = m_identities[n];
					if(RStrLen(entry.oldName) && text == entry.oldName)
					{
						Check(JSONCreateString(entry.name,output));
						goto Cleanup;
					}
					if(RStrLen(entry.oldID) && EqualText(text,entry.oldID,TRUE))
					{
						Check(JSONCreateString(entry.id,output));
						goto Cleanup;
					}
				}
			}
			else if(input && (input->GetType() == JSON::Object || input->GetType() == JSON::Array))
			{
				TStackRef<IJSONObject> object;
				TStackRef<IJSONArray> array;
				if(input->GetType() == JSON::Object)
				{
					Check(input->GetObject(&object));
				}
				else
				{
					Check(input->GetArray(&array));
				}
				sysint count = object ? object->Count() : array->Count();
				for(sysint n = 0; n < count; n++)
				{
					TStackRef<IJSONValue> child, mapped;
					if(object)
					{
						Check(object->GetValueByIndex(n,&child));
					}
					else
					{
						Check(array->GetValue(n,&child));
					}
					Check(RemapValue(child,&mapped,depth + 1));
					if(object)
					{
						Check(object->SetValueByIndex(n,mapped));
					}
					else
					{
						Check(array->Replace(n,mapped));
					}
				}
			}
			Check(JSONClone(input,output,TRUE));
	Cleanup:
			return hr;
		}
		HRESULT Remap(IJSONObject* object, UINT depth = 0)
		{
			HRESULT hr;
			TStackRef<IJSONValue> value;
			TStackRef<IJSONObject> properties;
			TStackRef<IQuadooPropertyDescriptors> descriptors;
			CRString type;
			BOOL visual;
			UINT count;
			CheckIf(depth > 128,E_INVALIDARG);
			Check(object->FindNonNullValueW(L"type",&value));
			Check(value->GetString(&type));
			value.Release();
			Check(Descriptors(type,&descriptors,&visual));
			if(object->HasField(RSTRING_CAST(L"menuParent")))
			{
				TStackRef<IJSONValue> original;
				TStackRef<IJSONValue> mapped;
				Check(object->FindNonNullValueW(L"menuParent",&original));
				Check(RemapValue(original,&mapped));
				Check(object->AddValueW(L"menuParent",mapped));
			}
			if(object->HasField(RSTRING_CAST(L"properties")))
			{
				Check(object->FindNonNullValueW(L"properties",&value));
				Check(value->GetObject(&properties));
				value.Release();
				properties->RemoveValueW(L"Name");
				if(type == RSTRING_CAST(L"ListView") || type == RSTRING_CAST(L"StatusBar"))
				{
					static PCWSTR const collections[] = {L"Columns",L"Items",L"Panels"};
					for(UINT collection = 0; collection < ARRAYSIZE(collections); collection++)
					{
						if(properties->HasField(RSTRING_CAST(collections[collection])))
						{
							TStackRef<IJSONValue> stored;
							TStackRef<IJSONArray> entries;
							Check(properties->FindNonNullValueW(collections[collection],&stored));
							Check(stored->GetArray(&entries));
							for(sysint entry = 0; entry < entries->Count(); entry++)
							{
								TStackRef<IJSONObject> item;
								GUID id;
								WCHAR text[40];
								Check(entries->GetObject(entry,&item));
								Check(CoCreateGuid(&id));
								INT length = StringFromGUID2(id,text,ARRAYSIZE(text));
								CheckIf(!length,E_FAIL);
								Check(JSONCreateStringW(text,length - 1,&value));
								Check(item->AddValueW(L"id",value));
								value.Release();
							}
						}
					}
				}
				Check(descriptors->GetCount(&count));
				CheckIf(count > 4096,E_INVALIDARG);
				for(UINT n = 0; n < count; n++)
				{
					TStackRef<IQuadooPropertyDescriptor> descriptor;
					QuadooProperty::Type propertyType;
					CRString name;
					Check(descriptors->GetAt(n,&descriptor));
					Check(descriptor->GetType(&propertyType));
					if(propertyType != QuadooProperty::ObjectReference) continue;
					Check(descriptor->GetName(&name));
					if(properties->HasField(name))
					{
						TStackRef<IJSONValue> original, mapped;
						Check(properties->FindValue(name,&original));
						Check(RemapValue(original,&mapped));
						Check(properties->AddValue(name,mapped));
					}
				}
			}
			static PCWSTR const fields[] = { L"children",L"components" };
			for(UINT f = 0; f < ARRAYSIZE(fields); f++) if(object->HasField(RSTRING_CAST(fields[f])))
			{
				TStackRef<IJSONArray> children;
				Check(DefinitionArray(object,fields[f],&children));
				for(sysint n = 0; n < children->Count(); n++)
				{
					TStackRef<IJSONObject> child;
					Check(children->GetObject(n,&child));
					Check(Remap(child,depth + 1));
				}
			}
	Cleanup:
			return hr;
		}
	};

	HRESULT CDesignerDocument::PasteDefinition(REFGUID selected, IJSONValue* definition, GUID* first, TArray<GUID>* added)
	{
		HRESULT hr;
		CPasteDefinitions paste(this);
		TStackRef<IJSONValue> clone;
		TStackRef<IJSONArray> items;
		TStackRef<IJSONObject> root;
		CheckIf(!definition || !first,E_INVALIDARG);
		*first = GUID_NULL;
		if(added)
		{
			added->Clear();
		}
		Check(JSONClone(definition,&clone,TRUE));
		if(clone->GetType() == JSON::Array)
		{
			Check(clone->GetArray(&items));
		}
		else
		{
			CheckIf(clone->GetType() != JSON::Object,E_INVALIDARG);
			Check(JSONCreateArray(&items));
			Check(items->Add(clone));
		}
		CheckIf(items->Count() < 1 || items->Count() > 1000,E_INVALIDARG);
		Check(Serialize(&root));
		Check(paste.Collect(root,FALSE));
		for(sysint n = 0; n < items->Count(); n++)
		{
			TStackRef<IJSONObject> item;
			Check(items->GetObject(n,&item));
			Check(paste.Collect(item,TRUE));
		}
		for(sysint n = 0; n < items->Count(); n++)
		{
			TStackRef<IJSONObject> item, target, properties;
			TStackRef<IJSONValue> value, wrapped;
			TStackRef<IQuadooPropertyDescriptors> descriptors;
			TStackRef<IQuadooPropertyDescriptor> tabOrder;
			TStackRef<IQuadooControlFactory> factory;
			TStackRef<IQuadooDesignFactory> placement;
			TStackRef<IQuadooControl> current;
			TStackRef<IQuadooControlContainer> parent;
			TStackRef<IJSONArray> children;
			CRString type;
			BOOL visual;
			GUID parentID, id;
			Check(items->GetObject(n,&item));
			Check(paste.Remap(item));
			Check(item->FindNonNullValueW(L"id",&value));
			Check(value->GetStringAsGuid(id));
			value.Release();
			Check(item->FindNonNullValueW(L"type",&value));
			Check(value->GetString(&type));
			value.Release();
			Check(paste.Descriptors(type,&descriptors,&visual,&factory));
			if(!visual && item->HasField(RSTRING_CAST(L"menuParent")))
			{
				TStackRef<IJSONValue> reference;
				Check(item->FindNonNullValueW(L"menuParent",&reference));
				GUID menuParent;
				Check(reference->GetStringAsGuid(menuParent));
				BOOL internal = FALSE;
				for(sysint k = 0; k < items->Count(); k++)
				{
					TStackRef<IJSONObject> other;
					TStackRef<IJSONValue> key;
					GUID otherID;
					Check(items->GetObject(k,&other));
					Check(other->FindNonNullValueW(L"id",&key));
					Check(key->GetStringAsGuid(otherID));
					if(IsEqualGUID(otherID,menuParent))
					{
						internal = TRUE;
						break;
					}
				}
				if(!internal)
				{
					TStackRef<IQuadooComponent> selectedMenu;
					TStackRef<IQuadooMenuItems> menuItems;
					if(SUCCEEDED(FindComponent(selected,&selectedMenu)) && SUCCEEDED(selectedMenu->QueryInterface(__uuidof(IQuadooMenuItems),reinterpret_cast<PVOID*>(&menuItems))))
					{
						WCHAR text[40];
						Check(selectedMenu->GetID(&menuParent));
						INT length = StringFromGUID2(menuParent,text,ARRAYSIZE(text));
						CheckIf(!length,E_FAIL);
						CRString identity;
						Check(RStrCreateW(length - 1,text,&identity));
						Check(PutString(item,L"menuParent",identity));
					}
				}
			}
			if(visual)
			{
				hr = Find(selected,&current);
				if(hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND)) current = m_root;
				else
				{
					Check(hr);
				}
				hr = factory->QueryInterface(__uuidof(IQuadooDesignFactory),reinterpret_cast<PVOID*>(&placement));
				if(hr != E_NOINTERFACE)
				{
					Check(hr);
				}
				// A selected page control targets its visible page for ordinary controls.
				{
					TStackRef<IQuadooTabControl> tabs;
					TStackRef<IQuadooTabPage> active;

					if(type != RSTRING_CAST(L"TabPage") && SUCCEEDED(current->QueryInterface(__uuidof(IQuadooTabControl),reinterpret_cast<PVOID*>(&tabs))))
					{
						Check(tabs->GetActivePage(&active));
						if(active) current = active;
					}
				}
				for(UINT depth = 0; current && depth < 128; depth++)
				{
					BOOL allowed = TRUE;
					parent.Release();
					hr = current->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&parent));
					if(hr != E_NOINTERFACE)
					{
						Check(hr);
						if(placement)
						{
							Check(placement->CanCreate(parent,&allowed));
						}
						if(allowed) break;
					}
					TStackRef<IQuadooControlContainer> next;
					Check(current->GetParent(&next));
					current = next;
					parent.Release();
				}
				CheckIf(!parent,E_INVALIDARG);
				Check(parent->GetID(&parentID));
				Check(FindDefinition(root,parentID,&target));
				Check(DefinitionArray(target,L"children",&children));
				hr = descriptors->Find(RSTRING_CAST(L"TabOrder"),&tabOrder);
				if(hr != DISP_E_MEMBERNOTFOUND)
				{
					INT next = 0;
					Check(hr);
					for(sysint c = 0; c < children->Count(); c++)
					{
						TStackRef<IJSONObject> child, values;
						TStackRef<IJSONValue> field;
						Check(children->GetObject(c,&child));
						if(child->HasField(RSTRING_CAST(L"properties")))
						{
							Check(child->FindNonNullValueW(L"properties",&field));
							Check(field->GetObject(&values));
							field.Release();
							if(values->HasField(RSTRING_CAST(L"TabOrder")))
							{
								INT order;
								Check(values->FindNonNullValueW(L"TabOrder",&field));
								Check(field->GetInteger(&order));
								CheckIf(order == INT_MAX,E_INVALIDARG);
								next = max(next,order + 1);
							}
						}
					}
					if(item->HasField(RSTRING_CAST(L"properties")))
					{
						Check(item->FindNonNullValueW(L"properties",&value));
						Check(value->GetObject(&properties));
						value.Release();
					}
					else
					{
						Check(JSONCreateObject(&properties));
						Check(JSONWrapObject(properties,&value));
						Check(item->AddValueW(L"properties",value));
						value.Release();
					}
					Check(PutInt(properties,L"TabOrder",next));
				}
			}
			else
			{
				Check(DefinitionArray(root,L"components",&children));
				if(type == RSTRING_CAST(L"MainMenu"))
				{
					// A copied main menu becomes the assigned menu in its new form.
					TStackRef<IJSONObject> values;
					TStackRef<IJSONValue> identity;
					Check(root->FindNonNullValueW(L"properties",&value));
					Check(value->GetObject(&values));
					value.Release();
					Check(item->FindNonNullValueW(L"id",&identity));
					Check(values->AddValueW(L"MainMenu",identity));
				}
			}
			Check(JSONWrapObject(item,&wrapped));
			Check(children->Add(wrapped));
			if(added && visual)
			{
				Check(added->Append(id));
			}
			if(!n)
				*first = id;
		}
		Check(ApplyDefinition(root));

	Cleanup:
		if(FAILED(hr) && first)
		{
			*first = GUID_NULL;
			if(added)
			{
				added->Clear();
			}
		}
		return hr;
	}
}
