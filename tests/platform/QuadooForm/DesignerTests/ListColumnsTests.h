#include "..\..\..\..\platform\QuadooForm\Designer\ListColumnsEditor.h"
#include "..\..\..\..\platform\QuadooForm\QuadooForm\PaletteResources.h"
#include "..\TestImages\ImageTestSupport.h"

static HRESULT ListViewPaletteArtwork()
{
	HRESULT hr;
	TStackRef<IQuadooControlRegistry> registry;
	TStackRef<IQuadooControlFactory> factory;
	TStackRef<IQuadooPaletteIcon> provider;
	HICON list = NULL;
	HICON box = NULL;
	CImageTestCanvas canvas;
	Check(QuadooGetControlRegistry(&registry));
	Check(registry->FindFactory(RSTRING_CAST(L"ListView"),&factory));
	Check(factory->QueryInterface(__uuidof(IQuadooPaletteIcon),reinterpret_cast<PVOID*>(&provider)));
	Check(provider->GetPaletteIcon(20,20,&list));
	REQUIRE(PaletteIconSize(list,20));
	REQUIRE(FindResourceW(GetModuleHandleW(L"QuadooForm.dll"),MAKEINTRESOURCEW(IDI_PALETTE_LISTVIEW),RT_GROUP_ICON) != NULL);
	provider.Release();
	factory.Release();
	Check(registry->FindFactory(RSTRING_CAST(L"ListBox"),&factory));
	Check(factory->QueryInterface(__uuidof(IQuadooPaletteIcon),reinterpret_cast<PVOID*>(&provider)));
	Check(provider->GetPaletteIcon(20,20,&box));
	Check(canvas.Create(20,20));
	canvas.Fill(RGB(255,255,255));
	REQUIRE(DrawIconEx(canvas.DC(),0,0,list,20,20,0,NULL,DI_NORMAL));
	DWORD listHash = canvas.Hash();
	canvas.Fill(RGB(255,255,255));
	REQUIRE(DrawIconEx(canvas.DC(),0,0,box,20,20,0,NULL,DI_NORMAL));
	REQUIRE(canvas.Hash() != listHash);
Cleanup:
	if(list)
	{
		DestroyIcon(list);
	}
	if(box)
	{
		DestroyIcon(box);
	}
	return hr;
}

static UINT s_columnDialogAction;
static UINT s_columnDialogTicks;
static BOOL s_columnDialogSeen;
static VOID CALLBACK ColumnDialogTimer(HWND owner, UINT, UINT_PTR timer, DWORD)
{
	HWND dialog = FindWindowW(L"#32770",L"ListView Columns Editor");
	if(!dialog)
	{
		HWND generic = FindWindowW(L"#32770",L"Columns");
		DWORD process = 0;
		if(generic && GetWindowThreadProcessId(generic,&process) && process == GetCurrentProcessId())
		{
			KillTimer(owner,timer);
			SendMessageW(generic,WM_COMMAND,IDCANCEL,0);
			return;
		}
	}
	if(!dialog && ++s_columnDialogTicks < 100)
	{
		return;
	}
	KillTimer(owner,timer);
	if(dialog)
	{
		s_columnDialogSeen = TRUE;
		SendMessageW(dialog,WM_COMMAND,821,0);
		SetDlgItemTextW(dialog,811,L"Dialog column");
		SetDlgItemTextW(dialog,812,L"137");
		if(s_columnDialogAction == 2)
		{
			SendMessageW(dialog,WM_COMMAND,0x3021,0);
			SetDlgItemTextW(dialog,811,L"Uncommitted later change");
			SendMessageW(dialog,WM_COMMAND,IDCANCEL,0);
		}
		else
		{
			SendMessageW(dialog,WM_COMMAND,s_columnDialogAction == 1 ? IDOK : IDCANCEL,0);
		}
	}
}

