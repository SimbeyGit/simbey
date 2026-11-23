#include "Runtime.h"

namespace QF
{
static HRESULT BuildDefinition(IQuadooConstructionContext* context, IQuadooControlContainer* parent,
	IJSONObject* definition, RSTRING path, UINT depth, IQuadooControl** result)
{
	HRESULT hr;
	TStackRef<IQuadooControlRegistry> registry;
	TStackRef<IQuadooControlFactory> factory;
	TStackRef<IQuadooControl> control;
	TStackRef<IQuadooConstructionParticipant> participant;
	TStackRef<IQuadooControlContainer> container;
	TStackRef<IQuadooFrame> frame;
	TStackRef<IQuadooComponentHost> componentHost;
	TStackRef<IQuadooComponentCollection> components;
	TStackRef<IQuadooComponentRegistry> componentRegistry;
	TStackRef<IQuadooDesignPlaceholder> parentPlaceholder;
	QuadooForm::ContextMode mode;
	TStackRef<IJSONValue> childList;
	TStackRef<IJSONArray> children;
	CRString type;
	TArray<IQuadooControl*> beforeDesignCreate;
	*result = NULL;
	CheckIf(depth >= 128, QuadooForm::EInvalidDefinition);
	Check(ReadString(definition, L"type", L"", &type));
	CheckIf(depth && (SameString(type, RSTRING_CAST(L"Frame")) || SameString(type, RSTRING_CAST(L"FrameInstance"))), QuadooForm::EInvalidDefinition);
	Check(context->GetMode(&mode));
	Check(context->GetRegistry(&registry));
	hr = registry->FindFactory(type,&factory);
	if(hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND))
	{
		Check(QueryOptional(registry,&componentRegistry));
		if(componentRegistry)
		{
			TStackRef<IQuadooComponentFactory> wrongCategory;
			hr = componentRegistry->FindComponentFactory(type,&wrongCategory);
			if(hr == S_OK && wrongCategory) { context->ReportError(QuadooForm::EInvalidDefinition,path,RSTRING_CAST(L"Nonvisual components belong in the root components array, not children.")); hr = QuadooForm::EInvalidDefinition; goto Cleanup; }
			CheckIf(hr != HRESULT_FROM_WIN32(ERROR_NOT_FOUND),FAILED(hr) ? hr : E_UNEXPECTED);
		}
		hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
	}
	if(hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND) && mode == QuadooForm::Design && parent)
	{
		TStackRef<IQuadooComponent> placeholder;
		Check(CreateDesignPlaceholder(context,parent,NULL,definition,NULL,NULL,NULL,QuadooForm::MissingFactory,&placeholder));
		Check(placeholder->QueryInterface(__uuidof(IQuadooControl),reinterpret_cast<PVOID*>(&control)));
	}
	else
	{
		Check(hr);
		Check(ValidateFactory(factory));
		if(mode == QuadooForm::Design && parent)
		{
			UINT count; Check(parent->GetChildCount(&count));
			for(UINT n = 0; n < count; n++) { TStackRef<IQuadooControl> item; Check(parent->GetChild(n,&item)); Check(beforeDesignCreate.Append(item)); item.Detach(); }
		}
		BOOL unavailableParent = parent && SUCCEEDED(parent->QueryInterface(__uuidof(IQuadooDesignPlaceholder),reinterpret_cast<PVOID*>(&parentPlaceholder)));
		hr = unavailableParent ? QuadooForm::EDesignUnsupported : factory->CreateControl(context,parent,definition,&control);
		if(hr == QuadooForm::EDesignUnsupported && mode == QuadooForm::Design && parent && !control)
		{
			UINT count; Check(parent->GetChildCount(&count)); CheckIf(count != static_cast<UINT>(beforeDesignCreate.Length()),E_UNEXPECTED);
			for(UINT n = 0; n < count; n++) { TStackRef<IQuadooControl> item; Check(parent->GetChild(n,&item)); CheckIf(!SameObject(item,beforeDesignCreate[n]),E_UNEXPECTED); }
			TStackRef<IQuadooPropertyDescriptors> properties; TStackRef<IQuadooEventDescriptors> events; TStackRef<IQuadooComponent> placeholder;
			Check(factory->GetProperties(&properties)); Check(factory->GetEvents(&events));
			Check(CreateDesignPlaceholder(context,parent,NULL,definition,properties,events,factory,unavailableParent ? QuadooForm::ParentDesignUnavailable : QuadooForm::DesignConstructionUnsupported,&placeholder));
			Check(placeholder->QueryInterface(__uuidof(IQuadooControl),reinterpret_cast<PVOID*>(&control)));
		}
		else { Check(hr); CheckIf(!control,E_UNEXPECTED); Check(ValidateControlMetadata(control,factory)); }
	}
	CheckIf(depth && SUCCEEDED(control->QueryInterface(__uuidof(IQuadooFrame), reinterpret_cast<PVOID*>(&frame))), QuadooForm::EInvalidDefinition);
	Check(control->QueryInterface(__uuidof(IQuadooConstructionParticipant), reinterpret_cast<PVOID*>(&participant)));
	if(definition->HasField(RSTRING_CAST(L"children")))
	{
		Check(definition->FindNonNullValueW(L"children", &childList)); Check(childList->GetArray(&children));
		if(children->Count()) Check(control->QueryInterface(__uuidof(IQuadooControlContainer), reinterpret_cast<PVOID*>(&container)));
		for(sysint n = 0; n < children->Count(); n++)
		{
			TStackRef<IJSONObject> childDefinition; TStackRef<IQuadooControl> child; CRString childPath;
			Check(RStrFormatW(&childPath, L"%ls.children[%d]", RStrToWide(path), n));
			Check(children->GetObject(n, &childDefinition));
			Check(BuildDefinition(context, container, childDefinition, childPath, depth + 1, &child));
		}
	}
	if(definition->HasField(RSTRING_CAST(L"components")))
	{
		TStackRef<IJSONValue> list; TStackRef<IJSONArray> definitions;
		Check(control->QueryInterface(__uuidof(IQuadooComponentHost),reinterpret_cast<PVOID*>(&componentHost)));
		Check(componentHost->GetComponents(&components));
		Check(definition->FindNonNullValueW(L"components",&list)); Check(list->GetArray(&definitions));
		CheckIf(definitions->Count() > 4096,QuadooForm::EInvalidDefinition);
		for(sysint n = 0; n < definitions->Count(); n++)
		{
			TStackRef<IJSONObject> componentDefinition; TStackRef<IQuadooComponent> component;
			Check(definitions->GetObject(n,&componentDefinition));
			Check(BuildComponent(context,components,componentDefinition,&component));
		}
	}
	*result = control.Detach(); hr = S_OK;
