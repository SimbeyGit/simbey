#include "..\Harness\MenuFixture.h"


static BOOL s_menuDialogVisited;
static UINT s_menuDialogAction;
static HRESULT s_menuDialogResult;
static BOOL CALLBACK DriveMenuDialog(HWND window, LPARAM)
{
	if(!GetDlgItem(window,610) || !GetDlgItem(window,625))
	{
		return TRUE;
	}
	s_menuDialogVisited = TRUE;
	if(s_menuDialogAction < 2)
	{
		SetDlgItemTextW(window,611,L"EditorFileMenu");
		SetDlgItemTextW(window,612,L"&Editor File");
		SetDlgItemTextW(window,613,L"Alt+F3");
		SetDlgItemTextW(window,614,L"EditorFileOpened");
		CheckDlgButton(window,615,BST_UNCHECKED);
		CheckDlgButton(window,616,BST_CHECKED);
		SendMessageW(window,WM_COMMAND,625,0);
		SendMessageW(window,WM_COMMAND,621,0);
		SetDlgItemTextW(window,612,L"Editor child");
		SetDlgItemTextW(window,613,L"Ctrl+Alt+M");
		SetDlgItemTextW(window,614,L"EditorChildClick");
		SendMessageW(window,WM_COMMAND,623,0);
	}
	else
	{
		SendMessageW(window,WM_COMMAND,622,0);
	}
	PostMessageW(window,WM_COMMAND,s_menuDialogAction == 0 ? IDCANCEL : IDOK,0);
	return FALSE;
}
static VOID CALLBACK MenuEditorTimer(HWND owner, UINT, UINT_PTR identity, DWORD)
{
	if(!s_menuDialogVisited)
	{
		EnumThreadWindows(GetCurrentThreadId(),DriveMenuDialog,0);
	}
	if(s_menuDialogVisited)
	{
		KillTimer(owner,identity);
	}
}
static HRESULT MenuEditorWorkflow(QFD::CDesignerEditor* editor, HWND owner, REFGUID fileID)
{
	HRESULT hr;
	TStackRef<IJSONObject> before;
	TStackRef<IJSONObject> after;
	TStackRef<IQuadooComponent> file;
	TStackRef<IQuadooProperties> properties;
	TStackRef<IJSONValue> value;
	CRString name;
	GUID added;
	Check(editor->Document().Serialize(&before));
	for(UINT action = 0; action < 2; action++)
	{
		Check(editor->SelectID(fileID));
		s_menuDialogVisited = FALSE;
		s_menuDialogAction = action;
		CheckIf(!SetTimer(owner,6011,30,MenuEditorTimer),HrEnsureLastError());
		hr = editor->EditMenu();
		KillTimer(owner,6011);
		Check(hr);
		REQUIRE(s_menuDialogVisited && hr == (action ? S_OK : S_FALSE));
		Check(editor->Document().Serialize(&after));
		if(!action)
		{
			REQUIRE(JSONCompareObjects(before,after) == 0);
			after.Release();
		}
	}
	Check(editor->GetSelectedID(&added));
	Check(editor->Document().FindComponent(fileID,&file));
	Check(file->GetName(&name));
	REQUIRE(name == RSTRING_CAST(L"EditorFileMenu"));
	name = NULL;
	Check(QFD::StringProperty(file,L"Caption",&name));
	REQUIRE(name == RSTRING_CAST(L"&Editor File"));
	REQUIRE(!QFD::BoolProperty(file,L"Enabled",TRUE) && QFD::BoolProperty(file,L"Checked"));
	{
		TStackRef<IQuadooComponent> child;
		Check(editor->Document().FindComponent(added,&child));
		name = NULL;
		Check(QFD::StringProperty(child,L"OnClick",&name));
		REQUIRE(name == RSTRING_CAST(L"EditorChildClick"));
	}
	Check(editor->SelectID(added));
	s_menuDialogVisited = FALSE;
	s_menuDialogAction = 2;
	CheckIf(!SetTimer(owner,6011,30,MenuEditorTimer),HrEnsureLastError());
	hr = editor->EditMenu();
	KillTimer(owner,6011);
	Check(hr);
	REQUIRE(s_menuDialogVisited);
	file.Release();
	REQUIRE(FAILED(editor->Document().FindComponent(added,&file)));
Cleanup:
	KillTimer(owner,6011);
	return hr;
}

