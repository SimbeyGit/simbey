#include "..\..\..\..\platform\QuadooForm\Designer\TreeNodesEditor.h"
#include "..\TestImages\TreeFixture.h"

static BOOL s_treeDialogSeen;
static BOOL s_treeDialogAccept;
static UINT s_treeDialogTicks;
static VOID CALLBACK TreeDialogTimer(HWND owner, UINT, UINT_PTR timer, DWORD)
{
	HWND dialog = FindWindowW(L"#32770",L"Tree Nodes Editor");
	if(!dialog && ++s_treeDialogTicks < 100)
	{
		return;
	}
	KillTimer(owner,timer);
	if(dialog)
	{
		s_treeDialogSeen = TRUE;
		SendMessageW(dialog,WM_COMMAND,821,0);
		SetDlgItemTextW(dialog,811,L"Dialog node");
		SendMessageW(dialog,WM_COMMAND,s_treeDialogAccept ? IDOK : IDCANCEL,0);
	}
}
static HRESULT DesignerTrees()
{
	HRESULT hr;
	HWND host = Host();
	TStackRef<QFD::CDesignerEditor> editor;
	TStackRef<IJSONObject> definition;
	TStackRef<IJSONObject> child;
	TStackRef<IJSONObject> original;
	TStackRef<IJSONObject> saved;
	TStackRef<IJSONObject> again;
	TStackRef<IJSONValue> value;
	TStackRef<IJSONArray> children;
	TStackRef<IQuadooControlContainer> root;
	TStackRef<IQuadooControl> control;
	TStackRef<IQuadooTreeView> tree;
	TStackRef<IQuadooTreeNodes> nodes;
	TStackRef<IQuadooTreeNode> node;
	TStackRef<IQuadooTreeNode> nested;
	GUID id;
	UINT count;
	CheckIf(!host,E_FAIL);
	Check(CreateEditor(host,&editor));
	Check(Parse(ImageListNativeDefinition,&definition));
	Check(Parse(TreeChildDefinition,&child));
	Check(JSONCreateArray(&children));
	Check(JSONWrapObject(child,&value));
	Check(children->Add(value));
	value.Release();
	Check(JSONWrapArray(children,&value));
	Check(definition->AddValueW(L"children",value));
	Check(editor->LoadDefinition(definition));
	Check(editor->Document().Root()->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&root)));
	Check(root->FindControl(RSTRING_CAST(L"Tree1"),&control));
	Check(control->GetID(&id));
	Check(editor->SelectID(id));
	REQUIRE(editor->CanExecute(QFD::TreeNodesEditor) && !editor->CanExecute(QFD::ImageListEditor));
	REQUIRE(FindTool(editor->ToolsWindow(),L"TreeView") >= 0);
	REQUIRE(InspectorRow(editor,L"Items") >= 0 && InspectorRow(editor,L"Images") >= 0);
	Check(editor->Document().Serialize(&original));
	{
		QFD::CTreeEditSession session;
		Check(session.Initialize(editor,id));
		Check(session.nodes->Insert(NULL,0,RSTRING_CAST(L"Cancelled"),&node));
	}
	Check(editor->Document().Serialize(&saved));
	REQUIRE(JSONCompareObjects(original,saved) == 0 && !editor->Document().Dirty());
	saved.Release();
	node.Release();
	{
		QFD::CTreeEditSession session;
		Check(session.Initialize(editor,id));
		Check(session.nodes->GetAt(0,&node));
		Check(session.nodes->Insert(node,1,RSTRING_CAST(L"New child"),&nested));
		Check(nested->SetImageIndex(1));
		Check(nested->SetSelectedImageIndex(0));
		Check(nested->SetChecked(TRUE));
		Check(node->SetExpanded(TRUE));
		Check(session.nodes->Move(nested,NULL,1));
		struct RejectTreeEdit : QFD::ISourceSite
		{
			virtual HRESULT PrepareDefinition(IJSONObject*, QFD::ISourceEdit** edit)
			{
				*edit = NULL;
				return E_ABORT;
			}
			virtual HRESULT OpenHandler(REFGUID, RSTRING)
			{
				return E_NOTIMPL;
			}
		} rejected;
		editor->Document().SetSourceSite(&rejected);
		hr = session.Apply();
		editor->Document().SetSourceSite(NULL);
		REQUIRE(hr == E_ABORT && !editor->Document().Dirty());
		Check(editor->Document().Serialize(&saved));
		REQUIRE(JSONCompareObjects(original,saved) == 0);
		saved.Release();
		Check(session.Apply());
		REQUIRE(editor->Document().Dirty());
		REQUIRE(session.Apply() == S_FALSE);
	}
	Check(editor->Document().Serialize(&saved));
	Check(editor->LoadDefinition(saved));
	Check(editor->Document().Serialize(&again));
	REQUIRE(JSONCompareObjects(saved,again) == 0);
	Check(editor->SelectID(id));
	for(UINT accept = 0; accept < 2; accept++)
	{
		s_treeDialogSeen = FALSE;
		s_treeDialogTicks = 0;
		s_treeDialogAccept = accept != 0;
		CheckIf(!SetTimer(host,8031,30,TreeDialogTimer),E_FAIL);
		Check(editor->ExecuteCommand(QFD::TreeNodesEditor,host));
		KillTimer(host,8031);
		REQUIRE(s_treeDialogSeen);
		TStackRef<IJSONObject> current;
		Check(editor->Document().Serialize(&current));
		REQUIRE((JSONCompareObjects(saved,current) == 0) == (accept == 0));
	}
	control.Release();
	Check(editor->Document().Find(id,&control));
	Check(control->QueryInterface(__uuidof(IQuadooTreeView),reinterpret_cast<PVOID*>(&tree)));
	Check(tree->GetNodes(&nodes));
	Check(nodes->GetCount(&count));
	REQUIRE(count == 4);
	hr = S_OK;
Cleanup:
	if(host)
	{
		KillTimer(host,8031);
	}
	if(editor)
	{
		editor.Release();
	}
	if(host)
	{
		DestroyWindow(host);
	}
	PumpPendingMessages();
	return hr;
}