Cleanup:
	if(FAILED(hr))
	{
		CRString detail;
		if(SUCCEEDED(RStrFormatW(&detail, L"%ls: construction failed (0x%.8x)", type.c_wstr() ? type.c_wstr() : L"?", hr))) context->ReportError(hr, path, detail);
		if(participant) participant->AbortConstruction(); else if(control) control->Destroy();
	}
	for(sysint n = 0; n < beforeDesignCreate.Length(); n++) beforeDesignCreate[n]->Release();
	return hr;
}
static HRESULT Collect(IQuadooComponent* control, TArray<IQuadooComponent*>& items, UINT depth)
{
	HRESULT hr;
	TStackRef<IQuadooControlContainer> container;
	CheckIf(depth >= 128, QuadooForm::EInvalidDefinition);
	Check(items.Append(control)); control->AddRef();
	if(SUCCEEDED(control->QueryInterface(__uuidof(IQuadooControlContainer), reinterpret_cast<PVOID*>(&container))))
	{
		UINT count; Check(container->GetChildCount(&count));
		for(UINT n = 0; n < count; n++)
		{ TStackRef<IQuadooControl> child; Check(container->GetChild(n, &child)); Check(Collect(child, items, depth + 1)); }
	}
	{
		TStackRef<IQuadooComponentHost> host;
		Check(QueryOptional(control,&host));
		if(host)
		{
			TStackRef<IQuadooComponentCollection> components; UINT count;
			Check(host->GetComponents(&components)); Check(components->GetCount(&count));
			for(UINT n = 0; n < count; n++) { TStackRef<IQuadooComponent> component; Check(components->GetAt(n,&component)); Check(items.Append(component)); component.Detach(); }
		}
	}
	hr = S_OK;
Cleanup:
	return hr;
}

