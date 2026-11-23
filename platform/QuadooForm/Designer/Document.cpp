#include "Designer.h"
#include "Library\Util\TextHelpers.h"

namespace QFD
{
	BOOL SameObject(IUnknown* first, IUnknown* second)
	{
		TStackRef<IUnknown> a, b;
		return first && second && SUCCEEDED(first->QueryInterface(IID_IUnknown,reinterpret_cast<PVOID*>(&a))) && SUCCEEDED(second->QueryInterface(IID_IUnknown,reinterpret_cast<PVOID*>(&b))) && a == static_cast<IUnknown*>(b);
	}

	HRESULT Property(IQuadooComponent* control, PCWSTR name, IQuadooProperties** values, UINT* index)
	{
		HRESULT hr;
		TStackRef<IQuadooProperties> properties; TStackRef<IQuadooPropertyDescriptors> descriptors; UINT count;
		CheckIf(!values || !index,E_POINTER); *values = NULL; CheckIf(!control || !name,E_INVALIDARG);
		Check(control->GetProperties(&properties)); Check(properties->GetDescriptors(&descriptors)); Check(descriptors->GetCount(&count)); CheckIf(count > 4096,E_INVALIDARG);
		for(UINT n = 0; n < count; n++)
		{
			TStackRef<IQuadooPropertyDescriptor> descriptor; CRString text;
			Check(descriptors->GetAt(n,&descriptor)); Check(descriptor->GetName(&text));
			if(text == RSTRING_CAST(name)) { *index = n; *values = properties.Detach(); hr = S_OK; goto Cleanup; }
		}
		hr = DISP_E_MEMBERNOTFOUND;
	Cleanup:
		return hr;
	}

	HRESULT ReadProperty(IQuadooComponent* control, PCWSTR name, IJSONValue** value)
	{
		HRESULT hr; TStackRef<IQuadooProperties> properties; UINT index;
		CheckIf(!value,E_POINTER); *value = NULL; Check(Property(control,name,&properties,&index)); Check(properties->GetValueJSON(index,value));
	Cleanup:
		return hr;
	}

	INT IntProperty(IQuadooComponent* control, PCWSTR name, INT fallback)
	{
		TStackRef<IJSONValue> value; INT number = fallback;
		if(SUCCEEDED(ReadProperty(control,name,&value)) && value) value->GetInteger(&number);
		return number;
	}

	BOOL BoolProperty(IQuadooComponent* control, PCWSTR name, BOOL fallback)
	{
		TStackRef<IJSONValue> value; bool flag = fallback != FALSE;
		if(SUCCEEDED(ReadProperty(control,name,&value)) && value) value->GetBoolean(&flag);
		return flag;
	}

	HRESULT StringProperty(IQuadooComponent* control, PCWSTR name, RSTRING* text)
	{
		HRESULT hr; TStackRef<IJSONValue> value;
		CheckIf(!text,E_POINTER); *text = NULL; Check(ReadProperty(control,name,&value)); CheckIf(!value,E_INVALIDARG); Check(value->GetString(text));
	Cleanup:
		return hr;
	}

	HRESULT PutString(IJSONObject* object, PCWSTR name, RSTRING text)
	{
		HRESULT hr; TStackRef<IJSONValue> value;
		Check(JSONCreateString(text,&value)); Check(object->AddValueW(name,value));
	Cleanup:
		return hr;
	}

	HRESULT PutInt(IJSONObject* object, PCWSTR name, INT number)
	{
		HRESULT hr; TStackRef<IJSONValue> value;
		Check(JSONCreateInteger(number,&value)); Check(object->AddValueW(name,value));
	Cleanup:
		return hr;
	}

	HRESULT Parse (PCWSTR text, INT length, __deref_out IJSONObject** object)
	{
		HRESULT hr;
		TStackRef<IJSONValue> value;

		CheckIf(!object,E_POINTER);
		*object = NULL;
		CheckIf(!text || length < 0 || length > 64 * 1024 * 1024,E_INVALIDARG);
		Check(JSONParse(NULL,text,length,&value));
		CheckIf(!value,E_INVALIDARG);
		Check(value->GetObject(object));

	Cleanup:
		return hr;
	}