static HRESULT DesignerListColumns()
{
	HRESULT hr;
	HWND host = Host();
	TStackRef<QFD::CDesignerEditor> editor;
	TStackRef<IJSONObject> definition;
	TStackRef<IJSONObject> original;
	TStackRef<IJSONObject> saved;
	TStackRef<IJSONObject> current;
	TStackRef<IQuadooControl> control;
	TStackRef<IQuadooControlContainer> root;
	TStackRef<IQuadooListView> view;
	TStackRef<IQuadooListColumns> columns;
	TStackRef<IQuadooListColumn> first;
	TStackRef<IQuadooListColumn> second;
	TStackRef<IQuadooListColumn> added;
	GUID id;
	GUID firstID;
	GUID addedID;
	UINT count;
	CRString caption;
	INT width;
	QuadooForm::Rect bounds = {10,190,240,120};
	CheckIf(!host,E_FAIL);
	Check(ListViewPaletteArtwork());
	Check(CreateEditor(host,&editor));
	Check(Parse(L"{\"type\":\"Form\",\"children\":[{\"type\":\"ListView\",\"name\":\"Results\",\"properties\":{\"Columns\":[{\"Caption\":\"Name\",\"Width\":101},{\"Caption\":\"Detail\",\"Width\":173}],\"Items\":[{\"Caption\":\"alpha\",\"SubItems\":[\"beta\"]}]}}]}",&definition));
	Check(editor->LoadDefinition(definition));
	Check(editor->Document().Root()->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&root)));
	Check(root->FindControl(RSTRING_CAST(L"Results"),&control));
	Check(control->GetID(&id));
	Check(editor->SelectID(id));
	REQUIRE(editor->CanExecute(QFD::ListColumnsEditor) && !editor->CanExecute(QFD::TreeNodesEditor));
	REQUIRE(InspectorRow(editor,L"Columns") >= 0 && InspectorRow(editor,L"Items") < 0);
	{
		HMENU menu = QFD::CreateControlMenu();
		CheckIf(!menu,E_FAIL);
		editor->UpdateCommandMenu(menu);
		BOOL enabled = !(GetMenuState(menu,QFD::ListColumnsEditor,MF_BYCOMMAND) & MF_GRAYED);
		DestroyMenu(menu);
		REQUIRE(enabled);
	}
	Check(editor->Document().Serialize(&original));
	{
		QFD::CListColumnsEditSession session;
		Check(session.Initialize(editor,id));
		Check(session.columns->Insert(2,RSTRING_CAST(L"Canceled"),&added));
	}
	Check(editor->Document().Serialize(&saved));
	REQUIRE(JSONCompareObjects(original,saved) == 0 && !editor->Document().Dirty());
	saved.Release();
	added.Release();
	{
		QFD::CListColumnsEditSession session;
		Check(session.Initialize(editor,id));
		Check(session.columns->GetAt(0,&first));
		Check(first->GetID(&firstID));
		Check(first->SetWidth(125));
		Check(session.columns->GetAt(1,&second));
		Check(second->SetCaption(RSTRING_CAST(L"Description")));
		Check(second->SetAlignment(QuadooForm::ListColumnRight));
		Check(second->SetDisplayIndex(0));
		Check(session.columns->Insert(2,RSTRING_CAST(L"Added"),&added));
		Check(added->GetID(&addedID));
		Check(session.columns->Move(added,1));
		struct RejectColumns : QFD::ISourceSite
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
		REQUIRE(session.Apply() == S_FALSE && editor->Document().Dirty());
	}
	control.Release();
	Check(editor->Document().Find(id,&control));
	Check(control->QueryInterface(__uuidof(IQuadooListView),reinterpret_cast<PVOID*>(&view)));
	Check(view->GetColumns(&columns));
	Check(columns->GetCount(&count));
	REQUIRE(count == 3);
	{
		TStackRef<IQuadooWinControl> native;
		HWND window;
		RECT bounds;
		Check(control->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&native)));
		Check(native->GetHandle(&window));
		HWND header = ListView_GetHeader(window);
		REQUIRE(Header_GetItemCount(header) == 3);
		REQUIRE(GetWindowLongW(header,GWL_STYLE) & WS_VISIBLE);
		REQUIRE(GetWindowRect(header,&bounds) && bounds.bottom > bounds.top && bounds.right > bounds.left);
	}
	first.Release();
	Check(columns->Find(firstID,&first));
	Check(first->GetWidth(&width));
	REQUIRE(width == 125);
	{
		TStackRef<IQuadooListItems> items;
		TStackRef<IQuadooListItem> item;
		Check(view->GetItems(&items));
		Check(items->GetAt(0,&item));
		Check(item->GetSubItem(1,&caption));
		REQUIRE(caption == RSTRING_CAST(L"beta"));
		caption = NULL;
		Check(item->GetSubItem(0,&caption));
		REQUIRE(!RStrLen(caption));
	}
	Check(editor->Document().Serialize(&saved));
	Check(editor->LoadDefinition(saved));
	Check(editor->Document().Serialize(&current));
	REQUIRE(JSONCompareObjects(saved,current) == 0);
	current.Release();
	Check(editor->SelectID(id));
	for(UINT action = 0; action < 3; action++)
	{
		s_columnDialogSeen = FALSE;
		s_columnDialogTicks = 0;
		s_columnDialogAction = action;
		CheckIf(!SetTimer(host,8151,30,ColumnDialogTimer),E_FAIL);
		if(action == 1)
		{
			StartInline(editor,L"Columns");
		}
		else
		{
			Check(editor->ExecuteCommand(QFD::ListColumnsEditor,host));
		}
		KillTimer(host,8151);
		REQUIRE(s_columnDialogSeen);
		Check(editor->Document().Serialize(&current));
		REQUIRE((JSONCompareObjects(saved,current) == 0) == (action == 0));
		saved = current;
		current.Release();
	}
	control.Release();
	view.Release();
	columns.Release();
	first.Release();
	Check(editor->Document().Find(id,&control));
	Check(control->QueryInterface(__uuidof(IQuadooListView),reinterpret_cast<PVOID*>(&view)));
	Check(view->GetColumns(&columns));
	Check(columns->GetCount(&count));
	REQUIRE(count == 5);
	Check(columns->GetAt(4,&first));
	caption = NULL;
	Check(first->GetCaption(&caption));
	Check(first->GetWidth(&width));
	REQUIRE(caption == RSTRING_CAST(L"Dialog column") && width == 137);
	// Collection dialogs are single-component operations; common scalars remain editable.
	GUID other;
	root.Release();
	Check(editor->Document().Root()->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&root)));
	Check(editor->AddControl(RSTRING_CAST(L"ListView"),root,&bounds,&other));
	Check(editor->SelectID(id));
	Check(editor->SelectID(other,TRUE));
	REQUIRE(editor->SelectionCount() == 2 && !editor->CanExecute(QFD::ListColumnsEditor));
	{
		LVITEMW row = {0};
		row.mask = LVIF_STATE;
		row.iItem = InspectorRow(editor,L"Columns");
		REQUIRE(row.iItem >= 0 && InspectorRow(editor,L"ReadOnly") >= 0);
		QFD::CSelectionProperties properties;
		GUID ids[] = {id,other};
		UINT index;
		DWORD flags;
		Check(properties.Initialize(&editor->Document(),ids,ARRAYSIZE(ids)));
		Check(properties.Find(RSTRING_CAST(L"Columns"),&index,&flags));
		REQUIRE(flags & QuadooForm::PropertyReadOnly);
		Check(properties.Find(RSTRING_CAST(L"ReadOnly"),&index,&flags));
		REQUIRE(!(flags & QuadooForm::PropertyReadOnly));
		TStackRef<IJSONValue> enabled;
		Check(JSONCreateBool(true,&enabled));
		Check(editor->SetSelectedProperty(index,enabled));
		control.Release();
		Check(editor->Document().Find(id,&control));
		REQUIRE(QFD::BoolProperty(control,L"ReadOnly"));
		control.Release();
		Check(editor->Document().Find(other,&control));
		REQUIRE(QFD::BoolProperty(control,L"ReadOnly"));
	}
	hr = S_OK;
Cleanup:
	if(host)
	{
		KillTimer(host,8151);
	}
	editor.Release();
	if(host)
	{
		DestroyWindow(host);
	}
	PumpPendingMessages();
	return hr;
}