HRESULT BuildComponent(IQuadooConstructionContext* context, IQuadooComponentCollection* owner, IJSONObject* definition, IQuadooComponent** result)
{
	HRESULT hr;
	TStackRef<IQuadooControlRegistry> registry; TStackRef<IQuadooComponentRegistry> components;
	TStackRef<IQuadooComponentFactory> factory; TStackRef<IQuadooComponent> component;
	TArray<IQuadooComponent*> previous; CRString type; QuadooForm::ContextMode mode; UINT count; BOOL captured = FALSE;
	CheckIf(!result,E_POINTER); *result = NULL;
	CheckIf(!context || !owner || !definition,E_INVALIDARG);
	Check(owner->GetCount(&count));
	for(UINT n = 0; n < count; n++) { TStackRef<IQuadooComponent> item; Check(owner->GetAt(n,&item)); Check(previous.Append(item)); item.Detach(); }
	captured = TRUE;
	Check(context->GetMode(&mode)); Check(context->GetRegistry(&registry));
	Check(registry->QueryInterface(__uuidof(IQuadooComponentRegistry),reinterpret_cast<PVOID*>(&components)));
	CheckIf(!components,E_UNEXPECTED);
	Check(ReadString(definition,L"type",L"",&type));
	{
		TStackRef<IQuadooControlFactory> visual; hr = registry->FindFactory(type,&visual);
		if(hr == S_OK && visual) { context->ReportError(QuadooForm::EInvalidDefinition,type,RSTRING_CAST(L"Visual controls belong in children, not the components array.")); hr = QuadooForm::EInvalidDefinition; goto Cleanup; }
		CheckIf(hr != HRESULT_FROM_WIN32(ERROR_NOT_FOUND),FAILED(hr) ? hr : E_UNEXPECTED);
	}
	if(definition->HasField(RSTRING_CAST(L"components"))) { CheckIf(TRUE,QuadooForm::EInvalidDefinition); }
	if(definition->HasField(RSTRING_CAST(L"children")))
	{ TStackRef<IJSONValue> value; TStackRef<IJSONArray> children; Check(definition->FindNonNullValueW(L"children",&value)); Check(value->GetArray(&children)); CheckIf(children->Count(),QuadooForm::EInvalidDefinition); }
	hr = components->FindComponentFactory(type,&factory);
	if(hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND) && mode == QuadooForm::Design)
	{ Check(CreateDesignPlaceholder(context,NULL,owner,definition,NULL,NULL,NULL,QuadooForm::MissingFactory,&component)); }
	else
	{
		Check(hr); Check(ValidateComponentFactory(factory));
		hr = factory->CreateComponent(context,owner,definition,&component);
		if(hr == QuadooForm::EDesignUnsupported && mode == QuadooForm::Design && !component)
		{
			Check(owner->GetCount(&count)); CheckIf(count != static_cast<UINT>(previous.Length()),E_UNEXPECTED);
			for(UINT n = 0; n < count; n++) { TStackRef<IQuadooComponent> item; Check(owner->GetAt(n,&item)); CheckIf(!SameObject(item,previous[n]),E_UNEXPECTED); }
			TStackRef<IQuadooPropertyDescriptors> properties; TStackRef<IQuadooEventDescriptors> events;
			Check(factory->GetProperties(&properties)); Check(factory->GetEvents(&events));
			Check(CreateDesignPlaceholder(context,NULL,owner,definition,properties,events,factory,QuadooForm::DesignConstructionUnsupported,&component));
		}
		else { Check(hr); CheckIf(!component,E_UNEXPECTED); Check(ValidateComponentMetadata(component,factory)); }
	}
	CheckIf(!component,E_UNEXPECTED);
	{ CRString actualType; Check(component->GetTypeName(&actualType)); CheckIf(!SameString(type,actualType),QuadooForm::EInvalidDefinition); }
	// Creation must actually register the returned canonical identity with owner.
	{ BOOL found = FALSE; Check(owner->GetCount(&count)); for(UINT n = 0; n < count; n++) { TStackRef<IQuadooComponent> item; Check(owner->GetAt(n,&item)); if(SameObject(item,component)) found = TRUE; } CheckIf(!found,E_UNEXPECTED); }
	*result = component.Detach(); hr = S_OK;
