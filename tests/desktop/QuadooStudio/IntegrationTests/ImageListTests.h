#include "..\..\..\platform\QuadooForm\TestImages\ImageListFixture.h"

static HRESULT StudioImageLists(PCWSTR scratch)
{
	HRESULT hr;
	TStackRef<CQuadooStudio> studio;
	TStackRef<IJSONObject> project;
	TStackRef<IJSONObject> definition;
	TStackRef<IJSONObject> componentDefinition;
	TStackRef<IJSONObject> values;
	TStackRef<IJSONArray> definitions;
	TStackRef<IJSONValue> value;
	TStackRef<IQuadooComponent> component;
	TStackRef<IQuadooProperties> properties;
	CRString folder;
	CRString projectPath;
	CRString packages;
	CRString sourcePath;
	CRString source;
	CRString type;
	HWND window = NULL;
	GUID id;
	CProjectFile* unit;
	UINT index;
	Check(RStrFormatW(&folder,L"%ls\\ImageLists",scratch));
	CheckIf(!CreateDirectoryW(folder.c_wstr(),NULL),E_FAIL);
	Check(RStrFormatW(&projectPath,L"%r\\ImageLists.qsproj",static_cast<RSTRING>(folder)));
	Check(RStrFormatW(&packages,L"%r\\Packages",static_cast<RSTRING>(folder)));
	Check(RStrFormatW(&sourcePath,L"%r\\MainForm.quadoo",static_cast<RSTRING>(folder)));
	Check(JSONCreateObject(&project));
	Check(QFD::PutString(project,L"target",RSTRING_CAST(L"application")));
	Check(CreateApplicationFixture(project,projectPath.c_wstr()));
	studio.Attach(__new CQuadooStudio(GetModuleHandle(NULL)));
	CheckAlloc(studio);
	Check(studio->Initialize(L"",1260,820,SW_SHOWNOACTIVATE,packages.c_wstr()));
	Check(studio->GetWindow(&window));
	Check(studio->OpenProject(projectPath.c_wstr()));
	Pump();
	CQuadooProject* ide = studio->Project();
	QFD::CDesignerEditor* editor = ide->ActiveDesigner();
	REQUIRE(editor != NULL);
	REQUIRE(GetMenuState(GetMenu(window),ID_FORM_IMAGELISTEDITOR,MF_BYCOMMAND) != static_cast<UINT>(-1));
	REQUIRE(!Enabled(ide,QFD::ImageListEditor));
	Check(RStrCreateW(LSP(L"ImageList"),&type));
	Check(editor->AddComponent(type,&id));
	Check(editor->Document().FindComponent(id,&component));
	REQUIRE(Enabled(ide,QFD::ImageListEditor));
	REQUIRE(ListView_GetItemCount(editor->TrayWindow()) == 1);
	{
		CRString menuType;
		GUID menu;
		Check(RStrCreateW(LSP(L"MainMenu"),&menuType));
		Check(editor->AddComponent(menuType,&menu));
		REQUIRE(Enabled(ide,QFD::MenuEditor));
		Check(editor->SelectID(id));
		REQUIRE(!Enabled(ide,QFD::MenuEditor) && Enabled(ide,QFD::ImageListEditor));
	}
	Check(ide->FindSource(RSTRING_CAST(L".\\MainForm.quadoo"),&unit));
	Pump();
	HWND tree = GetDlgItem(window,IDC_PROJECT_FILES);
	REQUIRE(FindComponent(tree,unit,id) != NULL);
	Check(JSONParse(NULL,ImageListNativeDefinition,StaticLength(ImageListNativeDefinition),&value));
	Check(value->GetObject(&definition));
	value.Release();
	Check(definition->FindNonNullValueW(L"components",&value));
	Check(value->GetArray(&definitions));
	value.Release();
	Check(definitions->GetObject(0,&componentDefinition));
	Check(componentDefinition->FindNonNullValueW(L"properties",&value));
	Check(value->GetObject(&values));
	value.Release();
	Check(values->FindNonNullValueW(L"Images",&value));
	Check(SetProperty(editor,component,L"Images",value));
	value.Release();
	Check(QFD::Property(component,L"ImageCount",&properties,&index));
	INT count;
	Check(properties->GetValueInt(index,&count));
	REQUIRE(count == 2);
	Check(JSONCreateString(RSTRING_CAST(L"StudioImages"),&value));
	Check(SetProperty(editor,component,L"Name",value));
	value.Release();
	Check(studio->Exec(NULL,ID_FILE_SAVEALL,0,NULL,NULL));
	Check(ReadText(sourcePath.c_wstr(),source));
	REQUIRE(TStrStr(source.c_wstr(),L"StudioImages") != NULL);
	Check(ide->SetDesignView(FALSE));
	REQUIRE(!Enabled(ide,QFD::ImageListEditor));
	Check(ide->SetDesignView(TRUE));
	Check(studio->Exec(NULL,ID_FILE_CLOSEPROJECT,0,NULL,NULL));
	Check(studio->OpenProject(projectPath.c_wstr()));
	Pump();
	component.Release();
	properties.Release();
	editor = studio->Project()->ActiveDesigner();
	REQUIRE(editor != NULL);
	Check(editor->Document().FindComponent(id,&component));
	REQUIRE(QFD::IntProperty(component,L"ImageCount") == 2);
	Check(editor->SelectID(id));
	Check(editor->Delete());
	Pump();
	Check(studio->Exec(NULL,ID_FILE_SAVEALL,0,NULL,NULL));
	source = NULL;
	Check(ReadText(sourcePath.c_wstr(),source));
	REQUIRE(TStrStr(source.c_wstr(),L"StudioImages") == NULL);
Cleanup:
	if(studio && studio->Project())
	{
		studio->Project()->CloseProject(FALSE);
	}
	if(studio)
	{
		studio->Destroy();
	}
	Pump();
	return hr;
}