static BOOL s_menuPreviewSeen;
static BOOL s_menuPreviewValid;
static BOOL CALLBACK CloseMenuPreview(HWND window, LPARAM owner)
{
	WCHAR name[64];
	GetClassNameW(window,name,ARRAYSIZE(name));
	if(!lstrcmpW(name,L"QuadooForm.Native.v2") && GetWindow(window,GW_OWNER) == reinterpret_cast<HWND>(owner))
	{
		RECT client;
		HMENU menu = GetMenu(window);
		s_menuPreviewSeen = TRUE;
		s_menuPreviewValid = IsMenu(menu) && GetMenuItemCount(menu) == 3 && GetClientRect(window,&client) && client.right == 380 && client.bottom == 180;
		if(menu)
		{
			SendMessageW(window,WM_MENUCOMMAND,0,reinterpret_cast<LPARAM>(GetSubMenu(menu,0)));
		}
		PostMessageW(window,WM_CLOSE,0,0);
		return FALSE;
	}
	return TRUE;
}
static VOID CALLBACK MenuPreviewTimer(HWND owner, UINT, UINT_PTR identity, DWORD)
{
	EnumThreadWindows(GetCurrentThreadId(),CloseMenuPreview,reinterpret_cast<LPARAM>(owner));
	if(s_menuPreviewSeen)
	{
		KillTimer(owner,identity);
	}
}
static HRESULT MenuModalPreview(QFD::CDesignerEditor* editor, HWND owner)
{
	HRESULT hr;
	LRESULT result;
	s_menuPreviewSeen = FALSE;
	s_menuPreviewValid = FALSE;
	CheckIf(!SetTimer(owner,6012,30,MenuPreviewTimer),HrEnsureLastError());
	Check(editor->PreviewForm(owner,&result));
	REQUIRE(s_menuPreviewSeen && s_menuPreviewValid);
Cleanup:
	KillTimer(owner,6012);
	return hr;
}
static HRESULT MenuPainting(QFD::CDesignerEditor* editor, REFGUID selected)
{
	HRESULT hr;
	HDC dc = NULL;
	HBITMAP bitmap = NULL;
	HGDIOBJ previous = NULL;
	BITMAPINFO format = {0};
	PVOID pixels;
	QFD::CMenuCells cells;
	INT height;
	format.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	format.bmiHeader.biWidth = 900;
	format.bmiHeader.biHeight = -300;
	format.bmiHeader.biPlanes = 1;
	format.bmiHeader.biBitCount = 32;
	dc = CreateCompatibleDC(NULL);
	CheckIf(!dc,HrEnsureLastError());
	bitmap = CreateDIBSection(dc,&format,DIB_RGB_COLORS,&pixels,NULL,0);
	CheckIf(!bitmap,HrEnsureLastError());
	previous = SelectObject(dc,bitmap);
	for(UINT dpi = 96; dpi <= 192; dpi += 48)
	{
		Check(QFD::LayoutMenuBar(editor->Document().Root(),dpi,160,cells,&height));
		REQUIRE(cells.Length() == 3 && height > 0 && height <= 300);
		RECT bar = {0,0,160,height};
		PatBlt(dc,0,0,900,300,BLACKNESS);
		QFD::PaintMenuBar(editor->Window(),dc,editor->Document().Root(),dpi,&bar,selected);
		REQUIRE(GetPixel(dc,cells[0].bounds.left + 1,cells[0].bounds.top + 1) == GetSysColor(COLOR_HIGHLIGHT));
		REQUIRE(GetPixel(dc,200,height + 1) == RGB(0,0,0));
	}
Cleanup:
	if(previous)
	{
		SelectObject(dc,previous);
	}
	if(bitmap)
	{
		DeleteObject(bitmap);
	}
	if(dc)
	{
		DeleteDC(dc);
	}
	return hr;
}
static HRESULT ReferenceDropdown(QFD::CDesignerEditor* editor, PCWSTR property, UINT count, HWND* combo)
{
	HRESULT hr = S_OK;
	WCHAR type[32];
	*combo = StartInline(editor,property);
	REQUIRE(*combo != NULL);
	GetClassNameW(*combo,type,ARRAYSIZE(type));
	REQUIRE(!lstrcmpiW(type,L"ComboBox"));
	REQUIRE((GetWindowLongW(*combo,GWL_STYLE) & CBS_DROPDOWNLIST) == CBS_DROPDOWNLIST);
	REQUIRE(SendMessageW(*combo,CB_GETCOUNT,0,0) == count);
	REQUIRE(SendMessageW(*combo,CB_FINDSTRINGEXACT,static_cast<WPARAM>(-1),reinterpret_cast<LPARAM>(L"(none)")) == 0);
Cleanup:
	return hr;
}
static HRESULT ReferenceAssignment(IQuadooComponent* component, PCWSTR name, REFGUID expected)
{
	HRESULT hr;
	TStackRef<IQuadooProperties> properties;
	TStackRef<IJSONValue> value;
	GUID actual;
	UINT index;
	Check(QFD::Property(component,name,&properties,&index));
	Check(properties->GetValueJSON(index,&value));
	if(IsEqualGUID(expected,GUID_NULL))
	{
		REQUIRE(!value);
	}
	else
	{
		REQUIRE(value != NULL);
		Check(value->GetStringAsGuid(actual));
		REQUIRE(IsEqualGUID(actual,expected));
	}
Cleanup:
	return hr;
}
static HRESULT DesignerReferences()
{
	HRESULT hr;
	HWND window = Host();
	HWND combo;
	TStackRef<QFD::CDesignerEditor> editor;
	TStackRef<IQuadooComponent> menu;
	TStackRef<IQuadooComponent> item;
	TStackRef<IQuadooFormMenu> assignment;
	TStackRef<IJSONObject> saved;
	TStackRef<IJSONObject> again;
	GUID rootID;
	GUID mainID;
	GUID popupID;
	GUID secondID;
	GUID itemID;
	WCHAR label[80];
	CheckIf(!window,HrEnsureLastError());
	Check(CreateEditor(window,&editor));
	Check(editor->NewDocument(TRUE));
	Check(editor->Document().Root()->GetID(&rootID));
	// Exercise the palette's managed type string, not a static RSTRING literal.
	Check(PickTool(editor,L"MainMenu"));
	Check(editor->GetSelectedID(&mainID));
	REQUIRE(!IsEqualGUID(mainID,rootID));
	Check(ReferenceAssignment(editor->Document().Root(),L"MainMenu",mainID));
	Check(editor->Document().Root()->QueryInterface(__uuidof(IQuadooFormMenu),reinterpret_cast<PVOID*>(&assignment)));
	Check(assignment->GetMainMenu(&menu));
	REQUIRE(menu != NULL);
	Check(menu->GetID(&itemID));
	REQUIRE(IsEqualGUID(itemID,mainID));
	Check(editor->Document().AddMenuItem(menu,&item));
	Check(item->GetID(&itemID));
	Check(PickTool(editor,L"PopupMenu"));
	Check(editor->GetSelectedID(&popupID));
	Check(PickTool(editor,L"PopupMenu"));
	Check(editor->GetSelectedID(&secondID));
	REQUIRE(!IsEqualGUID(secondID,popupID));
	Check(editor->SelectID(rootID));
	PumpPendingMessages();
	editor->Document().SetDirty(FALSE);
	Check(ReferenceDropdown(editor,L"MainMenu",2,&combo));
	REQUIRE(SendMessageW(combo,CB_GETCURSEL,0,0) == 1);
	SendMessageW(combo,CB_GETLBTEXT,1,reinterpret_cast<LPARAM>(label));
	REQUIRE(!lstrcmpW(label,L"MainMenu1"));
	SendMessageW(combo,CB_SETCURSEL,0,0);
	SendMessageW(combo,WM_KEYDOWN,VK_ESCAPE,0);
	Check(ReferenceAssignment(editor->Document().Root(),L"MainMenu",mainID));
	REQUIRE(!editor->Document().Dirty());
	Check(ReferenceDropdown(editor,L"MainMenu",2,&combo));
	SendMessageW(combo,CB_SETCURSEL,0,0);
	SendMessageW(GetParent(combo),WM_COMMAND,MAKEWPARAM(120,CBN_SELENDOK),reinterpret_cast<LPARAM>(combo));
	REQUIRE(!IsWindowVisible(combo) && editor->Document().Dirty());
	Check(ReferenceAssignment(editor->Document().Root(),L"MainMenu",GUID_NULL));
	menu.Release();
	Check(assignment->GetMainMenu(&menu));
	REQUIRE(!menu);
	Check(ReferenceDropdown(editor,L"MainMenu",2,&combo));
	SendMessageW(combo,CB_SETCURSEL,1,0);
	SendMessageW(combo,WM_KEYDOWN,VK_RETURN,0);
	Check(ReferenceAssignment(editor->Document().Root(),L"MainMenu",mainID));
	Check(ReferenceDropdown(editor,L"PopupMenu",3,&combo));
	REQUIRE(SendMessageW(combo,CB_GETCURSEL,0,0) == 0);
	SendMessageW(combo,CB_SETCURSEL,2,0);
	SendMessageW(GetParent(combo),WM_COMMAND,MAKEWPARAM(120,CBN_SELENDOK),reinterpret_cast<LPARAM>(combo));
	Check(ReferenceAssignment(editor->Document().Root(),L"PopupMenu",secondID));
	editor->Document().SetDirty(FALSE);
	Check(ReferenceDropdown(editor,L"PopupMenu",3,&combo));
	SendMessageW(combo,CB_SETCURSEL,1,0);
	SendMessageW(GetParent(combo),WM_COMMAND,MAKEWPARAM(120,CBN_SELENDCANCEL),reinterpret_cast<LPARAM>(combo));
	Check(ReferenceAssignment(editor->Document().Root(),L"PopupMenu",secondID));
	REQUIRE(!editor->Document().Dirty());
	// Switching editor classes must not reuse a hidden combo as a text EDIT.
	combo = StartInline(editor,L"Caption");
	REQUIRE(combo != NULL);
	GetClassNameW(combo,label,ARRAYSIZE(label));
	REQUIRE(!lstrcmpiW(label,L"Edit"));
	SetWindowTextW(combo,L"Reference test");
	SendMessageW(combo,WM_KEYDOWN,VK_RETURN,0);
	Check(ReferenceAssignment(editor->Document().Root(),L"MainMenu",mainID));
	Check(editor->Document().Serialize(&saved));
	Check(editor->LoadDefinition(saved));
	Check(ReferenceAssignment(editor->Document().Root(),L"MainMenu",mainID));
	Check(ReferenceAssignment(editor->Document().Root(),L"PopupMenu",secondID));
	Check(editor->Document().Serialize(&again));
	REQUIRE(JSONCompareObjects(saved,again) == 0);
	// Frames use the same popup picker and do not expose a main-menu assignment.
	Check(editor->NewDocument(FALSE));
	Check(editor->Document().Root()->GetID(&rootID));
	Check(PickTool(editor,L"PopupMenu"));
	Check(editor->GetSelectedID(&popupID));
	Check(editor->SelectID(rootID));
	REQUIRE(InspectorRow(editor,L"MainMenu") < 0);
	Check(ReferenceDropdown(editor,L"PopupMenu",2,&combo));
	SendMessageW(combo,CB_SETCURSEL,1,0);
	SendMessageW(combo,WM_KEYDOWN,VK_RETURN,0);
	Check(ReferenceAssignment(editor->Document().Root(),L"PopupMenu",popupID));
Cleanup:
	editor.Release();
	if(window)
	{
		DestroyWindow(window);
	}
	return hr;
}
static HRESULT DesignerMenus()
{
	HRESULT hr;
	HWND host = Host();
	TStackRef<QFD::CDesignerEditor> editor;
	TStackRef<IJSONObject> definition;
	TStackRef<IJSONObject> saved;
	TStackRef<IJSONObject> again;
	TStackRef<IQuadooComponent> menu;
	TStackRef<IQuadooComponent> file;
	TStackRef<IQuadooComponent> added;
	TStackRef<IQuadooComponentHost> componentHost;
	TStackRef<IQuadooComponentCollection> components;
	TStackRef<IQuadooMenuItems> items;
	TStackRef<IQuadooProperties> properties;
	TStackRef<IJSONValue> value;
	QFD::CMenuCells cells;
	QuadooForm::Rect bounds;
	RECT insets;
	INT wideHeight;
	INT narrowHeight;
	GUID fileID;
	GUID addedID;
	UINT index;
	BOOL foundHierarchy = FALSE;
	POINT point;
	Check(DesignerReferences());
	CheckIf(!host,HrEnsureLastError());
	Check(CreateEditor(host,&editor));
	Check(Parse(MenuTestDefinition,&definition));
	Check(editor->LoadDefinition(definition));
	Check(QFD::AssignedMainMenu(editor->Document().Root(),&menu));
	REQUIRE(menu != NULL);
	Check(editor->Document().Root()->QueryInterface(__uuidof(IQuadooComponentHost),reinterpret_cast<PVOID*>(&componentHost)));
	Check(componentHost->GetComponents(&components));
	Check(components->FindComponent(RSTRING_CAST(L"FileMenu"),&file));
	Check(file->GetID(&fileID));
	Check(MenuPainting(editor,fileID));
	Check(QFD::LayoutMenuBar(editor->Document().Root(),96,900,cells,&wideHeight));
	REQUIRE(cells.Length() == 3 && wideHeight > 0);
	REQUIRE(CRString(cells[0].caption) == RSTRING_CAST(L"&File"));
	cells.Clear();
	Check(QFD::LayoutMenuBar(editor->Document().Root(),96,80,cells,&narrowHeight));
	REQUIRE(narrowHeight >= wideHeight * 3 && cells.Length() == 3);
	QFD::CDesignerWindowFrame::Insets(editor->Document().Root(),TRUE,96,&insets);
	REQUIRE(insets.top > wideHeight);
	Check(editor->Document().Root()->GetBounds(&bounds));
	REQUIRE(bounds.Width == 380 && bounds.Height == 180);
	HWND formWindow;
	{
		TStackRef<IQuadooWinControl> native;
		Check(editor->Document().Root()->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&native)));
		Check(native->GetHandle(&formWindow));
	}
	REQUIRE(GetMenu(formWindow) == NULL);
	INT tray = ListView_GetItemCount(editor->TrayWindow());
	REQUIRE(tray == 2);
	for(UINT n = 0; n < editor->OutlineCount(); n++)
	{
		QFD::OutlineEntry entry;
		Check(editor->GetOutlineEntry(n,&entry));
		if(entry.name == RSTRING_CAST(L"OpenItem"))
		{
			REQUIRE(IsEqualGUID(entry.parent,fileID));
			foundHierarchy = TRUE;
		}
	}
	REQUIRE(foundHierarchy);
	Check(editor->SelectID(fileID));
	RECT selected;
	Check(editor->MenuSelectionRect(&selected));
	point.x = (selected.left + selected.right) / 2;
	point.y = (selected.top + selected.bottom) / 2;
	GUID rootID;
	Check(editor->Document().Root()->GetID(&rootID));
	Check(editor->SelectID(rootID));
	Check(editor->SelectMenuAt(point));
	GUID actual;
	Check(editor->GetSelectedID(&actual));
	REQUIRE(IsEqualGUID(actual,fileID));
	Check(QFD::Property(file,L"Caption",&properties,&index));
	Check(JSONCreateString(RSTRING_CAST(L"&Renamed"),&value));
	Check(editor->SetSelectedProperty(index,value));
	cells.Clear();
	Check(QFD::LayoutMenuBar(editor->Document().Root(),96,380,cells,&wideHeight));
	REQUIRE(CRString(cells[0].caption) == RSTRING_CAST(L"&Renamed"));
	value.Release();
	Check(JSONCreateBool(false,&value));
	properties.Release();
	Check(QFD::Property(file,L"Visible",&properties,&index));
	Check(editor->SetSelectedProperty(index,value));
	cells.Clear();
	Check(QFD::LayoutMenuBar(editor->Document().Root(),96,380,cells,&wideHeight));
	REQUIRE(cells.Length() == 2);
	value.Release();
	Check(JSONCreateBool(true,&value));
	Check(editor->SetSelectedProperty(index,value));
	Check(editor->Document().AddMenuItem(file,&added));
	Check(added->GetID(&addedID));
	Check(added->QueryInterface(__uuidof(IQuadooMenuItems),reinterpret_cast<PVOID*>(&items)));
	Check(editor->Document().MoveMenuItem(added,FALSE));
	added.Release();
	file.Release();
	Check(editor->Document().FindComponent(addedID,&added));
	Check(editor->Document().FindComponent(fileID,&file));
	Check(editor->Document().Serialize(&saved));
	Check(editor->LoadDefinition(saved));
	Check(editor->Document().Serialize(&again));
	REQUIRE(JSONCompareObjects(saved,again) == 0);
	added.Release();
	Check(editor->Document().FindComponent(addedID,&added));
	Check(editor->Document().Remove(added));
	added.Release();
	REQUIRE(FAILED(editor->Document().FindComponent(addedID,&added)));
	REQUIRE(editor->CanExecute(QFD::MenuEditor));
	Check(MenuModalPreview(editor,host));
	Check(MenuEditorWorkflow(editor,host,fileID));
	{
		TStackRef<IQuadooComponent> popup;
		TStackRef<IQuadooComponent> pasted;
		TStackRef<IJSONValue> clipboard;
		TStackRef<IQuadooMenuItems> children;
		TStackRef<IQuadooMenuItem> childItem;
		GUID popupID;
		GUID copyID;
		GUID parentID;
		components.Release();
		componentHost.Release();
		Check(editor->Document().Root()->QueryInterface(__uuidof(IQuadooComponentHost),reinterpret_cast<PVOID*>(&componentHost)));
		Check(componentHost->GetComponents(&components));
		Check(components->FindComponent(RSTRING_CAST(L"PopupMenu1"),&popup));
		Check(popup->GetID(&popupID));
		Check(editor->Document().CopySelectionDefinition(popupID,&clipboard));
		REQUIRE(clipboard->GetType() == JSON::Array);
		Check(editor->Document().PasteDefinition(popupID,clipboard,&copyID));
		REQUIRE(!IsEqualGUID(copyID,popupID));
		Check(editor->Document().FindComponent(copyID,&pasted));
		Check(pasted->QueryInterface(__uuidof(IQuadooMenuItems),reinterpret_cast<PVOID*>(&children)));
		UINT count;
		Check(children->GetItemCount(&count));
		REQUIRE(count == 1);
		popup.Release();
		Check(children->GetItem(0,&popup));
		Check(popup->QueryInterface(__uuidof(IQuadooMenuItem),reinterpret_cast<PVOID*>(&childItem)));
		Check(childItem->GetMenuParentID(&parentID));
		REQUIRE(IsEqualGUID(parentID,copyID));
	}
Cleanup:
	editor.Release();
	if(host)
	{
		DestroyWindow(host);
	}
	return hr;
}
