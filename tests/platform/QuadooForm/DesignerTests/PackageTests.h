#include "..\..\..\..\platform\QuadooForm\Packages\Packages.h"
#include "RuntimePluginFixture.h"

class CPackageDelegate : public CBaseUnknown, public IQuadooDelegate
{
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooDelegate)
	END_UNK_MAP
	STDMETHOD(Invoke)(IQuadooEventArgs*) { return S_OK; }
};
class CPackageBinder : public CBaseUnknown, public IQuadooEventsBinder
{
public:
	UINT bound,post;
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooEventsBinder)
	END_UNK_MAP
	CPackageBinder() : bound(0),post(0) {}
	STDMETHOD(BindEvent)(RSTRING name,IQuadooDelegate** value)
	{
		HRESULT hr=S_OK; CRString expected=RSTRING_CAST(L"Plugin_Start"); CheckIf(!value,E_POINTER); *value=NULL; CheckIf(!(expected==name),E_FAIL);
		*value=__new CPackageDelegate; CheckAlloc(*value); bound++;
	Cleanup: return hr;
	}
	STDMETHOD(PostBind)(IQuadooControl*) { post++; return S_OK; }
};

class CPackageTestHost : public QFP::IHost
{
public:
	QFD::CDesignerEditor* editor;
	TStackRef<IJSONObject> unopened;
	BOOL unreadable;
	CPackageTestHost() : editor(NULL),unreadable(FALSE) {}
	HRESULT VisitDefinitions(QFP::IDefinitionVisitor* visitor)
	{
		HRESULT hr=S_OK; TStackRef<IJSONObject> definition;
		CheckIf(unreadable,E_ACCESSDENIED);
		if(editor && editor->Document().Root()) { Check(editor->CommitPropertyEdit()); Check(editor->Document().Serialize(&definition)); Check(visitor->Visit(definition,RSTRING_CAST(L"UnsavedForm.qform.json"))); }
		if(unopened) { Check(visitor->Visit(unopened,RSTRING_CAST(L"UnopenedFrame.qframe.json"))); }
	Cleanup: return hr;
	}
	HRESULT GetEditors(TArray<QFD::CDesignerEditor*>& editors) { return editor ? editors.Append(editor) : S_OK; }
};
static INT PackageLeaseChild(INT count,WCHAR** arguments)
{
	HRESULT hr = E_INVALIDARG;
	HANDLE ready = NULL;
	HANDLE release = NULL;
	HANDLE lease = NULL;
	CRString type;
	if(count == 6)
	{
		ready = OpenEventW(EVENT_MODIFY_STATE,FALSE,arguments[4]);
		release = OpenEventW(SYNCHRONIZE,FALSE,arguments[5]);
		// argv strings need not have the four-byte alignment required by tagged
		// static RSTRING pointers. A managed copy preserves the exact type name.
		hr = RStrCreateW(TStrLenChecked(arguments[3]),arguments[3],&type);
		if(SUCCEEDED(hr))
		{
			hr = QFP::AcquireUsage(arguments[2],type,FALSE,&lease);
		}
		if(SUCCEEDED(hr) && ready && release)
		{
			SetEvent(ready);
			hr = WaitForSingleObject(release,10000) == WAIT_OBJECT_0 ? S_OK : E_FAIL;
		}
		else
		{
			hr = E_FAIL;
		}
	}
	if(lease)
	{
		CloseHandle(lease);
	}
	if(ready)
	{
		CloseHandle(ready);
	}
	if(release)
	{
		CloseHandle(release);
	}
	return FAILED(hr) ? 1 : 0;
}
static HRESULT PackageCrossProcess(QFP::CCatalog* catalog,QFP::IHost* host)
{
	HRESULT hr; GUID id; WCHAR guid[40],executable[MAX_PATH]; CRString readyName,releaseName,command,detail;
	HANDLE ready=NULL,release=NULL; PROCESS_INFORMATION process; STARTUPINFOW startup;
	ZeroMemory(&process,sizeof(process)); ZeroMemory(&startup,sizeof(startup)); startup.cb=sizeof(startup);
	Check(CoCreateGuid(&id)); StringFromGUID2(id,guid,ARRAYSIZE(guid));
	Check(RStrFormatW(&readyName,L"Local\\QFP.Ready.%ls",guid)); Check(RStrFormatW(&releaseName,L"Local\\QFP.Release.%ls",guid));
	ready=CreateEventW(NULL,TRUE,FALSE,readyName.c_wstr()); CheckIf(!ready,HrEnsureLastError()); release=CreateEventW(NULL,TRUE,FALSE,releaseName.c_wstr()); CheckIf(!release,HrEnsureLastError());
	CheckIf(!GetModuleFileNameW(NULL,executable,ARRAYSIZE(executable)),HrEnsureLastError());
	Check(RStrFormatW(&command,L"\"%ls\" --package-lease \"%ls\" QuadooPlugin \"%ls\" \"%ls\"",executable,catalog->UsageDirectory(),readyName.c_wstr(),releaseName.c_wstr()));
	CheckIf(!CreateProcessW(executable,const_cast<PWSTR>(command.c_wstr()),NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&startup,&process),HrEnsureLastError());
	REQUIRE(WaitForSingleObject(ready,5000)==WAIT_OBJECT_0); REQUIRE(catalog->CanRemove(0,host,detail)==S_FALSE);
	REQUIRE(catalog->Remove(0,host)==HRESULT_FROM_WIN32(ERROR_BUSY));
	SetEvent(release); REQUIRE(WaitForSingleObject(process.hProcess,5000)==WAIT_OBJECT_0);
	REQUIRE(catalog->CanRemove(0,host,detail)==S_OK);
Cleanup:
	if(release) SetEvent(release);
	if(process.hProcess) { if(WaitForSingleObject(process.hProcess,2000)==WAIT_TIMEOUT) TerminateProcess(process.hProcess,1); CloseHandle(process.hProcess); }
	if(process.hThread) CloseHandle(process.hThread); if(ready) CloseHandle(ready); if(release) CloseHandle(release); return hr;
}
static HRESULT SavePackageFixture(PCWSTR type,BOOL visual,BOOL container,PCWSTR path)
{
	HRESULT hr; TStackRef<IJSONObject> object,control,property; TStackRef<IJSONArray> properties;
	TStackRef<IJSONValue> value; GUID id; WCHAR guid[40]; CMemoryStream text;
	Check(JSONCreateObject(&object)); Check(QFD::PutInt(object,L"schemaVersion",1)); Check(CoCreateGuid(&id)); StringFromGUID2(id,guid,ARRAYSIZE(guid));
	Check(QFD::PutString(object,L"packageId",RSTRING_CAST(guid))); Check(QFD::PutString(object,L"name",RSTRING_CAST(type)));
	Check(JSONCreateObject(&control)); Check(QFD::PutString(control,L"type",RSTRING_CAST(type))); Check(QFD::PutString(control,L"kind",RSTRING_CAST(visual ? L"visual" : L"nonvisual")));
	Check(JSONCreateBool(container!=FALSE,&value)); Check(control->AddValueW(L"container",value)); value.Release();
	Check(JSONCreateArray(&properties)); Check(JSONCreateObject(&property)); Check(QFD::PutString(property,L"name",RSTRING_CAST(L"Module"))); Check(QFD::PutString(property,L"type",RSTRING_CAST(L"String")));
	Check(JSONWrapObject(property,&value)); Check(properties->Add(value)); value.Release(); Check(JSONWrapArray(properties,&value)); Check(control->AddValueW(L"properties",value)); value.Release();
	Check(JSONWrapObject(control,&value)); Check(object->AddValueW(L"control",value));
	Check(JSONSerializeObject(object,&text)); Check(Text::SaveToFile(text.TGetReadPtr<WCHAR>(),text.DataRemaining()/sizeof(WCHAR),path));
Cleanup: return hr;
}
static HRESULT InvalidPackageMetadata(IJSONObject* prototype)
{
	HRESULT hr;
	static PCWSTR const controls[]={
		L"{\"type\":\"Invalid\",\"kind\":\"nonvisual\",\"container\":true}",
		L"{\"type\":\"Invalid\",\"kind\":\"other\"}",
		L"{\"type\":\"Invalid\",\"kind\":\"nonvisual\",\"properties\":[{\"name\":\"Name\",\"type\":\"String\"}]}",
		L"{\"type\":\"Invalid\",\"kind\":\"nonvisual\",\"properties\":[{\"name\":\"Value\",\"type\":\"Int\",\"default\":\"bad\"}]}",
		L"{\"type\":\"Invalid\",\"kind\":\"nonvisual\",\"properties\":[{\"name\":\"Value\",\"type\":\"Unknown\"}]}",
		L"{\"type\":\"Invalid\",\"kind\":\"nonvisual\",\"properties\":[{\"name\":\"Value\",\"type\":\"String\"},{\"name\":\"Value\",\"type\":\"String\"}]}",
		L"{\"type\":\"Invalid\",\"kind\":\"nonvisual\",\"properties\":[{\"name\":\"Pulse\",\"type\":\"Event\"}]}",
		L"{\"type\":\"Invalid\",\"kind\":\"nonvisual\",\"events\":[{\"name\":\"OnCreate\"}]}",
		L"{\"type\":\"Invalid\",\"kind\":\"nonvisual\",\"events\":[{\"name\":\"Pulse\",\"arguments\":[{\"name\":\"Value\",\"type\":\"Invalid\"}]}]}",
		L"{\"type\":\"Invalid\",\"kind\":\"nonvisual\",\"dll\":\"NeverLoad.dll\"}"
	};
	for(UINT n=0;n<ARRAYSIZE(controls);n++)
	{
		TStackRef<IJSONValue> copy,value; TStackRef<IJSONObject> object; TStackRef<QFP::CDefinition> definition;
		Check(JSONCloneObject(prototype,&copy,TRUE)); Check(copy->GetObject(&object)); Check(JSONParse(NULL,controls[n],TStrLenAssert(controls[n]),&value)); Check(object->AddValueW(L"control",value));
		definition.Attach(__new QFP::CDefinition); CheckAlloc(definition); REQUIRE(FAILED(definition->Initialize(object)));
	}
Cleanup: return hr;
}
static HRESULT PackageReferenceEditing(QFD::CDesignerEditor* editor, REFGUID id)
{
	HRESULT hr;
	TStackRef<IQuadooComponent> component;
	TStackRef<IQuadooProperties> properties;
	TStackRef<IJSONValue> value;
	TStackRef<IJSONObject> saved;
	TStackRef<IJSONObject> again;
	CRString name;
	CRString text;
	GUID rootID;
	HWND combo;
	UINT index;
	INT choice;
	Check(editor->SelectID(id));
	Check(editor->Document().FindComponent(id,&component));
	Check(editor->Document().Root()->GetID(&rootID));
	Check(editor->Document().Root()->GetName(&name));
	Check(ReferenceDropdown(editor,L"Target",editor->OutlineCount() + 1,&combo));
	REQUIRE(SendMessageW(combo,CB_GETCURSEL,0,0) == 0);
	choice = static_cast<INT>(SendMessageW(combo,CB_FINDSTRINGEXACT,static_cast<WPARAM>(-1),reinterpret_cast<LPARAM>(name.c_wstr())));
	REQUIRE(choice > 0);
	SendMessageW(combo,CB_SETCURSEL,choice,0);
	SendMessageW(combo,WM_KEYDOWN,VK_RETURN,0);
	Check(ReferenceAssignment(component,L"Target",rootID));
	Check(editor->Document().Serialize(&saved));
	Check(editor->LoadDefinition(saved));
	component.Release();
	Check(editor->Document().FindComponent(id,&component));
	Check(ReferenceAssignment(component,L"Target",rootID));
	Check(editor->Document().Serialize(&again));
	REQUIRE(JSONCompareObjects(saved,again) == 0);
	Check(editor->SelectID(id));
	Check(ReferenceDropdown(editor,L"Target",editor->OutlineCount() + 1,&combo));
	SendMessageW(combo,CB_SETCURSEL,0,0);
	SendMessageW(GetParent(combo),WM_COMMAND,MAKEWPARAM(120,CBN_SELENDOK),reinterpret_cast<LPARAM>(combo));
	Check(ReferenceAssignment(component,L"Target",GUID_NULL));
	Check(QFD::Property(component,L"Target",&properties,&index));
	Check(JSONCreateString(RSTRING_CAST(L"UnavailableControl"),&value));
	Check(editor->SetSelectedProperty(index,value));
	editor->Document().SetDirty(FALSE);
	Check(ReferenceDropdown(editor,L"Target",editor->OutlineCount() + 2,&combo));
	REQUIRE(SendMessageW(combo,CB_GETCURSEL,0,0) == editor->OutlineCount() + 1);
	SendMessageW(combo,WM_KEYDOWN,VK_RETURN,0);
	text = NULL;
	Check(QFD::StringProperty(component,L"Target",&text));
	REQUIRE(text == RSTRING_CAST(L"UnavailableControl") && !editor->Document().Dirty());
Cleanup:
	return hr;
}
static HRESULT PackagePropertyTypes(QFP::CCatalog* catalog,CPackageTestHost* host,PCWSTR source)
{
	HRESULT hr; TStackRef<IJSONObject> object,control; TStackRef<IJSONValue> value;
	TStackRef<IQuadooComponent> component; TStackRef<IQuadooProperties> properties; TStackRef<IQuadooPropertyDescriptors> descriptors;
	TStackRef<IQuadooPropertyDescriptor> descriptor; TStackRef<IJSONArray> snapshot;
	GUID id; UINT index; LONGLONG wide; DOUBLE real; CRString text,detail; CMemoryStream output;
	static const WCHAR list[]=L"["
		L"{\"name\":\"Wide\",\"type\":\"Long\",\"default\":4294967296},"
		L"{\"name\":\"Real\",\"type\":\"Float\",\"default\":1.25},"
		L"{\"name\":\"Flag\",\"type\":\"Boolean\",\"default\":true},"
		L"{\"name\":\"Tint\",\"type\":\"Color\",\"default\":-2147483633},"
		L"{\"name\":\"Choice\",\"type\":\"Enum\",\"choices\":[{\"name\":\"First\",\"value\":4},{\"name\":\"Second\",\"value\":8}]},"
		L"{\"name\":\"Bits\",\"type\":\"Flags\",\"choices\":[{\"name\":\"One\",\"value\":1},{\"name\":\"Two\",\"value\":2}]},"
		L"{\"name\":\"Lines\",\"type\":\"Memo\",\"default\":\"line one\\nline two\"},"
		L"{\"name\":\"Items\",\"type\":\"List\",\"default\":[\"one\",\"two\"]},"
		L"{\"name\":\"Area\",\"type\":\"Rect\"},{\"name\":\"Position\",\"type\":\"Point\"},"
		L"{\"name\":\"Extent\",\"type\":\"Size\"},{\"name\":\"Typeface\",\"type\":\"Font\"},"
		L"{\"name\":\"Glyph\",\"type\":\"Icon\"},{\"name\":\"Artwork\",\"type\":\"Picture\"},"
		L"{\"name\":\"Target\",\"type\":\"ObjectReference\"},{\"name\":\"Data\",\"type\":\"Collection\"},"
		L"{\"name\":\"LockedFlag\",\"type\":\"Boolean\",\"default\":true,\"readOnly\":true},"
		L"{\"name\":\"Constant\",\"type\":\"Int\",\"default\":3,\"readOnly\":true}]";
	Check(SavePackageFixture(L"PropertyBag",FALSE,FALSE,source)); Check(QFP::LoadJSON(source,&object)); Check(object->FindNonNullValueW(L"control",&value)); Check(value->GetObject(&control)); value.Release();
	Check(JSONParse(NULL,list,ARRAYSIZE(list)-1,&value)); Check(control->AddValueW(L"properties",value)); value.Release();
	Check(JSONSerializeObject(object,&output)); Check(Text::SaveToFile(output.TGetReadPtr<WCHAR>(),output.DataRemaining()/sizeof(WCHAR),source));
	Check(catalog->Install(source,host)); Check(host->editor->AddComponent(RSTRING_CAST(L"PropertyBag"),&id)); Check(host->editor->Document().FindComponent(id,&component));
	Check(QFD::Property(component,L"Wide",&properties,&index)); Check(properties->GetValueLong(index,&wide)); REQUIRE(wide==4294967296LL); properties.Release();
	Check(QFD::Property(component,L"Real",&properties,&index)); Check(properties->GetValueFloat(index,&real)); REQUIRE(real==1.25); properties.Release();
	Check(QFD::Property(component,L"Choice",&properties,&index)); INT integer; Check(properties->GetValueInt(index,&integer)); REQUIRE(integer==4); REQUIRE(FAILED(properties->SetValueInt(index,7))); Check(properties->SetValueInt(index,8)); properties.Release();
	Check(QFD::Property(component,L"Bits",&properties,&index)); Check(properties->SetValueInt(index,3)); REQUIRE(FAILED(properties->SetValueInt(index,4))); properties.Release();
	Check(QFD::Property(component,L"Constant",&properties,&index)); REQUIRE(properties->SetValueInt(index,4)==E_ACCESSDENIED); properties.Release();
	Check(QFD::Property(component,L"Tint",&properties,&index)); Check(properties->GetValueInt(index,&integer)); REQUIRE(static_cast<DWORD>(integer)==0x8000000f); properties.Release();
	Check(QFD::StringProperty(component,L"Lines",&text)); REQUIRE(text==RSTRING_CAST(L"line one\nline two"));
	Check(QFD::Property(component,L"Items",&properties,&index)); Check(properties->GetValueJSON(index,&value)); Check(value->GetArray(&snapshot)); value.Release();
	Check(snapshot->Clear()); snapshot.Release(); Check(properties->GetValueJSON(index,&value)); Check(value->GetArray(&snapshot)); REQUIRE(snapshot->Count()==2); properties.Release(); value.Release(); snapshot.Release();
	Check(component->GetProperties(&properties)); Check(properties->GetDescriptors(&descriptors)); Check(descriptors->Find(RSTRING_CAST(L"Data"),&descriptor)); Check(descriptor->GetDefaultValue(&value)); Check(value->GetArray(&snapshot)); value.Release();
	Check(JSONCreateInteger(9,&value)); Check(snapshot->Add(value)); snapshot.Release(); value.Release(); Check(descriptor->GetDefaultValue(&value)); Check(value->GetArray(&snapshot)); REQUIRE(snapshot->Count()==0); snapshot.Release(); value.Release();
	REQUIRE(properties->GetValueJSON(UINT_MAX,&value)==HRESULT_FROM_WIN32(ERROR_INVALID_INDEX) && !value);
	REQUIRE(properties->SetValueInt(UINT_MAX,0)==HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
	{
		HWND field = StartInline(host->editor,L"Wide");
		REQUIRE(field && !(GetWindowLongW(field,GWL_STYLE) & ES_MULTILINE));
		SetWindowTextW(field,L"4294967301");
		SendMessageW(field,WM_KEYDOWN,VK_RETURN,0);
		properties.Release();
		Check(QFD::Property(component,L"Wide",&properties,&index));
		Check(properties->GetValueLong(index,&wide));
		REQUIRE(wide == 4294967301LL);
		properties.Release();
		field = StartInline(host->editor,L"Real");
		REQUIRE(field != NULL);
		SetWindowTextW(field,L"-2.75");
		SendMessageW(field,WM_KEYDOWN,VK_RETURN,0);
		Check(QFD::Property(component,L"Real",&properties,&index));
		Check(properties->GetValueFloat(index,&real));
		REQUIRE(real == -2.75);
		properties.Release();
		REQUIRE(StartInline(host->editor,L"Constant") == NULL);
	}
	Check(ChooseProperty(host->editor,L"Flag",2,0));
	REQUIRE(!QFD::BoolProperty(component,L"Flag",TRUE));
	Check(QFD::ReadProperty(component,L"Flag",&value));
	REQUIRE(value && value->GetType() == JSON::Boolean);
	value.Release();
	Check(ChooseProperty(host->editor,L"Choice",2,0));
	REQUIRE(QFD::IntProperty(component,L"Choice") == 4);
	Check(QFD::ReadProperty(component,L"Choice",&value));
	REQUIRE(value && value->GetType() == JSON::Integer);
	value.Release();
	REQUIRE(StartInline(host->editor,L"LockedFlag") == NULL);
	REQUIRE(QFD::BoolProperty(component,L"LockedFlag"));
	Check(PackageReferenceEditing(host->editor,id));
	component.Release(); properties.Release(); Check(host->editor->Delete()); PumpPendingMessages(); Check(catalog->Remove(0,host));
	// A descriptor remains valid after its factory/type is unregistered.
	Check(descriptor->GetDefaultValue(&value)); Check(value->GetArray(&snapshot)); REQUIRE(snapshot->Count()==0);
Cleanup: return hr;
}
static HRESULT PackageResourceCycles(QFP::CCatalog* catalog,CPackageTestHost* host,PCWSTR source,IJSONValue* icon)
{
	HRESULT hr; TStackRef<IJSONObject> object; CMemoryStream text; CGuiResourceMonitor resources(L"Package controls, fonts and icons");
	TStackRef<IQuadooFactoryLifetime> lifetime; TStackRef<IQuadooControlFactory> factory;
	Check(SavePackageFixture(L"PackageBox",TRUE,TRUE,source)); Check(QFP::LoadJSON(source,&object)); Check(object->AddValueW(L"icon",icon));
	Check(JSONSerializeObject(object,&text)); Check(Text::SaveToFile(text.TGetReadPtr<WCHAR>(),text.DataRemaining()/sizeof(WCHAR),source)); Check(catalog->Install(source,host));
	Check(catalog->FindFactory(RSTRING_CAST(L"PackageBox"),&factory)); Check(factory->QueryInterface(__uuidof(IQuadooFactoryLifetime),reinterpret_cast<PVOID*>(&lifetime)));
	while(!resources.Complete())
	{
		TStackRef<IQuadooControlContainer> root,container; TStackRef<IQuadooControl> box,button;
		Check(host->editor->NewDocument(TRUE)); Check(host->editor->Document().Root()->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&root)));
		QuadooForm::Rect bounds={8,10,220,140}; Check(host->editor->Document().Add(root,RSTRING_CAST(L"PackageBox"),&bounds,&box)); Check(box->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&container)));
		Check(host->editor->Document().Add(container,RSTRING_CAST(L"Button"),&bounds,&button));
		host->editor->SetDPI(144); host->editor->SetDPI(96); PumpPendingMessages();
		HWND handle; { TStackRef<IQuadooWinControl> window; Check(box->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&window))); Check(window->GetHandle(&handle)); }
		button.Release(); container.Release(); box.Release(); root.Release(); Check(host->editor->NewDocument(FALSE)); PumpPendingMessages();
		UINT live; Check(lifetime->GetLiveCount(&live)); REQUIRE(!live && !IsWindow(handle)); Check(resources.AfterCycle());
	}
	Check(catalog->Remove(0,host));
