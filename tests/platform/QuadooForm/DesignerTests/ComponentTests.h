static HRESULT DesignerComponents()
{
	HRESULT hr;
	HWND host = Host(); TStackRef<QFD::CDesignerEditor> editor;
	TStackRef<IQuadooComponent> open, save; TStackRef<IQuadooComponentCollection> components;
	TStackRef<IJSONValue> value; TStackRef<IJSONObject> before, after;
	GUID openID, saveID, selected; UINT count, index; TStackRef<IQuadooProperties> values;
	CheckIf(!host,HrEnsureLastError()); Check(CreateEditor(host,&editor)); Check(editor->NewDocument(TRUE));
	Check(editor->AddComponent(RSTRING_CAST(L"OpenFile"),&openID)); Check(editor->AddComponent(RSTRING_CAST(L"SaveFile"),&saveID));
	Check(editor->Document().FindComponent(openID,&open)); Check(editor->Document().FindComponent(saveID,&save));
	REQUIRE(ListView_GetItemCount(editor->TrayWindow()) == 2 && TreeView_GetCount(editor->TreeWindow()) == 4);
	Check(editor->SelectID(openID)); Check(editor->GetSelectedID(&selected)); REQUIRE(IsEqualGUID(openID,selected));
	Check(QFD::Property(open,L"Title",&values,&index)); Check(JSONCreateStringW(L"Choose data",11,&value)); Check(editor->SetSelectedProperty(index,value)); value.Release(); values.Release();
	Check(QFD::Property(open,L"OnAccept",&values,&index)); Check(JSONCreateStringW(L"OpenFile1_Accept",16,&value)); Check(editor->SetSelectedProperty(index,value)); value.Release(); values.Release();
	REQUIRE(QFD::Property(open,L"Left",&values,&index) == DISP_E_MEMBERNOTFOUND);
	Check(editor->Document().Serialize(&before)); Check(editor->LoadDefinition(before)); Check(editor->Document().Serialize(&after)); REQUIRE(JSONCompareObjects(before,after) == 0); after.Release();
	Check(editor->SelectID(saveID)); Check(editor->Delete()); Check(TestComponents(editor->Document().Root(),&components)); Check(components->GetCount(&count)); REQUIRE(count == 1); components.Release();
	open.Release(); save.Release();
	Check(editor->NewDocument(FALSE)); Check(editor->AddComponent(RSTRING_CAST(L"OpenFile"),&openID));
	Check(TestComponents(editor->Document().Root(),&components)); Check(components->GetCount(&count)); REQUIRE(count == 1 && !editor->Document().IsForm());
	{
		Check(PickTool(editor,L"Timer"));
		TStackRef<IQuadooComponent> timer; Check(components->GetCount(&count)); Check(components->FindComponent(RSTRING_CAST(L"Timer1"),&timer)); REQUIRE(count == 2 && ListView_GetItemCount(editor->TrayWindow()) == 2);
		Check(timer->GetID(&selected)); Check(editor->SelectID(selected));
		Check(QFD::Property(timer,L"Interval",&values,&index)); Check(JSONCreateInteger(15,&value)); Check(editor->SetSelectedProperty(index,value)); values.Release(); value.Release();
		Check(QFD::Property(timer,L"OnTimer",&values,&index)); Check(JSONCreateStringW(L"Timer1_Tick",11,&value)); Check(editor->SetSelectedProperty(index,value)); values.Release(); value.Release();
		before.Release(); after.Release(); Check(editor->Document().Serialize(&before)); Check(editor->LoadDefinition(before)); Check(editor->Document().Serialize(&after)); REQUIRE(JSONCompareObjects(before,after) == 0);
	}
	{
		TStackRef<IQuadooComponent> colors;
		TStackRef<IQuadooColorDialog> operation;
		BOOL accepted = TRUE;
		Check(PickTool(editor,L"ColorDialog"));
		components.Release();
		Check(TestComponents(editor->Document().Root(),&components));
		Check(components->FindComponent(RSTRING_CAST(L"ColorDialog1"),&colors));
		Check(colors->GetID(&selected));
		Check(editor->SelectID(selected));
		Check(colors->QueryInterface(__uuidof(IQuadooColorDialog),reinterpret_cast<PVOID*>(&operation)));
		REQUIRE(operation->Execute(host,&accepted) == QuadooForm::EInvalidState && !accepted);
		Check(QFD::Property(colors,L"Color",&values,&index));
		Check(JSONCreateInteger(static_cast<INT>(0x80000000 | COLOR_INFOBK),&value));
		Check(editor->SetSelectedProperty(index,value)); values.Release(); value.Release();
		before.Release(); after.Release();
		Check(editor->Document().Serialize(&before));
		Check(editor->LoadDefinition(before));
		Check(editor->Document().Serialize(&after));
		REQUIRE(JSONCompareObjects(before,after) == 0);
		colors.Release();
		Check(editor->Document().FindComponent(selected,&colors));
		Check(QFD::Property(colors,L"Color",&values,&index));
		INT raw;
		Check(values->GetValueInt(index,&raw)); REQUIRE(raw == static_cast<INT>(0x80000000 | COLOR_INFOBK));
		values.Release();
		Check(editor->SelectID(selected));
		Check(editor->Delete());
	}
Cleanup:
	if(host) DestroyWindow(host); editor.Release();
	return hr;
}
static HRESULT PlaceholderRoundTrips()
{
	HRESULT hr;
	HWND host = Host(); TStackRef<QFD::CDesignerEditor> editor;
	TStackRef<IQuadooControlRegistry> registry; TStackRef<IQuadooComponentRegistry> componentRegistry;
	TStackRef<IQuadooControlFactory> visualFactory; TStackRef<IQuadooComponentFactory> serviceFactory;
	TStackRef<IExternalDesignPolicy> visualPolicy, servicePolicy; TStackRef<IQuadooControl> child;
	TStackRef<IQuadooDesignPlaceholder> placeholder; TStackRef<IQuadooComponentCollection> components; TStackRef<IQuadooComponent> component;
	TStackRef<IJSONObject> definition, before, after, original, saved, candidate; TStackRef<IJSONValue> value, sourceValue;
	TStackRef<IJSONArray> sourceChildren, savedChildren; QuadooForm::PlaceholderReason reason; CRString detail;
	GUID id; UINT index; TStackRef<IQuadooProperties> properties;
	CheckIf(!host,HrEnsureLastError()); Check(QuadooGetControlRegistry(&registry));
	Check(registry->QueryInterface(__uuidof(IQuadooComponentRegistry),reinterpret_cast<PVOID*>(&componentRegistry)));
	Check(CreateExternalFactory(&visualFactory)); Check(registry->RegisterFactory(visualFactory));
	Check(CreateExternalComponentFactory(&serviceFactory)); Check(componentRegistry->RegisterComponentFactory(serviceFactory));
	Check(visualFactory->QueryInterface(__uuidof(IExternalDesignPolicy),reinterpret_cast<PVOID*>(&visualPolicy)));
	Check(serviceFactory->QueryInterface(__uuidof(IExternalDesignPolicy),reinterpret_cast<PVOID*>(&servicePolicy)));
	Check(CreateEditor(host,&editor,registry));
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"Placeholders\",\"unknownRoot\":{\"keep\":true},\"children\":[{\"type\":\"MissingPanel\",\"name\":\"Unavailable\",\"id\":\"{11000000-0000-0000-0000-000000000001}\",\"properties\":{\"Left\":4,\"Top\":8,\"Width\":180,\"Height\":100,\"Custom\":{\"nested\":[1,\"x\",false]}},\"events\":{\"OnUnknown\":\"KeepHandler\"},\"vendor\":[1,2],\"children\":[{\"type\":\"Button\",\"name\":\"Nested\",\"id\":\"{11000000-0000-0000-0000-000000000002}\",\"properties\":{\"Caption\":\"Keep me\"},\"unknown\":17}]}],\"components\":[{\"type\":\"MissingService\",\"name\":\"UnavailableService\",\"id\":\"{11000000-0000-0000-0000-000000000003}\",\"properties\":{\"Opaque\":{\"x\":[3,4]}},\"events\":{\"OnUnknown\":\"KeepServiceHandler\"},\"vendor\":\"keep\"}]}",&definition));
	Check(editor->LoadDefinition(definition));
	REQUIRE(ListView_GetItemCount(editor->TrayWindow()) == 1 && TreeView_GetCount(editor->TreeWindow()) == 5);
	{ TStackRef<IQuadooControlContainer> root; Check(editor->Document().Root()->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&root))); Check(root->FindControl(RSTRING_CAST(L"Unavailable"),&child)); }
	Check(child->QueryInterface(__uuidof(IQuadooDesignPlaceholder),reinterpret_cast<PVOID*>(&placeholder)));
	{ const IID* paths[] = { &__uuidof(IQuadooComponent),&__uuidof(IQuadooControl),&__uuidof(IQuadooWinControl),&__uuidof(IQuadooControlContainer),&__uuidof(IQuadooChildWindowHost),&__uuidof(IQuadooLayoutControl),&__uuidof(IQuadooDesignCapabilities),&__uuidof(IQuadooProperties),&__uuidof(IQuadooPropertyNotifications),&__uuidof(IQuadooConstructionParticipant),&__uuidof(IQuadooDefinitionSource),&__uuidof(IQuadooDesignPlaceholder) }; Check(CheckCOMIdentity(child,paths,ARRAYSIZE(paths))); }
	Check(placeholder->GetReason(&reason,&detail)); REQUIRE(reason == QuadooForm::MissingFactory && RStrLen(detail)); detail = static_cast<RSTRING>(NULL); placeholder.Release();
	Check(editor->Document().Serialize(&before));
	Check(definition->FindNonNullValueW(L"children",&sourceValue)); Check(sourceValue->GetArray(&sourceChildren)); sourceValue.Release();
	Check(before->FindNonNullValueW(L"children",&value)); Check(value->GetArray(&savedChildren)); value.Release();
	Check(sourceChildren->GetObject(0,&original)); Check(savedChildren->GetObject(0,&saved)); REQUIRE(JSONCompareObjects(original,saved) == 0);
	Check(definition->FindNonNullValueW(L"components",&sourceValue)); Check(before->FindNonNullValueW(L"components",&value)); REQUIRE(JSONCompare(sourceValue,value) == 0); sourceValue.Release(); value.Release();
	Check(child->GetID(&id)); Check(editor->SelectID(id));
	Check(QFD::Property(child,L"Left",&properties,&index)); Check(JSONCreateInteger(31,&value)); Check(editor->SetSelectedProperty(index,value)); value.Release(); properties.Release();
	Check(editor->Document().Serialize(&after));
	Check(editor->LoadDefinition(after));
	Check(TestComponents(editor->Document().Root(),&components)); Check(components->FindComponent(RSTRING_CAST(L"UnavailableService"),&component));
	Check(component->QueryInterface(__uuidof(IQuadooDesignPlaceholder),reinterpret_cast<PVOID*>(&placeholder))); placeholder.Release();
	{ const IID* paths[] = { &__uuidof(IQuadooComponent),&__uuidof(IQuadooNonvisualComponent),&__uuidof(IQuadooProperties),&__uuidof(IQuadooPropertyNotifications),&__uuidof(IQuadooConstructionParticipant),&__uuidof(IQuadooDefinitionSource),&__uuidof(IQuadooDesignPlaceholder) }; Check(CheckCOMIdentity(component,paths,ARRAYSIZE(paths))); }
	Check(component->GetID(&id)); Check(editor->SelectID(id)); REQUIRE(ListView_GetNextItem(editor->TrayWindow(),-1,LVNI_SELECTED) == 0);
	{ LRESULT result; REQUIRE(FAILED(editor->PreviewForm(host,&result))); }
	Check(editor->Document().Serialize(&candidate)); REQUIRE(JSONCompareObjects(after,candidate) == 0); candidate.Release();
	components.Release(); component.Release(); child.Release();
	// Registered factories explicitly opt out of Design construction. Descriptors
	// remain editable; genuine failures and malformed known values still reject.
	Check(visualPolicy->SetDesignResult(QuadooForm::EDesignUnsupported)); Check(servicePolicy->SetDesignResult(QuadooForm::EDesignUnsupported));
	definition.Release(); Check(Parse(L"{\"type\":\"Form\",\"name\":\"Typed\",\"children\":[{\"type\":\"ExternalMeter\",\"name\":\"Meter\",\"properties\":{\"Level\":45,\"Legend\":\"typed\"},\"vendor\":{\"keep\":1}}],\"components\":[{\"type\":\"ExternalService\",\"name\":\"Service\",\"properties\":{\"Level\":19},\"events\":{\"OnPulse\":\"Handler\"}}]}",&definition));
	Check(editor->LoadDefinition(definition));
	Check(TestComponents(editor->Document().Root(),&components)); Check(components->FindComponent(RSTRING_CAST(L"Service"),&component));
	Check(component->QueryInterface(__uuidof(IQuadooDesignPlaceholder),reinterpret_cast<PVOID*>(&placeholder))); Check(placeholder->GetReason(&reason,&detail)); REQUIRE(reason == QuadooForm::DesignConstructionUnsupported); placeholder.Release(); detail = static_cast<RSTRING>(NULL);
	Check(component->GetID(&id)); Check(editor->SelectID(id)); Check(QFD::Property(component,L"Level",&properties,&index)); Check(JSONCreateInteger(21,&value)); Check(editor->SetSelectedProperty(index,value)); value.Release(); properties.Release();
	before.Release(); Check(editor->Document().Serialize(&before));
	Check(visualPolicy->SetDesignResult(E_OUTOFMEMORY)); REQUIRE(editor->LoadDefinition(before) == E_OUTOFMEMORY);
	candidate.Release(); Check(editor->Document().Serialize(&candidate)); REQUIRE(JSONCompareObjects(before,candidate) == 0); candidate.Release();
	Check(visualPolicy->SetDesignResult(S_OK)); Check(servicePolicy->SetDesignResult(S_OK));
	components.Release(); component.Release();
	Check(editor->LoadDefinition(before)); Check(TestComponents(editor->Document().Root(),&components)); Check(components->FindComponent(RSTRING_CAST(L"Service"),&component));
	REQUIRE(component->QueryInterface(__uuidof(IQuadooDesignPlaceholder),reinterpret_cast<PVOID*>(&placeholder)) == E_NOINTERFACE && !placeholder);
	GUID resolved; Check(component->GetID(&resolved)); REQUIRE(IsEqualGUID(id,resolved) && QFD::IntProperty(component,L"Level") == 21);
Cleanup:
	if(host) DestroyWindow(host); editor.Release();
	return hr;
}