	static HRESULT FindID(IQuadooControl* root, REFGUID id, IQuadooControl** result, UINT depth)
	{
		HRESULT hr; GUID current; TStackRef<IQuadooControlContainer> container;
		CheckIf(depth > 128,E_INVALIDARG); Check(root->GetID(&current));
		if(IsEqualGUID(current,id)) { *result = root; root->AddRef(); hr = S_OK; goto Cleanup; }
		hr = root->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&container));
		if(hr == E_NOINTERFACE) { hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND); goto Cleanup; } Check(hr);
		UINT count; Check(container->GetChildCount(&count));
		for(UINT n = 0; n < count; n++)
		{
			TStackRef<IQuadooControl> child; Check(container->GetChild(n,&child)); hr = FindID(child,id,result,depth + 1);
			if(hr != HRESULT_FROM_WIN32(ERROR_NOT_FOUND)) goto Cleanup;
		}
		hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
	Cleanup:
		return hr;
	}

	CDesignerDocument::CDesignerDocument() : m_registry(NULL), m_context(NULL), m_root(NULL), m_usage(NULL), m_sourceSite(NULL), m_host(NULL), m_dirty(FALSE), m_form(FALSE) {}
	CDesignerDocument::~CDesignerDocument() { Clear(); SafeRelease(m_registry); }

	HRESULT CDesignerDocument::Initialize (IQuadooControlRegistry* registry)
	{
		HRESULT hr;

		CheckIf(m_registry,QuadooForm::EInvalidState);
		if(registry)
		{
			SetInterface(m_registry,registry);
			hr = S_OK;
		}
		else
		{
			Check(QuadooGetControlRegistry(&m_registry));
		}

	Cleanup:
		return hr;
	}

	VOID CDesignerDocument::Clear ()
	{
		if(m_root)
		{
			m_root->Destroy();
			SafeRelease(m_root);
		}
		SafeRelease(m_context);
		SafeRelease(m_usage);
		if(m_host)
		{
			DestroyWindow(m_host);
			m_host = NULL;
		}
		m_path = static_cast<RSTRING>(NULL);
		m_pages.Clear();
		m_dirty = FALSE;
	}

	HRESULT CDesignerDocument::CapturePages (IQuadooControl* control)
	{
		HRESULT hr = S_OK;
		TStackRef<IQuadooTabControl> tabs; TStackRef<IQuadooControlContainer> container;
		if(SUCCEEDED(control->QueryInterface(__uuidof(IQuadooTabControl),reinterpret_cast<PVOID*>(&tabs)))) { Check(RememberPage(tabs)); }
		if(SUCCEEDED(control->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&container))))
		{
			UINT count; Check(container->GetChildCount(&count));
			for(UINT n = 0; n < count; n++) { TStackRef<IQuadooControl> child; Check(container->GetChild(n,&child)); Check(CapturePages(child)); }
		}
	Cleanup:
		return hr;
	}

	HRESULT CDesignerDocument::RememberPage(IQuadooTabControl* tabs)
	{
		HRESULT hr;
		PageView view; TStackRef<IQuadooTabPage> active;
		Check(tabs->GetID(&view.tabs)); Check(tabs->GetActivePage(&active)); view.active = GUID_NULL;
		if(active) { Check(active->GetID(&view.active)); }
		for(sysint n = 0; n < m_pages.Length(); n++) if(IsEqualGUID(m_pages[n].tabs,view.tabs)) { m_pages[n] = view; hr = S_OK; goto Cleanup; }
		Check(m_pages.Append(view));
	Cleanup:
		return hr;
	}

	HRESULT CDesignerDocument::Load (HWND workspace, IJSONObject* definition, RSTRING path)
	{
		HRESULT hr;
		CDesignerDocument candidate; TStackRef<IQuadooForm> form; TStackRef<IQuadooFrame> frame;
		QuadooForm::ContextOptions options; HWND window;
		CheckIf(!workspace || !definition || !m_registry,E_INVALIDARG); Check(candidate.Initialize(m_registry));
		{ TStackRef<IQuadooDesignUsageFactory> usage; hr=m_registry->QueryInterface(__uuidof(IQuadooDesignUsageFactory),reinterpret_cast<PVOID*>(&usage));
		  if(hr!=E_NOINTERFACE) { Check(hr); Check(usage->CreateUsage(definition,&candidate.m_usage)); } }
		candidate.m_host = CreateWindowExW(WS_EX_CONTROLPARENT,L"STATIC",L"",WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,0,0,1,1,workspace,NULL,GetModuleHandle(NULL),NULL); CheckIf(!candidate.m_host,HrEnsureLastError());
		ZeroMemory(&options,sizeof(options)); options.cbSize = sizeof(options); options.Mode = QuadooForm::Design; options.DPI = 96; options.hwndHost = candidate.m_host; options.pRegistry = m_registry;
		Check(QuadooCreateConstructionContext(&options,&candidate.m_context)); Check(QuadooCreateControl(candidate.m_context,NULL,definition,&candidate.m_root));
		candidate.m_form = SUCCEEDED(candidate.m_root->QueryInterface(__uuidof(IQuadooForm),reinterpret_cast<PVOID*>(&form)));
		if(!candidate.m_form) { Check(candidate.m_root->QueryInterface(__uuidof(IQuadooFrame),reinterpret_cast<PVOID*>(&frame))); }
		Check(candidate.CapturePages(candidate.m_root)); candidate.m_path = path;
		{ TStackRef<IQuadooWinControl> native; Check(candidate.m_root->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&native))); Check(native->GetHandle(&window)); }
		// Local design visibility does not edit the persisted root Visible property.
		ShowWindow(window,SW_SHOWNOACTIVATE);
		Clear(); SwapData(m_usage,candidate.m_usage); SwapData(m_root,candidate.m_root); SwapData(m_context,candidate.m_context); SwapData(m_host,candidate.m_host);
		m_pages.Swap(candidate.m_pages); m_form = candidate.m_form; m_path = candidate.m_path; m_dirty = FALSE;
	Cleanup:
		return hr;
	}

	HRESULT CDesignerDocument::New (HWND workspace, BOOL form)
	{
		HRESULT hr;
		TStackRef<IJSONObject> definition, properties;
		TStackRef<IJSONValue> value;

		Check(JSONCreateObject(&definition));
		Check(JSONCreateObject(&properties));
		Check(PutString(properties,L"Caption",RSTRING_CAST(form ? L"Form1" : L"Frame1")));
		Check(JSONWrapObject(properties,&value));
		Check(definition->AddValueW(L"properties",value));
		Check(PutString(definition,L"type",RSTRING_CAST(form ? L"Form" : L"Frame")));
		Check(PutString(definition,L"name",RSTRING_CAST(form ? L"Form1" : L"Frame1")));
		Check(Load(workspace,definition));

	Cleanup:
		return hr;
	}

	HRESULT CDesignerDocument::Open (HWND workspace, PCWSTR path)
	{
		HRESULT hr;
		HANDLE file = INVALID_HANDLE_VALUE; LARGE_INTEGER size; PWSTR text = NULL; INT length;
		TStackRef<IJSONObject> definition; CRString filename;

		CheckIf(!path,E_INVALIDARG);
		Check(RStrCreateW(TStrLenAssert(path),path,&filename));
		file = CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
		CheckIf(file == INVALID_HANDLE_VALUE,HrEnsureLastError());
		CheckIf(!GetFileSizeEx(file,&size),HrEnsureLastError());
		CheckIf(size.QuadPart > 64 * 1024 * 1024,E_INVALIDARG);
		Check(Text::LoadFromHandle(file,&text,&length,CP_UTF8));
		Check(Parse(text,length,&definition));
		Check(Load(workspace,definition,filename));

	Cleanup:
		if(file != INVALID_HANDLE_VALUE)
			CloseHandle(file);
		SafeDeleteArray(text);
		return hr;
	}

	HRESULT CDesignerDocument::RestorePages (IJSONObject* definition)
	{
		HRESULT hr;
		TStackRef<IJSONValue> value;
		TStackRef<IJSONArray> children;
		GUID id;

		Check(definition->FindNonNullValueW(L"id",&value));
		Check(value->GetStringAsGuid(id));
		value.Release();

		if(definition->HasField(RSTRING_CAST(L"children")))
		{
			Check(definition->FindNonNullValueW(L"children",&value));
			Check(value->GetArray(&children));
			value.Release();
		}
		else
		{
			Check(JSONCreateArray(&children));
		}

		for(sysint p = 0; p < m_pages.Length(); p++)
		{
			if(IsEqualGUID(id,m_pages[p].tabs))
			{
				INT selected = -1;
				for(sysint n = 0; n < children->Count(); n++)
				{
					TStackRef<IJSONObject> child; GUID childID;

					Check(children->GetObject(n,&child));
					Check(child->FindNonNullValueW(L"id",&value));
					Check(value->GetStringAsGuid(childID));
					value.Release();

					if(IsEqualGUID(childID,m_pages[p].active))
						selected = static_cast<INT>(n);
				}

				TStackRef<IJSONObject> properties;
				Check(definition->FindNonNullValueW(L"properties",&value));
				Check(value->GetObject(&properties));
				value.Release();
				
				Check(PutInt(properties,L"ActivePageIndex",selected));
				break;
			}
		}
		for(sysint n = 0; n < children->Count(); n++)
		{
			TStackRef<IJSONObject> child;
			Check(children->GetObject(n,&child));
			Check(RestorePages(child));
		}

	Cleanup:
		return hr;
	}

	HRESULT CDesignerDocument::Serialize(IJSONObject** definition)
	{
		HRESULT hr; TStackRef<IQuadooFormSerializer> serializer;
		CheckIf(!definition,E_POINTER); *definition = NULL; CheckIf(!m_root,QuadooForm::EInvalidState);
		Check(QuadooGetFormSerializer(&serializer)); Check(serializer->Serialize(m_root,definition)); Check(RestorePages(*definition));
		if(m_usage) { Check(m_usage->Update(*definition)); }
	Cleanup:
		if(FAILED(hr) && definition) SafeRelease(*definition);
		return hr;
	}

	HRESULT CDesignerDocument::Save(PCWSTR path)
	{
		HRESULT hr;
		TStackRef<IJSONObject> definition; CMemoryStream text; CRString filename;
		WCHAR full[MAX_PATH], temporary[MAX_PATH], directory[MAX_PATH]; PWSTR part;
		temporary[0] = 0;
		CheckIf(!path || !*path,E_INVALIDARG); Check(RStrCreateW(TStrLenAssert(path),path,&filename)); Check(Serialize(&definition)); Check(JSONSerializeObject(definition,&text));
		{
			INT encoded = WideCharToMultiByte(CP_UTF8,0,text.TGetReadPtr<WCHAR>(),text.DataRemaining() / sizeof(WCHAR),NULL,0,NULL,NULL);
			CheckIf(!encoded,HrEnsureLastError()); CheckIf(encoded > 64 * 1024 * 1024 - 3,E_INVALIDARG);
		}
		DWORD count = GetFullPathNameW(path,ARRAYSIZE(full),full,&part); CheckIf(!count || count >= ARRAYSIZE(full) || !part,E_INVALIDARG);
		Check(TStrCchCpy(directory,ARRAYSIZE(directory),full)); directory[part - full] = 0;
		CheckIf(!GetTempFileNameW(directory,L"QFD",0,temporary),HrEnsureLastError());
		Check(Text::SaveToFile(text.TGetReadPtr<WCHAR>(),text.DataRemaining() / sizeof(WCHAR),temporary));
		CheckIf(!MoveFileExW(temporary,full,MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH),HrEnsureLastError()); temporary[0] = 0;
		m_path = filename; m_dirty = FALSE;
	Cleanup:
		if(temporary[0]) DeleteFileW(temporary);
		return hr;
	}

	HRESULT CDesignerDocument::Find(REFGUID id, IQuadooControl** control)
	{
		HRESULT hr;
		CheckIf(!control,E_POINTER); *control = NULL; CheckIf(!m_root,QuadooForm::EInvalidState); Check(FindID(m_root,id,control,0));
	Cleanup:
		return hr;
	}

	HRESULT CDesignerDocument::Add(IQuadooControlContainer* parent, RSTRING type, const QuadooForm::Rect* bounds, IQuadooControl** control)
	{
		HRESULT hr;
		TStackRef<IQuadooControlFactory> factory; TStackRef<IQuadooDesignFactory> placement;
		TStackRef<IQuadooControlContainer> names; TStackRef<IJSONObject> definition, properties; TStackRef<IJSONValue> wrapped;
		CRString name; BOOL allowed = TRUE;
		TStackRef<ISourceEdit> sourceEdit;
		CheckIf(!control,E_POINTER);
		*control = NULL;
		CheckIf(!parent || !type || !bounds,E_INVALIDARG);
		if(m_usage) { Check(m_usage->Include(type)); }
		Check(m_registry->FindFactory(type,&factory));
		hr = factory->QueryInterface(__uuidof(IQuadooDesignFactory),reinterpret_cast<PVOID*>(&placement));
		if(hr != E_NOINTERFACE)
		{
			Check(hr);
			Check(placement->CanCreate(parent,&allowed));
		}
		CheckIf(!allowed,E_INVALIDARG);
		Check(m_root->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&names)));
		for(UINT n = 1; n < 100000; n++)
		{
			TStackRef<IQuadooComponent> exists; TStackRef<IQuadooComponentHost> host; TStackRef<IQuadooComponentCollection> components; name = static_cast<RSTRING>(NULL);
			Check(RStrFormatW(&name,L"%ls%u",RStrToWide(type),n));
			hr = m_root->QueryInterface(__uuidof(IQuadooComponentHost),reinterpret_cast<PVOID*>(&host));
			if(hr == E_NOINTERFACE) { TStackRef<IQuadooControl> visual; hr = names->FindControl(name,&visual); }
			else { Check(hr); CheckIf(!host,E_UNEXPECTED); Check(host->GetComponents(&components)); hr = components->FindComponent(name,&exists); }
			if(hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND)) break;
			Check(hr);
			CheckIf(n == 99999,E_FAIL);
		}
		Check(JSONCreateObject(&definition));
		Check(JSONCreateObject(&properties));
		Check(PutString(definition,L"type",type));
		Check(PutString(definition,L"name",name));
		Check(PutInt(properties,L"Left",bounds->Left));
		Check(PutInt(properties,L"Top",bounds->Top));
		{
			TStackRef<IQuadooPropertyDescriptors> descriptors;
			Check(factory->GetProperties(&descriptors));
			static PCWSTR const colors[] = {L"BackColor",L"ForeColor"};
			for(UINT n = 0; n < ARRAYSIZE(colors); n++)
			{
				TStackRef<IQuadooPropertyDescriptor> descriptor;
				TStackRef<IQuadooProperties> parentProperties;
				TStackRef<IJSONValue> color;
				QuadooProperty::Type propertyType;
				DWORD flags;
				UINT index;
				hr = descriptors->Find(RSTRING_CAST(colors[n]),&descriptor);
				if(hr == DISP_E_MEMBERNOTFOUND) continue;
				Check(hr);
				Check(descriptor->GetType(&propertyType));
				Check(descriptor->GetFlags(&flags));
				if(propertyType != QuadooProperty::Color || (flags & (QuadooForm::PropertyReadOnly | QuadooForm::PropertyRuntimeOnly))) continue;
				hr = Property(parent,colors[n],&parentProperties,&index);
				if(hr == DISP_E_MEMBERNOTFOUND) continue;
				Check(hr);
				Check(parentProperties->GetValueJSON(index,&color));
				// Copy the parent's stored color, including its system-color tag.
				Check(properties->AddValueW(colors[n],color));
			}
		}
		{
			TStackRef<IQuadooPropertyDescriptors> descriptors; TStackRef<IQuadooPropertyDescriptor> caption;
			Check(factory->GetProperties(&descriptors));
			if(SUCCEEDED(descriptors->Find(RSTRING_CAST(L"Caption"),&caption)))
			{
				Check(PutString(properties,L"Caption",name));
			}
		}
		{
			TStackRef<IQuadooPropertyDescriptors> descriptors; TStackRef<IQuadooPropertyDescriptor> tabOrder;
			Check(factory->GetProperties(&descriptors));
			hr = descriptors->Find(RSTRING_CAST(L"TabOrder"),&tabOrder);
			if(hr != DISP_E_MEMBERNOTFOUND)
			{
				INT next;
				Check(hr);
				Check(NextTabOrder(parent,&next));
				Check(PutInt(properties,L"TabOrder",next));
			}
		}
		Check(JSONWrapObject(properties,&wrapped));
		Check(definition->AddValueW(L"properties",wrapped));
		Check(PrepareInsert(parent,definition,FALSE,&sourceEdit));
		Check(QuadooCreateControl(m_context,parent,definition,control));
		{
			TStackRef<IQuadooDesignCapabilities> capability; DWORD flags = 0; QuadooForm::Rect actual;
			Check((*control)->GetBounds(&actual));
			if(SUCCEEDED((*control)->QueryInterface(__uuidof(IQuadooDesignCapabilities),reinterpret_cast<PVOID*>(&capability))))
			{
				Check(capability->GetDesignCapabilities(&flags));
			}
			if((flags & QuadooForm::DesignResizeWidth) && bounds->Width > 0) actual.Width = bounds->Width;
			if((flags & QuadooForm::DesignResizeHeight) && bounds->Height > 0) actual.Height = bounds->Height;
			Check((*control)->SetBounds(&actual));
			Check(CapturePages(*control));
			TStackRef<IQuadooTabControl> tabs;
			if(SUCCEEDED(parent->QueryInterface(__uuidof(IQuadooTabControl),reinterpret_cast<PVOID*>(&tabs))))
			{
				GUID id;
				Check(tabs->GetID(&id));
				BOOL empty = FALSE;
				for(sysint n = 0; n < m_pages.Length(); n++) if(IsEqualGUID(m_pages[n].tabs,id)) empty = IsEqualGUID(m_pages[n].active,GUID_NULL);
				if(empty)
				{
					Check(RememberPage(tabs));
				}
			}
		}
		m_dirty = TRUE;
	Cleanup:
		if(FAILED(hr) && control && *control) { (*control)->Destroy(); SafeRelease(*control); }
		return FinishSourceEdit(sourceEdit,hr);
	}

	static HRESULT FindOutlineEntry(IQuadooComponent* component, REFGUID id, IQuadooComponent** found, UINT depth)
	{
		HRESULT hr;
		TStackRef<IQuadooComponentOutline> outline;
		TStackRef<IQuadooControlContainer> container;
		UINT count;
		CheckIf(depth >= 128,E_INVALIDARG);
		hr = component->QueryInterface(&outline);
		if(hr != E_NOINTERFACE)
		{
			Check(hr);
			Check(outline->GetOutlineCount(&count));
			CheckIf(count > 4096,E_INVALIDARG);
			for(UINT n = 0; n < count; n++)
			{
				TStackRef<IQuadooComponent> child;
				GUID current;
				Check(outline->GetOutlineItem(n,&child));
				Check(child->GetID(&current));
				if(IsEqualGUID(current,id))
				{
					*found = child.Detach();
					goto Cleanup;
				}
				hr = FindOutlineEntry(child,id,found,depth + 1);
				CheckIf(hr != HRESULT_FROM_WIN32(ERROR_NOT_FOUND),hr);
			}
		}
		hr = component->QueryInterface(&container);
		if(hr != E_NOINTERFACE)
		{
			Check(hr);
			Check(container->GetChildCount(&count));
			for(UINT n = 0; n < count; n++)
			{
				TStackRef<IQuadooControl> child;
				Check(container->GetChild(n,&child));
				hr = FindOutlineEntry(child,id,found,depth + 1);
				CheckIf(hr != HRESULT_FROM_WIN32(ERROR_NOT_FOUND),hr);
			}
		}
		hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
	Cleanup:
		return hr;
	}

	HRESULT CDesignerDocument::FindComponent(REFGUID id, IQuadooComponent** component)
	{
		HRESULT hr; TStackRef<IQuadooControl> control; TStackRef<IQuadooComponentHost> host; TStackRef<IQuadooComponentCollection> components; UINT count;
		CheckIf(!component,E_POINTER); *component = NULL;
		hr = Find(id,&control);
		if(SUCCEEDED(hr)) { Check(control->QueryInterface(__uuidof(IQuadooComponent),reinterpret_cast<PVOID*>(component))); goto Cleanup; }
		CheckIf(hr != HRESULT_FROM_WIN32(ERROR_NOT_FOUND),hr);
		hr = FindOutlineEntry(m_root,id,component,0);
		CheckIf(hr != HRESULT_FROM_WIN32(ERROR_NOT_FOUND),hr);
		hr = m_root->QueryInterface(__uuidof(IQuadooComponentHost),reinterpret_cast<PVOID*>(&host));
		if(hr == E_NOINTERFACE) { hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND); goto Cleanup; }
		Check(hr); CheckIf(!host,E_UNEXPECTED);
		Check(host->GetComponents(&components)); Check(components->GetCount(&count));
		for(UINT n = 0; n < count; n++)
		{
			TStackRef<IQuadooComponent> item; GUID current; Check(components->GetAt(n,&item)); Check(item->GetID(&current));
			if(IsEqualGUID(id,current)) { *component = item.Detach(); hr = S_OK; goto Cleanup; }
		}
		hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
	Cleanup:
		return hr;
	}

	HRESULT CDesignerDocument::AddComponent(RSTRING type, IQuadooComponent** component)
	{
		HRESULT hr;
		TStackRef<IQuadooComponentHost> host;
		TStackRef<IQuadooComponentCollection> components;
		TStackRef<IJSONObject> definition; CRString name;
		TStackRef<ISourceEdit> sourceEdit;
		CheckIf(!component,E_POINTER); *component = NULL;
		CheckIf(!m_root || !type,E_INVALIDARG);
		if(m_usage)
		{
			Check(m_usage->Include(type));
		}
		Check(m_root->QueryInterface(__uuidof(IQuadooComponentHost),reinterpret_cast<PVOID*>(&host)));
		Check(host->GetComponents(&components));
		for(UINT n = 1; n < 100000; n++)
		{
			TStackRef<IQuadooComponent> exists; name = static_cast<RSTRING>(NULL);
			Check(RStrFormatW(&name,L"%ls%u",RStrToWide(type),n));
			hr = components->FindComponent(name,&exists);
			if(hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND))
				break;
			Check(hr);
			CheckIf(n == 99999,E_FAIL);
		}
		Check(JSONCreateObject(&definition));
		Check(PutString(definition,L"type",type));
		Check(PutString(definition,L"name",name));
		Check(PrepareInsert(m_root,definition,TRUE,&sourceEdit));
		Check(QuadooCreateComponent(m_context,components,definition,component));
		{
			TStackRef<IQuadooMenu> menu;
			HRESULT query = (*component)->QueryInterface(__uuidof(IQuadooMenu),reinterpret_cast<PVOID*>(&menu));
			if(query != E_NOINTERFACE)
			{
				BOOL popup;
				Check(query);
				CheckIf(!menu,E_UNEXPECTED);
				Check(menu->IsPopup(&popup));
				if(!popup)
				{
					Check(SetMenuAssignment(*component));
				}
			}
		}
		m_dirty = TRUE;
	Cleanup:
		if(FAILED(hr) && component && *component)
		{
			(*component)->Destroy();
			SafeRelease(*component);
		}
		return FinishSourceEdit(sourceEdit,hr);
	}

	HRESULT CDesignerDocument::SetProperty(IQuadooComponent* control, UINT index, IJSONValue* value)
	{
		HRESULT hr;
		TStackRef<IQuadooProperties> properties;
		TStackRef<IJSONValue> previous;
		TStackRef<IJSONObject> snapshot;
		TStackRef<IQuadooPropertyDescriptors> descriptors;
		TStackRef<IQuadooPropertyDescriptor> descriptor;
		CRString propertyName;
		DWORD flags;
		TStackRef<ISourceEdit> sourceEdit;
		QuadooProperty::Type type;
		GUID id;

		CheckIf(!control,E_INVALIDARG);
		Check(Serialize(&snapshot));
		Check(control->GetProperties(&properties));
		Check(properties->GetDescriptors(&descriptors));
		Check(descriptors->GetAt(index,&descriptor));
		Check(descriptor->GetFlags(&flags));
		CheckIf(flags & (QuadooForm::PropertyReadOnly | QuadooForm::PropertyRuntimeOnly),E_ACCESSDENIED);
		Check(descriptor->GetName(&propertyName));
		Check(properties->GetValueJSON(index,&previous));
		Check(descriptor->GetType(&type));
		if(type == QuadooProperty::Event || propertyName == RSTRING_CAST(L"Name"))
		{
			Check(control->GetID(&id));
			Check(PrepareProperty(id,propertyName,type == QuadooProperty::Event,value,&sourceEdit));
		}
		hr = properties->SetValueJSON(index,value);
		if(FAILED(hr))
		{
			HRESULT restore = properties->SetValueJSON(index,previous);
			if(FAILED(restore))
			{
				BOOL dirty = m_dirty;
				Load(GetParent(m_host),snapshot,m_path);
				m_dirty = dirty;
			}
			goto Cleanup;
		}
		if(hr == S_FALSE)
			goto Cleanup;
		{
			TStackRef<IQuadooTabControl> tabs;
			if(propertyName == RSTRING_CAST(L"ActivePageIndex") && SUCCEEDED(control->QueryInterface(__uuidof(IQuadooTabControl),reinterpret_cast<PVOID*>(&tabs)))) { Check(RememberPage(tabs)); }
		}
		m_dirty = TRUE;
	Cleanup:
		return FinishSourceEdit(sourceEdit,hr);
	}

	HRESULT CDesignerDocument::SetBounds(IQuadooControl* control, const QuadooForm::Rect* bounds)
	{
		HRESULT hr; QuadooForm::Rect previous; TStackRef<IJSONObject> snapshot;
		CheckIf(!control || !bounds,E_INVALIDARG); Check(control->GetBounds(&previous));
		CheckIfIgnore(previous.Left == bounds->Left && previous.Top == bounds->Top && previous.Width == bounds->Width && previous.Height == bounds->Height,S_FALSE);
		Check(Serialize(&snapshot));
		hr = control->SetBounds(bounds);
		if(FAILED(hr))
		{
			if(FAILED(control->SetBounds(&previous)))
			{
				BOOL dirty = m_dirty;
				Load(GetParent(m_host),snapshot,m_path);
				m_dirty = dirty;
			}
			goto Cleanup;
		}
		if(hr != S_FALSE)
			m_dirty = TRUE;
	Cleanup:
		return hr;
	}

	HRESULT CDesignerDocument::Remove(IQuadooComponent* control)
	{
		HRESULT hr;
		TStackRef<IJSONObject> previous;
		TStackRef<IQuadooControlContainer> parent;
		TStackRef<IQuadooTabControl> tabs;
		GUID removed, parentID;
		BOOL replaceActive = FALSE;
		TStackRef<ISourceEdit> sourceEdit;

		CheckIf(!control || SameObject(control,m_root),E_INVALIDARG);
		{
			TStackRef<IQuadooMenuItems> menu;
			if(SUCCEEDED(control->QueryInterface(__uuidof(IQuadooMenuItems),reinterpret_cast<PVOID*>(&menu))))
			{
				Check(RemoveMenu(control));
				goto Cleanup;
			}
		}
		Check(Serialize(&previous));
		{
			TStackRef<IQuadooControl> visual;
			hr = control->QueryInterface(__uuidof(IQuadooControl),reinterpret_cast<PVOID*>(&visual));
			if(hr != E_NOINTERFACE)
			{
				Check(hr);
				Check(visual->GetParent(&parent));
			}
		}
		Check(control->GetID(&removed));
		if(parent && SUCCEEDED(parent->QueryInterface(__uuidof(IQuadooTabControl),reinterpret_cast<PVOID*>(&tabs))))
		{
			Check(tabs->GetID(&parentID));
			for(sysint n = 0; n < m_pages.Length(); n++)
			{
				if(IsEqualGUID(m_pages[n].tabs,parentID))
					replaceActive = IsEqualGUID(m_pages[n].active,removed);
			}
		}
		Check(PrepareRemove(removed,&sourceEdit));
		hr = control->Destroy();
		if(FAILED(hr))
		{
			BOOL dirty = m_dirty;
			Load(GetParent(m_host),previous,m_path);
			m_dirty = dirty;
			goto Cleanup;
		}
		// Selecting a page is a view operation. Deleting another page must not save
		// that temporary selection; GUID-based serialization handles reindexing.
		if(replaceActive)
		{
			Check(RememberPage(tabs));
		}
		m_dirty = TRUE;
	Cleanup:
		return FinishSourceEdit(sourceEdit,hr);
	}
}
