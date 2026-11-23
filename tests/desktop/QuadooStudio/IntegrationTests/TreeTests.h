#include "..\..\..\..\platform\QuadooForm\Designer\TreeNodesEditor.h"

static HRESULT StudioTrees(PCWSTR scratch)
{
	HRESULT hr;
	TStackRef<CQuadooStudio> studio;
	TStackRef<IJSONObject> project;
	TStackRef<IQuadooControlContainer> parent;
	TStackRef<IQuadooComponent> component;
	TStackRef<IQuadooComponent> images;
	TStackRef<IQuadooTreeView> tree;
	TStackRef<IQuadooTreeNodes> nodes;
	TStackRef<IQuadooTreeNode> node;
	TStackRef<IQuadooTreeNode> nested;
	TStackRef<IJSONValue> value;
	CRString folder;
	CRString projectPath;
	CRString packages;
	CRString sourcePath;
	CRString source;
	HWND window = NULL;
	GUID id;
	GUID imageID;
	CProjectFile* unit;
	UINT count;
	QuadooForm::Rect bounds = {12,12,220,180};
	Check(RStrFormatW(&folder,L"%ls\\TreeViews",scratch));
	CheckIf(!CreateDirectoryW(folder.c_wstr(),NULL),E_FAIL);
	Check(RStrFormatW(&projectPath,L"%r\\TreeViews.qsproj",static_cast<RSTRING>(folder)));
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
	REQUIRE(GetMenuState(GetMenu(window),ID_FORM_TREENODESEDITOR,MF_BYCOMMAND) != static_cast<UINT>(-1));
	REQUIRE(!Enabled(ide,QFD::TreeNodesEditor));
	Check(editor->AddComponent(RSTRING_CAST(L"ImageList"),&imageID));
	Check(editor->Document().FindComponent(imageID,&images));
	Check(editor->Document().Root()->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&parent)));
	Check(editor->AddControl(RSTRING_CAST(L"TreeView"),parent,&bounds,&id));
	Check(editor->Document().FindComponent(id,&component));
	REQUIRE(Enabled(ide,QFD::TreeNodesEditor) && !Enabled(ide,QFD::ImageListEditor));
	{
		WCHAR reference[40];
		INT length = StringFromGUID2(imageID,reference,ARRAYSIZE(reference));
		CheckIf(!length,E_FAIL);
		Check(JSONCreateStringW(reference,length - 1,&value));
		Check(SetProperty(editor,component,L"Images",value));
		value.Release();
	}
	{
		QFD::CTreeEditSession session;
		Check(session.Initialize(editor,id));
		Check(session.nodes->Insert(NULL,0,RSTRING_CAST(L"Studio root"),&node));
		Check(session.nodes->Insert(node,0,RSTRING_CAST(L"Studio child"),&nested));
		Check(nested->SetImageIndex(0));
		Check(node->SetExpanded(TRUE));
		Check(session.Apply());
	}
	Pump();
	Check(ide->FindSource(RSTRING_CAST(L".\\MainForm.quadoo"),&unit));
	HWND outline = GetDlgItem(window,IDC_PROJECT_FILES);
	REQUIRE(FindComponent(outline,unit,id) != NULL);
	// Nodes are collection objects, and must not appear as GUI components.
	GUID nodeID;
	Check(node->GetID(&nodeID));
	REQUIRE(FindComponent(outline,unit,nodeID) == NULL);
	Check(studio->Exec(NULL,ID_FILE_SAVEALL,0,NULL,NULL));
	Check(ReadText(sourcePath.c_wstr(),source));
	REQUIRE(TStrStr(source.c_wstr(),L"TreeView1") != NULL);
	Check(ide->SetDesignView(FALSE));
	REQUIRE(!Enabled(ide,QFD::TreeNodesEditor));
	Check(ide->SetDesignView(TRUE));
	Check(studio->Exec(NULL,ID_FILE_CLOSEPROJECT,0,NULL,NULL));
	Check(studio->OpenProject(projectPath.c_wstr()));
	Pump();
	editor = studio->Project()->ActiveDesigner();
	REQUIRE(editor != NULL);
	component.Release();
	Check(editor->Document().FindComponent(id,&component));
	Check(component->QueryInterface(__uuidof(IQuadooTreeView),reinterpret_cast<PVOID*>(&tree)));
	Check(tree->GetNodes(&nodes));
	Check(nodes->GetCount(&count));
	REQUIRE(count == 1);
	node.Release();
	nested.Release();
	Check(nodes->GetAt(0,&node));
	Check(node->GetChildCount(&count));
	REQUIRE(count == 1);
	images.Release();
	Check(tree->GetImages(&images));
	REQUIRE(images != NULL);
	Check(editor->SelectID(imageID));
	Check(editor->Delete());
	Check(studio->Exec(NULL,ID_FILE_SAVEALL,0,NULL,NULL));
	Check(studio->Exec(NULL,ID_FILE_CLOSEPROJECT,0,NULL,NULL));
	Check(studio->OpenProject(projectPath.c_wstr()));
	Pump();
	editor = studio->Project()->ActiveDesigner();
	component.Release();
	tree.Release();
	images.Release();
	Check(editor->Document().FindComponent(id,&component));
	Check(component->QueryInterface(__uuidof(IQuadooTreeView),reinterpret_cast<PVOID*>(&tree)));
	Check(tree->GetImages(&images));
	REQUIRE(!images);

	hr = S_OK;
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