Cleanup: return hr;
}
static HRESULT DesignTimePackages()
{
	HRESULT hr; WCHAR temporary[MAX_PATH]={0},base[MAX_PATH]; HWND window=Host();
	TStackRef<QFP::CCatalog> catalog; TStackRef<QFD::CDesignerEditor> editor;
	TStackRef<QFP::CDefinition> definition; TStackRef<IQuadooComponent> component;
	TStackRef<IQuadooControl> box,leaf,button; TStackRef<IQuadooControlContainer> root,container;
	TStackRef<IQuadooProperties> properties; TStackRef<IJSONObject> saved; TStackRef<IJSONValue> value;
	TStackRef<IQuadooControlFactory> nativeFactory;
	CRString directory,source,sourcePNG,boxSource,leafSource,detail,text,example; GUID id; UINT index; HICON icon=NULL;
	CPackageTestHost host;
	CheckIf(!window,HrEnsureLastError()); Check(NewTemporary(temporary,ARRAYSIZE(temporary))); CheckIf(!DeleteFileW(temporary),HrEnsureLastError()); CheckIf(!CreateDirectoryW(temporary,NULL),HrEnsureLastError());
	Check(RStrFormatW(&directory,L"%ls\\Installed",temporary)); Check(RStrFormatW(&source,L"%ls\\QuadooPlugin.json",temporary)); Check(RStrFormatW(&sourcePNG,L"%ls\\QuadooPlugin.png",temporary));
	CheckIf(!GetModuleFileNameW(NULL,base,ARRAYSIZE(base)),HrEnsureLastError()); PWSTR slash=const_cast<PWSTR>(TStrRChr(base,L'\\')); CheckIf(!slash,E_UNEXPECTED); *slash=0;
	Check(RStrFormatW(&example,L"%ls\\DesignerExamples\\QuadooPlugin.json",base)); CheckIf(!CopyFileW(example.c_wstr(),source.c_wstr(),TRUE),HrEnsureLastError()); example=static_cast<RSTRING>(NULL);
	Check(RStrFormatW(&example,L"%ls\\DesignerExamples\\QuadooPlugin.png",base)); CheckIf(!CopyFileW(example.c_wstr(),sourcePNG.c_wstr(),TRUE),HrEnsureLastError());
	Check(QFP::LoadPackage(source.c_wstr(),TRUE,&definition)); REQUIRE(!definition->visual && !definition->container && definition->type==RSTRING_CAST(L"QuadooPlugin"));
	Check(InvalidPackageMetadata(definition->json));
	REQUIRE(definition->Find(L"Module")!=UINT_MAX && definition->Find(L"Width")==UINT_MAX);
	Check(definition->GetIcon(20,20,&icon)); REQUIRE(icon!=NULL); DestroyIcon(icon); icon=NULL;
	{ TStackRef<IJSONObject> invalid; REQUIRE(FAILED(QFP::ParseJSON(L"{\"x\":1,\"x\":2}",13,&invalid))); REQUIRE(FAILED(QFP::ParseJSON(L"{\"x\":1,\"\\u0078\":2}",18,&invalid))); }
	catalog.Attach(__new QFP::CCatalog); CheckAlloc(catalog); Check(catalog->Initialize(directory.c_wstr())); REQUIRE(catalog->Count()==0);
	Check(catalog->FindFactory(RSTRING_CAST(L"Button"),&nativeFactory));
	{ const IID* paths[]={&__uuidof(IQuadooControlRegistry),&__uuidof(IQuadooComponentRegistry),&__uuidof(IQuadooFactoryCatalog),&__uuidof(IQuadooDesignUsageFactory)}; Check(CheckCOMIdentity(catalog->Registry(),paths,ARRAYSIZE(paths))); }
	Check(CreateEditor(window,&editor,catalog->Registry())); host.editor=editor; Check(editor->NewDocument(TRUE));
	for(LONG fail=0;fail<6;fail++)
	{
		QFP::FailAfter(fail); HRESULT failure=catalog->Install(source.c_wstr(),&host); QFP::FailAfter(-1);
		REQUIRE(FAILED(failure)); REQUIRE(catalog->Count()==0 && FindTool(editor->ToolsWindow(),L"QuadooPlugin")<0);
		{ TStackRef<QFP::CCatalog> check; check.Attach(__new QFP::CCatalog); CheckAlloc(check); Check(check->Initialize(directory.c_wstr())); REQUIRE(check->Count()==0); }
	}
	Check(catalog->Install(source.c_wstr(),&host)); REQUIRE(catalog->Count()==1 && FindTool(editor->ToolsWindow(),L"QuadooPlugin")>=0);
	{ TStackRef<IQuadooControlFactory> current; Check(catalog->FindFactory(RSTRING_CAST(L"Button"),&current)); REQUIRE(QFD::SameObject(nativeFactory,current)); }
	{ const IID* paths[]={&__uuidof(IQuadooComponentFactory),&__uuidof(IQuadooFactoryLifetime),&__uuidof(IQuadooPaletteIcon)}; Check(CheckCOMIdentity(catalog->At(0)->factory,paths,ARRAYSIZE(paths))); }
	REQUIRE(catalog->Install(source.c_wstr(),&host)==HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS));
	CheckIf(!DeleteFileW(source.c_wstr()) || !DeleteFileW(sourcePNG.c_wstr()),HrEnsureLastError());
	{ TStackRef<QFP::CCatalog> reopened; reopened.Attach(__new QFP::CCatalog); CheckAlloc(reopened); Check(reopened->Initialize(directory.c_wstr())); REQUIRE(reopened->Count()==1 && reopened->At(0)->definition && reopened->At(0)->definition->icon); }
	Check(editor->AddComponent(RSTRING_CAST(L"QuadooPlugin"),&id)); Check(editor->Document().FindComponent(id,&component));
	Check(QFD::StringProperty(component,L"Module",&text)); REQUIRE(!RStrLen(text)); text=static_cast<RSTRING>(NULL);
	Check(QFD::Property(component,L"Module",&properties,&index)); Check(JSONCreateString(RSTRING_CAST(L"ProductionPlugin.dll"),&value)); Check(editor->SetSelectedProperty(index,value)); value.Release();
	Check(QFD::StringProperty(component,L"Module",&text)); REQUIRE(text==RSTRING_CAST(L"ProductionPlugin.dll"));
	properties.Release(); Check(QFD::Property(component,L"OnCreate",&properties,&index));
	Check(JSONCreateString(RSTRING_CAST(L"Plugin_Start"),&value)); Check(editor->SetSelectedProperty(index,value)); value.Release();
	{ const IID* paths[]={&__uuidof(IQuadooComponent),&__uuidof(IQuadooProperties),&__uuidof(IQuadooNonvisualComponent),&__uuidof(IQuadooConstructionParticipant),&__uuidof(IQuadooDefinitionSource),&__uuidof(IQuadooPropertyNotifications)}; Check(CheckCOMIdentity(component,paths,ARRAYSIZE(paths))); }
	REQUIRE(catalog->CanRemove(0,&host,detail)==S_FALSE && RStrLen(detail)); detail=static_cast<RSTRING>(NULL);
	Check(editor->Document().Serialize(&saved)); REQUIRE(!saved->HasField(RSTRING_CAST(L"packageId")));
	{ LRESULT result; REQUIRE(FAILED(editor->PreviewForm(window,&result))); }
	{
		// The real factory is independent of the design hierarchy and uses a
		// fresh Runtime registry; no installed package catalog is consulted.
		TStackRef<IQuadooControlRegistry> registry; TStackRef<IQuadooComponentRegistry> components;
		TStackRef<IQuadooPropertyDescriptors> descriptors; TStackRef<IQuadooEventDescriptors> events;
		TStackRef<CRealPluginFactory> factory; TStackRef<CPackageBinder> binder;
		TStackRef<IQuadooControl> form; TStackRef<IQuadooConstructionContext> context; QuadooForm::ContextOptions options;
		ZeroMemory(&options,sizeof(options)); options.cbSize=sizeof(options); options.Mode=QuadooForm::Runtime; options.DPI=96; options.hwndHost=window;
		Check(QuadooGetControlRegistry(&registry)); options.pRegistry=registry;
		Check(QuadooCreateConstructionContext(&options,&context)); REQUIRE(FAILED(QuadooCreateControl(context,NULL,saved,&form)) && !form); context.Release();
		Check(definition->GetProperties(&descriptors)); Check(definition->GetEvents(&events));
		factory.Attach(__new CRealPluginFactory(descriptors,events)); CheckAlloc(factory);
		Check(registry->QueryInterface(__uuidof(IQuadooComponentRegistry),reinterpret_cast<PVOID*>(&components))); Check(components->RegisterComponentFactory(factory));
		binder.Attach(__new CPackageBinder); CheckAlloc(binder); options.pBinder=binder;
		Check(QuadooCreateConstructionContext(&options,&context)); Check(QuadooCreateControl(context,NULL,saved,&form)); REQUIRE(factory->live==1 && binder->bound==1 && binder->post==1);
		Check(form->Destroy()); form.Release(); REQUIRE(factory->live==0);
	}
	Check(editor->Delete()); PumpPendingMessages(); REQUIRE(catalog->CanRemove(0,&host,detail)==S_FALSE);
	component.Release(); properties.Release(); Check(catalog->UpdateUsage(&host)); REQUIRE(catalog->CanRemove(0,&host,detail)==S_OK);
	host.unopened=saved; REQUIRE(catalog->CanRemove(0,&host,detail)==S_FALSE); REQUIRE(RStrLen(detail)>0); host.unopened.Release();
	host.unreadable=TRUE; REQUIRE(FAILED(catalog->CanRemove(0,&host,detail))); REQUIRE(FAILED(catalog->Remove(0,&host))); host.unreadable=FALSE;
	Check(catalog->UpdateUsage(&host));
	Check(PackageCrossProcess(catalog,&host));
	for(LONG fail=0;fail<3;fail++) { QFP::FailAfter(fail); HRESULT failure=catalog->Remove(0,&host); QFP::FailAfter(-1); REQUIRE(FAILED(failure)); REQUIRE(catalog->Count()==1 && FindTool(editor->ToolsWindow(),L"QuadooPlugin")>=0); }
	Check(catalog->Remove(0,&host)); REQUIRE(catalog->Count()==0 && FindTool(editor->ToolsWindow(),L"QuadooPlugin")<0);
	Check(editor->Document().Root()->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&root)));
	Check(RStrFormatW(&boxSource,L"%ls\\Box.json",temporary)); Check(RStrFormatW(&leafSource,L"%ls\\Leaf.json",temporary));
	Check(SavePackageFixture(L"PackageBox",TRUE,TRUE,boxSource.c_wstr())); Check(SavePackageFixture(L"PackageLeaf",TRUE,FALSE,leafSource.c_wstr()));
	Check(catalog->Install(boxSource.c_wstr(),&host)); Check(catalog->Install(leafSource.c_wstr(),&host));
	{ QuadooForm::Rect bounds={10,12,240,160}; Check(editor->Document().Add(root,RSTRING_CAST(L"PackageBox"),&bounds,&box)); }
	Check(box->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&container)));
	{ const IID* paths[]={&__uuidof(IQuadooComponent),&__uuidof(IQuadooControl),&__uuidof(IQuadooWinControl),&__uuidof(IQuadooControlContainer),&__uuidof(IQuadooChildWindowHost),&__uuidof(IQuadooLayoutControl),&__uuidof(IQuadooProperties)}; Check(CheckCOMIdentity(box,paths,ARRAYSIZE(paths))); }
	{ QuadooForm::Rect bounds={8,9,70,24}; Check(editor->Document().Add(container,RSTRING_CAST(L"Button"),&bounds,&button)); bounds.Left=90; Check(editor->Document().Add(container,RSTRING_CAST(L"PackageLeaf"),&bounds,&leaf)); }
	{
		TStackRef<IQuadooControl> label; TStackRef<IQuadooProperties> values; TStackRef<IQuadooWinControl> window;
		QuadooForm::Rect bounds={8,36,120,24}; UINT color; HWND handle;
		Check(editor->Document().Add(container,RSTRING_CAST(L"Label"),&bounds,&label));
		Check(QFD::Property(label,L"ForeColor",&values,&color)); Check(values->SetValueInt(color,RGB(200,20,30)));
		Check(container->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&window))); Check(window->GetHandle(&handle));
		CImageTestCanvas canvas; Check(canvas.Create(240,160)); canvas.Fill(RGB(2,3,4));
		SendMessageW(handle,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(canvas.DC()),PRF_CLIENT);
		UINT red=0; for(INT y=36;y<60;y++) for(INT x=8;x<128;x++) { COLORREF pixel=canvas.Pixel(x,y); if(GetRValue(pixel)>100 && GetGValue(pixel)<80 && GetBValue(pixel)<80) red++; }
		REQUIRE(red>0);
	}
	{ TStackRef<IQuadooControlContainer> invalid; REQUIRE(leaf->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&invalid))==E_NOINTERFACE); }
	{ TStackRef<IQuadooLayoutControl> layout; QuadooForm::Size measured; Check(container->QueryInterface(__uuidof(IQuadooLayoutControl),reinterpret_cast<PVOID*>(&layout))); Check(layout->Measure(&measured)); REQUIRE(measured.Width==160 && measured.Height>=36); }
	{ TStackRef<IQuadooProperties> values; UINT align; Check(QFD::Property(button,L"Align",&values,&align)); Check(values->SetValueInt(align,QuadooForm::AlignClient)); Check(container->UpdateLayout()); QuadooForm::Rect bounds; Check(button->GetEffectiveBounds(&bounds)); REQUIRE(bounds.Width==240 && bounds.Height==160); }
	saved.Release(); Check(editor->Document().Serialize(&saved)); REQUIRE(saved->HasField(RSTRING_CAST(L"children")));
	leaf.Release(); button.Release(); container.Release(); box.Release(); root.Release(); Check(editor->NewDocument(FALSE)); PumpPendingMessages();
	Check(catalog->UpdateUsage(&host)); Check(catalog->Remove(0,&host)); Check(catalog->Remove(0,&host)); REQUIRE(catalog->Count()==0);
	Check(PackagePropertyTypes(catalog,&host,boxSource.c_wstr()));
	Check(PackageResourceCycles(catalog,&host,boxSource.c_wstr(),definition->icon));
Cleanup:
	QFP::FailAfter(-1);
	if(icon) DestroyIcon(icon); host.editor=NULL; component.Release(); properties.Release(); container.Release(); leaf.Release(); button.Release(); box.Release(); root.Release();
	if(window) DestroyWindow(window); editor.Release(); catalog.Release();
	if(FAILED(hr)) { CRString message; if(SUCCEEDED(RStrFormatW(&message,L"Package scratch: %ls\r\n",temporary))) Log(message.c_wstr()); }
	return hr;
}