Cleanup:
	if(FAILED(hr))
	{
		if(component) { TStackRef<IQuadooConstructionParticipant> abort; if(SUCCEEDED(component->QueryInterface(__uuidof(IQuadooConstructionParticipant),reinterpret_cast<PVOID*>(&abort)))) abort->AbortConstruction(); else component->Destroy(); }
		if(captured && owner && SUCCEEDED(owner->GetCount(&count)))
			for(UINT n = count; n > 0; n--)
			{
				TStackRef<IQuadooComponent> item;
				if(SUCCEEDED(owner->GetAt(n - 1,&item)))
				{
					BOOL existed = FALSE; for(sysint p = 0; p < previous.Length(); p++) if(SameObject(previous[p],item)) existed = TRUE;
					if(!existed) { TStackRef<IQuadooConstructionParticipant> abort; if(SUCCEEDED(item->QueryInterface(__uuidof(IQuadooConstructionParticipant),reinterpret_cast<PVOID*>(&abort)))) abort->AbortConstruction(); else item->Destroy(); }
				}
			}
		if(context) context->ReportError(hr,type,RSTRING_CAST(L"Nonvisual component construction failed"));
	}
	for(sysint n = 0; n < previous.Length(); n++) previous[n]->Release();
	return hr;
}
}

HRESULT WINAPI QuadooCreateControl(IQuadooConstructionContext* context, IQuadooControlContainer* parent,
	IJSONObject* definition, IQuadooControl** result)
{
	HRESULT hr;
	TStackRef<IQuadooControl> root;
	TStackRef<IQuadooConstructionParticipant> participant;
	TStackRef<IQuadooControlContainer> container;
	TStackRef<IQuadooEventsBinder> binder;
	TArray<IQuadooComponent*> items;
	TArray<IQuadooControl*> previous;
	QuadooForm::ContextMode mode;
	BOOL updateStarted = FALSE;
	CheckIf(!result, E_POINTER); *result = NULL; CheckIf(!context || !definition, E_INVALIDARG);
	Check(context->GetMode(&mode));
	if(parent)
	{
		UINT count; Check(parent->GetChildCount(&count));
		for(UINT n = 0; n < count; n++)
		{ TStackRef<IQuadooControl> child; Check(parent->GetChild(n, &child)); Check(previous.Append(child)); child.Detach(); }
		Check(parent->BeginUpdate()); updateStarted = TRUE;
	}
	Check(QF::BuildDefinition(context, parent, definition, RSTRING_CAST(L"$"), 0, &root));
	Check(QF::ValidateMenuGraph(root));
	Check(QF::Collect(root, items, 0));
	for(sysint n = 0; n < items.Length(); n++)
	{
		participant.Release(); Check(items[n]->QueryInterface(__uuidof(IQuadooConstructionParticipant), reinterpret_cast<PVOID*>(&participant)));
		Check(participant->BindEvents(context));
	}
	if(SUCCEEDED(root->QueryInterface(__uuidof(IQuadooControlContainer), reinterpret_cast<PVOID*>(&container)))) { Check(container->UpdateLayout()); }
	{
		TStackRef<IQuadooFormMenu> menus;
		Check(QF::QueryOptional(root,&menus));
		if(menus)
		{
			Check(menus->RefreshMenu());
		}
	}
	Check(context->GetBinder(&binder));
	if(binder && mode == QuadooForm::Runtime) { Check(binder->PostBind(root)); }
	for(sysint n = items.Length(); n > 0; n--)
	{
		participant.Release(); Check(items[n - 1]->QueryInterface(__uuidof(IQuadooConstructionParticipant), reinterpret_cast<PVOID*>(&participant)));
		Check(participant->InitializeControl());
	}
	for(sysint n = 0; n < items.Length(); n++)
	{
		participant.Release(); Check(items[n]->QueryInterface(__uuidof(IQuadooConstructionParticipant), reinterpret_cast<PVOID*>(&participant)));
		Check(participant->ActivateControl());
	}
	if(parent) { updateStarted = FALSE; Check(parent->EndUpdate()); }
	*result = root.Detach(); hr = S_OK;
Cleanup:
	if(FAILED(hr))
	{
		if(root)
		{
			TStackRef<IQuadooConstructionParticipant> abort;
			if(SUCCEEDED(root->QueryInterface(__uuidof(IQuadooConstructionParticipant), reinterpret_cast<PVOID*>(&abort)))) abort->AbortConstruction(); else root->Destroy();
		}
		if(parent && updateStarted)
		{
			UINT count = 0; parent->GetChildCount(&count);
			for(UINT n = count; n > 0; n--)
			{
				TStackRef<IQuadooControl> child;
				if(SUCCEEDED(parent->GetChild(n - 1, &child)))
				{
					BOOL existed = FALSE;
					for(sysint p = 0; p < previous.Length(); p++) if(QF::SameObject(previous[p], child)) existed = TRUE;
					if(!existed)
					{
						TStackRef<IQuadooConstructionParticipant> abort;
						if(SUCCEEDED(child->QueryInterface(__uuidof(IQuadooConstructionParticipant), reinterpret_cast<PVOID*>(&abort)))) abort->AbortConstruction(); else child->Destroy();
					}
				}
			}
		}
		if(context) context->ReportError(hr, RSTRING_CAST(L"$"), RSTRING_CAST(L"Definition construction or initialization failed"));
	}
	if(updateStarted) { HRESULT end = parent->EndUpdate(); if(SUCCEEDED(hr)) hr = end; }
	for(sysint n = 0; n < items.Length(); n++) items[n]->Release();
	for(sysint n = 0; n < previous.Length(); n++) previous[n]->Release();
	return hr;
}
HRESULT WINAPI QuadooCreateForm(IQuadooEventsBinder* binder, IQuadooWinControl* owner, IJSONObject* data, IQuadooForm** form)
{
	HRESULT hr;
	TStackRef<IQuadooConstructionContext> context;
	TStackRef<IQuadooControl> control;
	QuadooForm::ContextOptions options;
	CRString type;
	ZeroMemory(&options, sizeof(options)); options.cbSize = sizeof(options); options.pBinder = binder;
	CheckIf(!form, E_POINTER); *form = NULL; CheckIf(!data, E_INVALIDARG);
	Check(QF::ReadString(data, L"type", L"", &type)); CheckIf(!QF::SameString(type, RSTRING_CAST(L"Form")), QuadooForm::EInvalidDefinition);
	if(owner) { Check(owner->GetHandle(&options.hwndHost)); }
	Check(QuadooCreateConstructionContext(&options, &context)); Check(QuadooCreateControl(context, NULL, data, &control));
	Check(control->QueryInterface(__uuidof(IQuadooForm), reinterpret_cast<PVOID*>(form)));
Cleanup:
	if(FAILED(hr) && control) control->Destroy();
	return hr;
}
HRESULT WINAPI QuadooCreateFrame(IQuadooEventsBinder* binder, IQuadooControlContainer* parent, IJSONObject* data, IQuadooFrame** frame)
{
	HRESULT hr;
	TStackRef<IQuadooConstructionContext> context;
	TStackRef<IQuadooControl> control;
	QuadooForm::ContextOptions options;
	CRString type;
	ZeroMemory(&options, sizeof(options)); options.cbSize = sizeof(options); options.pBinder = binder;
	CheckIf(!frame, E_POINTER); *frame = NULL; CheckIf(!parent || !data, E_INVALIDARG);
	Check(QF::ReadString(data, L"type", L"", &type)); CheckIf(!QF::SameString(type, RSTRING_CAST(L"Frame")), QuadooForm::EInvalidDefinition);
	{ HWND hwnd; Check(parent->GetHandle(&hwnd)); options.DPI = QF::ControlDPI(parent); }
	Check(QuadooCreateConstructionContext(&options, &context)); Check(QuadooCreateControl(context, parent, data, &control));
	Check(control->QueryInterface(__uuidof(IQuadooFrame), reinterpret_cast<PVOID*>(frame)));
Cleanup:
	if(FAILED(hr) && control) control->Destroy();
	return hr;
}
