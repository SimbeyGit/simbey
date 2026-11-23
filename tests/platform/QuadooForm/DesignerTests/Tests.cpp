#include "..\..\..\..\platform\QuadooForm\Designer\Designer.h"
#include "..\..\..\TestPaths.h"
#include "..\TestExtension\ExternalControl.h"
#include "Library\Util\TextHelpers.h"
#include <oleacc.h>
#pragma comment(lib,"oleacc.lib")

#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

static UINT g_checks;
static VOID Log(PCWSTR text)
{
	CHAR buffer[4096]; DWORD written; INT count = WideCharToMultiByte(CP_UTF8,0,text,-1,buffer,sizeof(buffer),NULL,NULL);
	if(count) WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),buffer,count - 1,&written,NULL);
}
static BOOL Expect(BOOL condition, PCWSTR text, UINT line)
{
	g_checks++; if(!condition) { CRString message; if(SUCCEEDED(RStrFormatW(&message,L"FAILED line %u: %ls\r\n",line,text))) Log(message.c_wstr()); } return condition;
}
#define WIDE_LITERAL_(text) L##text
#define WIDE_LITERAL(text) WIDE_LITERAL_(text)
#define REQUIRE(condition) CheckIf(!Expect(!!(condition),WIDE_LITERAL(#condition),__LINE__),E_FAIL)
static BOOL PumpPendingMessages()
{
	MSG message;
	while(PeekMessageW(&message,NULL,0,0,PM_REMOVE)) { if(message.message == WM_QUIT) return TRUE; TranslateMessage(&message); DispatchMessageW(&message); }
	return FALSE;
}
#include "..\Harness\GuiResourceTests.h"
#include "..\Harness\TestIcon.h"
static BOOL EqualBounds(const QuadooForm::Rect& a, const QuadooForm::Rect& b) { return !memcmp(&a,&b,sizeof(a)); }
static HRESULT Parse(PCWSTR text, IJSONObject** object) { return QFD::Parse(text,TStrLenAssert(text),object); }
static HWND Host()
{
	HWND host = CreateWindowExW(0,L"STATIC",L"QuadooForm Designer tests",WS_OVERLAPPEDWINDOW | WS_VISIBLE,10,10,1250,800,NULL,NULL,GetModuleHandle(NULL),NULL);
	// Hidden launcher startup information may suppress the first window even
	// with WS_VISIBLE. Hit-testing tests require a visible host and children.
	if(host)
	{
		ShowWindow(host,SW_SHOWNOACTIVATE);
	}
	return host;
}
static HRESULT CreateEditor(HWND host, QFD::CDesignerEditor** editor, IQuadooControlRegistry* registry = NULL)
{
	HRESULT hr;
	CheckIf(!editor,E_POINTER); *editor = __new QFD::CDesignerEditor; CheckAlloc(*editor);
	Check((*editor)->Create(host,registry)); CheckIf(!MoveWindow((*editor)->Window(),0,0,1230,720,TRUE),HrEnsureLastError());
Cleanup:
	return hr;
}
static INT FindTool(HWND list, PCWSTR text)
{
	LVFINDINFOW find; ZeroMemory(&find,sizeof(find)); find.flags = LVFI_STRING; find.psz = text;
	return ListView_FindItem(list,-1,&find);
}
static HRESULT PickTool(QFD::CDesignerEditor* editor, PCWSTR text)
{
	HRESULT hr = S_OK;
	HWND list = GetDlgItem(editor->Window(),102);
	INT row = FindTool(list,text);
	CheckIf(row < 0,E_FAIL);
	ListView_SetItemState(list,-1,0,LVIS_SELECTED | LVIS_FOCUSED);
	ListView_SetItemState(list,row,LVIS_SELECTED | LVIS_FOCUSED,LVIS_SELECTED | LVIS_FOCUSED);
Cleanup:
	return hr;
}
static HRESULT Edit(QFD::CDesignerEditor* editor, IQuadooControl* control, PCWSTR name, IJSONValue* value)
{
	HRESULT hr; TStackRef<IQuadooProperties> properties; UINT index; GUID id;
	Check(control->GetID(&id)); Check(editor->SelectID(id)); Check(QFD::Property(control,name,&properties,&index)); Check(editor->SetSelectedProperty(index,value));
Cleanup:
	return hr;
}
static HRESULT NewTemporary(WCHAR* path, UINT count)
{
	HRESULT hr;
	WCHAR directory[MAX_PATH];
	Check(TestScratchDirectory(L"QuadooFormDesigner",directory,ARRAYSIZE(directory)));
	CheckIf(!GetTempFileNameW(directory,L"QFD",0,path),HrEnsureLastError());
Cleanup:
	return hr;
}
static HRESULT DocumentWorkflows()
{
	HRESULT hr;
	HWND host = Host(); TStackRef<QFD::CDesignerEditor> editor;
	TStackRef<IQuadooControlContainer> root, panel; TStackRef<IQuadooControl> control, button;
	TStackRef<IJSONObject> before, after; TStackRef<IJSONValue> value; TStackRef<IQuadooProperties> properties;
	QuadooForm::Rect bounds = { 12,16,240,150 }; GUID rootID, panelID, buttonID, selected, loadedID;
	WCHAR path[MAX_PATH] = { 0 }, bad[MAX_PATH] = { 0 }; UINT index;
	CheckIf(!host,HrEnsureLastError()); Check(CreateEditor(host,&editor)); Check(editor->NewDocument(TRUE));
	Check(editor->Document().Root()->GetID(&rootID)); REQUIRE(!IsEqualGUID(rootID,GUID_NULL) && editor->Document().IsForm());
	Check(editor->Document().Root()->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&root)));
	Check(editor->AddControl(RSTRING_CAST(L"Panel"),root,&bounds,&panelID)); Check(editor->Document().Find(panelID,&control)); Check(control->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&panel))); control.Release();
	bounds.Left = 10; bounds.Top = 25; bounds.Width = 110; bounds.Height = 28;
	Check(editor->AddControl(RSTRING_CAST(L"Button"),panel,&bounds,&buttonID)); Check(editor->Document().Find(buttonID,&button)); Check(editor->GetSelectedID(&selected)); REQUIRE(IsEqualGUID(selected,buttonID));
	REQUIRE(TreeView_GetCount(editor->TreeWindow()) == 3);
	Check(JSONCreateString(RSTRING_CAST(L"R\x00e9sum\x00e9 \x65e5\x672c\x8a9e \x03a9"),&value)); Check(Edit(editor,button,L"Caption",value)); value.Release();
	Check(JSONCreateStringW(L"MyButton_Click",14,&value)); Check(Edit(editor,button,L"OnClick",value)); value.Release();
	Check(TestIconValue(&value)); Check(Edit(editor,root,L"Icon",value)); value.Release();
	Check(NewTemporary(path,ARRAYSIZE(path))); Check(editor->SaveDocument(path)); REQUIRE(!editor->Document().Dirty() && editor->Document().Path());
	Check(editor->Document().Serialize(&before));
	Check(JSONCreateStringW(L"Panel1",6,&value)); REQUIRE(Edit(editor,button,L"Name",value) == QuadooForm::EDuplicateName); value.Release();
	Check(editor->Document().Serialize(&after)); REQUIRE(JSONCompareObjects(before,after) == 0 && !editor->Document().Dirty()); after.Release();
	Check(NewTemporary(bad,ARRAYSIZE(bad))); Check(Text::SaveToFile(L"{ broken",8,bad)); REQUIRE(FAILED(editor->OpenDocument(bad)));
	Check(editor->Document().Serialize(&after)); REQUIRE(JSONCompareObjects(before,after) == 0); after.Release();
	{ TStackRef<IJSONObject> invalid; Check(Parse(L"{\"type\":\"Form\",\"name\":\"FailedCandidate\",\"children\":[{\"type\":\"Button\",\"name\":\"invalid\",\"properties\":{\"Width\":\"bad\"}}]}",&invalid)); REQUIRE(FAILED(editor->LoadDefinition(invalid))); Check(editor->Document().Serialize(&after)); REQUIRE(JSONCompareObjects(before,after) == 0); after.Release(); REQUIRE(TreeView_GetCount(editor->TreeWindow()) == 3); }
	Check(editor->OpenDocument(path)); Check(editor->Document().Root()->GetID(&loadedID)); REQUIRE(IsEqualGUID(rootID,loadedID)); Check(editor->Document().Serialize(&after)); REQUIRE(JSONCompareObjects(before,after) == 0); after.Release();
	button.Release(); panel.Release(); root.Release(); Check(editor->Document().Find(buttonID,&button));
	Check(JSONCreateInteger(77,&value)); Check(Edit(editor,button,L"Left",value)); value.Release(); REQUIRE(editor->Document().Dirty());
	REQUIRE(FAILED(editor->SaveDocument(L"Z:\\QuadooFormMissingDirectory\\cannot-save.json")) && editor->Document().Dirty());
	Check(editor->SelectID(buttonID)); Check(editor->Delete()); REQUIRE(editor->Document().Find(buttonID,&control) == HRESULT_FROM_WIN32(ERROR_NOT_FOUND)); REQUIRE(TreeView_GetCount(editor->TreeWindow()) == 2);
	Check(editor->NewDocument(FALSE)); REQUIRE(!editor->Document().IsForm()); Check(editor->Document().Root()->GetID(&loadedID)); REQUIRE(!IsEqualGUID(rootID,loadedID));
	{ RECT insets; QFD::CDesignerWindowFrame::Insets(editor->Document().Root(),FALSE,144,&insets); REQUIRE(insets.left == 0 && insets.top == 0 && insets.right == 0 && insets.bottom == 0); }
	{ LRESULT result; REQUIRE(editor->PreviewForm(host,&result) == E_INVALIDARG); }
	REQUIRE(QFD::Property(editor->Document().Root(),L"Icon",&properties,&index) == DISP_E_MEMBERNOTFOUND);
	{
		WCHAR example[MAX_PATH]; DWORD length = GetModuleFileNameW(NULL,example,ARRAYSIZE(example)); CheckIf(!length || length >= ARRAYSIZE(example),E_INVALIDARG);
		while(length && example[length - 1] != L'\\') length--;
		Check(TStrCchCpy(example + length,ARRAYSIZE(example) - length,L"DesignerExamples\\Settings.qform.json")); Check(editor->OpenDocument(example)); REQUIRE(editor->Document().IsForm() && !editor->Document().Dirty());
		Check(editor->Document().Root()->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&root))); Check(root->FindControl(RSTRING_CAST(L"nameEdit"),&control)); CRString text; Check(QFD::StringProperty(control,L"Text",&text)); REQUIRE(text == RSTRING_CAST(L"L\x00e9" L"a")); control.Release(); root.Release();
		Check(TStrCchCpy(example + length,ARRAYSIZE(example) - length,L"DesignerExamples\\Profile.qframe.json")); Check(editor->OpenDocument(example)); REQUIRE(!editor->Document().IsForm() && !editor->Document().Dirty());
	}
Cleanup:
	if(host) DestroyWindow(host); editor.Release(); if(path[0]) DeleteFileW(path); if(bad[0]) DeleteFileW(bad);
	return hr;
}
static const WCHAR TabDocument[] =
	L"{\"type\":\"Form\",\"name\":\"TabEditor\",\"properties\":{\"Width\":480,\"Height\":310},\"children\":[{\"type\":\"TabControl\",\"name\":\"tabs\",\"properties\":{\"Align\":\"Client\",\"ActivePageIndex\":0},\"children\":["
	L"{\"type\":\"TabPage\",\"name\":\"pageOne\",\"properties\":{\"Caption\":\"General\"}},{\"type\":\"TabPage\",\"name\":\"pageTwo\",\"properties\":{\"Caption\":\"Advanced\"},\"children\":[{\"type\":\"TextBox\",\"name\":\"entry\",\"properties\":{\"Left\":20,\"Top\":30,\"Width\":120}},{\"type\":\"ComboBox\",\"name\":\"combo\",\"properties\":{\"Left\":20,\"Top\":90}},{\"type\":\"Label\",\"name\":\"label\",\"properties\":{\"AutoSize\":true,\"Caption\":\"Auto size\",\"Top\":140}}]}]}]}";
static HTREEITEM FindTree(HWND tree, HTREEITEM item, PCWSTR prefix)
{
	for(; item; item = TreeView_GetNextSibling(tree,item))
	{
		WCHAR name[256]; TVITEMW data; ZeroMemory(&data,sizeof(data)); data.mask = TVIF_TEXT; data.hItem = item; data.pszText = name; data.cchTextMax = ARRAYSIZE(name);
		if(TreeView_GetItem(tree,&data) && lstrlenW(name) >= lstrlenW(prefix) && CompareStringW(LOCALE_INVARIANT,0,name,lstrlenW(prefix),prefix,lstrlenW(prefix)) == CSTR_EQUAL) return item;
		HTREEITEM child = FindTree(tree,TreeView_GetChild(tree,item),prefix); if(child) return child;
	}
	return NULL;
}
static VOID GripPoint(const RECT* rect, UINT grip, POINT* point)
{
	point->x = grip == 1 || grip == 6 ? (rect->left + rect->right) / 2 : grip == 0 || grip == 3 || grip == 5 ? rect->left : rect->right;
	point->y = grip < 3 ? rect->top : grip > 4 ? rect->bottom : (rect->top + rect->bottom) / 2;
}
static HRESULT GripGestures(QFD::CDesignerEditor* editor, IQuadooControl* entry)
{
	HRESULT hr; GUID id; QuadooForm::Rect initial, actual, expected; RECT rect; POINT point;
	HDC dc = NULL; HBITMAP bitmap = NULL; HGDIOBJ previous = NULL;
	Check(entry->GetID(&id)); Check(editor->SelectID(id)); Check(entry->GetBounds(&initial));
	dc = CreateCompatibleDC(NULL); CheckIf(!dc,HrEnsureLastError());
	{ HDC screen = GetDC(editor->CanvasWindow()); bitmap = CreateCompatibleBitmap(screen,1600,1600); ReleaseDC(editor->CanvasWindow(),screen); } CheckIf(!bitmap,HrEnsureLastError()); previous = SelectObject(dc,bitmap);
	for(UINT zoom = 50; zoom <= 200; zoom += 50)
	{
		editor->SetZoom(zoom); Check(editor->SelectID(id));
		for(UINT grip = 0; grip < 8; grip++)
		{
			Check(editor->SelectionRect(&rect)); GripPoint(&rect,grip,&point);
			SendMessageW(editor->OverlayWindow(),WM_PRINTCLIENT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT);
			REQUIRE(GetPixel(dc,point.x,point.y) == RGB(30,100,210));
			SendMessageW(editor->OverlayWindow(),WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(point.x,point.y)); REQUIRE(GetCapture() == editor->OverlayWindow());
			point.x += MulDiv(8,zoom,100); point.y += MulDiv(8,zoom,100);
			SendMessageW(editor->OverlayWindow(),WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(point.x,point.y)); SendMessageW(editor->OverlayWindow(),WM_LBUTTONUP,0,MAKELPARAM(point.x,point.y));
			expected = initial;
			if(grip == 0 || grip == 3 || grip == 5) { expected.Left += 8; expected.Width -= 8; }
			if(grip == 2 || grip == 4 || grip == 7) expected.Width += 8;
			if(grip < 3) { expected.Top += 8; expected.Height -= 8; }
			if(grip > 4) expected.Height += 8;
			Check(entry->GetBounds(&actual)); REQUIRE(EqualBounds(expected,actual)); Check(editor->SetSelectionBounds(&initial));
		}
	}
	editor->SetZoom(100);
Cleanup:
	if(previous) SelectObject(dc,previous); if(bitmap) DeleteObject(bitmap); if(dc) DeleteDC(dc);
	return hr;
}
static HRESULT SavedActive(QFD::CDesignerEditor* editor, INT* index)
{
	HRESULT hr; TStackRef<IJSONObject> definition, tabs, properties; TStackRef<IJSONValue> value; TStackRef<IJSONArray> children;
	Check(editor->Document().Serialize(&definition)); Check(definition->FindNonNullValueW(L"children",&value)); Check(value->GetArray(&children)); value.Release(); Check(children->GetObject(0,&tabs)); Check(tabs->FindNonNullValueW(L"properties",&value)); Check(value->GetObject(&properties)); value.Release(); Check(properties->FindNonNullValueW(L"ActivePageIndex",&value)); Check(value->GetInteger(index));
Cleanup:
	return hr;
}
static HRESULT SelectionAndGeometry()
{
	HRESULT hr; HWND host = Host(); TStackRef<QFD::CDesignerEditor> editor;
	TStackRef<IJSONObject> definition, before, after; TStackRef<IQuadooControlContainer> root; TStackRef<IQuadooControl> entry, combo, tabsControl, label;
	TStackRef<IQuadooTabControl> tabs; GUID id, selected; DWORD flags; INT active; QuadooForm::Rect requested, changed; RECT rect;
	CheckIf(!host,HrEnsureLastError()); Check(CreateEditor(host,&editor)); Check(Parse(TabDocument,&definition)); Check(editor->LoadDefinition(definition));
	Check(editor->Document().Root()->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&root))); Check(root->FindControl(RSTRING_CAST(L"entry"),&entry)); Check(root->FindControl(RSTRING_CAST(L"combo"),&combo)); Check(root->FindControl(RSTRING_CAST(L"label"),&label)); Check(root->FindControl(RSTRING_CAST(L"tabs"),&tabsControl)); Check(tabsControl->QueryInterface(__uuidof(IQuadooTabControl),reinterpret_cast<PVOID*>(&tabs)));
	Check(editor->Document().Serialize(&before)); Check(entry->GetID(&id)); Check(editor->SelectID(id)); Check(editor->GetSelectedID(&selected)); REQUIRE(IsEqualGUID(id,selected)); Check(tabs->GetActivePageIndex(&active)); REQUIRE(active == 1 && !editor->Document().Dirty());
	Check(editor->Document().Serialize(&after)); REQUIRE(JSONCompareObjects(before,after) == 0); after.Release();
	{ HTREEITEM item = FindTree(editor->TreeWindow(),TreeView_GetRoot(editor->TreeWindow()),L"combo ("); REQUIRE(item != NULL); TreeView_SelectItem(editor->TreeWindow(),item); Check(combo->GetID(&selected)); Check(editor->GetSelectedID(&id)); REQUIRE(IsEqualGUID(id,selected)); Check(entry->GetID(&id)); Check(editor->SelectID(id)); }
	for(UINT dpi = 96; dpi <= 192; dpi += 48)
	{
		editor->SetDPI(dpi);
		for(UINT zoom = 50; zoom <= 200; zoom += 50)
		{
			editor->SetZoom(zoom); Check(editor->SelectID(id)); Check(editor->SelectionRect(&rect)); REQUIRE(abs((rect.right - rect.left) - MulDiv(120,MulDiv(dpi,zoom,100),96)) <= 1);
			Check(editor->Document().Serialize(&after)); REQUIRE(JSONCompareObjects(before,after) == 0); after.Release();
		}
	}
	editor->SetDPI(96); editor->SetZoom(100); Check(editor->SelectID(id)); Check(GripGestures(editor,entry)); Check(entry->GetBounds(&requested)); changed = requested; changed.Width += 30; changed.Height += 10; Check(editor->SetSelectionBounds(&changed)); Check(entry->GetBounds(&requested)); REQUIRE(EqualBounds(requested,changed));
	Check(combo->GetID(&id)); Check(editor->SelectID(id)); Check(editor->SelectionCapabilities(&flags)); REQUIRE((flags & QuadooForm::DesignResizeWidth) && !(flags & QuadooForm::DesignResizeHeight)); Check(combo->GetBounds(&requested)); changed = requested; changed.Height += 5; REQUIRE(editor->SetSelectionBounds(&changed) == E_ACCESSDENIED); Check(combo->GetBounds(&changed)); REQUIRE(EqualBounds(requested,changed));
	{ POINT point; Check(editor->SelectionRect(&rect)); GripPoint(&rect,1,&point); SendMessageW(editor->OverlayWindow(),WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(point.x,point.y)); REQUIRE(GetCapture() != editor->OverlayWindow()); Check(combo->GetBounds(&changed)); REQUIRE(EqualBounds(requested,changed)); }
	Check(label->GetID(&id)); Check(editor->SelectID(id)); Check(editor->SelectionCapabilities(&flags)); REQUIRE(!(flags & (QuadooForm::DesignResizeWidth | QuadooForm::DesignResizeHeight)));
	{
		TStackRef<IJSONValue> value; Check(JSONCreateInteger(QuadooForm::AlignTop,&value)); Check(Edit(editor,entry,L"Align",value)); value.Release(); Check(entry->GetID(&id)); Check(editor->SelectID(id)); Check(editor->SelectionCapabilities(&flags)); REQUIRE(flags == QuadooForm::DesignResizeHeight);
		Check(entry->GetBounds(&requested)); Check(editor->SelectionRect(&rect)); POINT point = { (rect.left + rect.right) / 2,(rect.top + rect.bottom) / 2 }; BOOL dirty = editor->Document().Dirty();
		SendMessageW(editor->OverlayWindow(),WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(point.x,point.y)); SendMessageW(editor->OverlayWindow(),WM_LBUTTONUP,0,MAKELPARAM(point.x,point.y)); Check(entry->GetBounds(&changed)); REQUIRE(EqualBounds(requested,changed) && editor->Document().Dirty() == dirty);
		Check(JSONCreateInteger(35,&value)); Check(Edit(editor,entry,L"MinHeight",value)); value.Release(); Check(entry->GetBounds(&requested)); changed = requested; changed.Height = 34; REQUIRE(editor->SetSelectionBounds(&changed) == E_INVALIDARG);
	}
	{
		TStackRef<IQuadooControlContainer> container; TStackRef<IQuadooControl> third; QuadooForm::Rect bounds = { 0,0,0,0 }; GUID thirdID; INT saved;
		Check(tabsControl->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&container))); Check(editor->AddControl(RSTRING_CAST(L"TabPage"),container,&bounds,&thirdID));
		Check(editor->Document().Find(thirdID,&third)); Check(editor->Delete()); Check(SavedActive(editor,&saved)); REQUIRE(saved == 0);
		UINT count; Check(container->GetChildCount(&count)); REQUIRE(count == 2); REQUIRE(editor->AddControl(RSTRING_CAST(L"Button"),container,&bounds,&thirdID) == E_INVALIDARG); Check(container->GetChildCount(&count)); REQUIRE(count == 2); Check(SavedActive(editor,&saved)); REQUIRE(saved == 0);
	}
Cleanup:
	if(host) DestroyWindow(host); editor.Release();
	return hr;
}
static HRESULT ClickPageHeader(QFD::CDesignerEditor* editor, IQuadooTabControl* tabs, INT index)
{
	HRESULT hr;
	HWND window; RECT rect; POINT point;
	Check(tabs->GetHandle(&window)); CheckIf(!TabCtrl_GetItemRect(window,index,&rect),E_FAIL);
	point.x = (rect.left + rect.right) / 2; point.y = (rect.top + rect.bottom) / 2;
	MapWindowPoints(window,editor->CanvasWindow(),&point,1);
	SendMessageW(editor->OverlayWindow(),WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(point.x,point.y));
	SendMessageW(editor->OverlayWindow(),WM_LBUTTONUP,0,MAKELPARAM(point.x,point.y));
	CheckIf(PumpPendingMessages(),E_ABORT);
Cleanup:
	return hr;
}
static HRESULT PlacePageControl(QFD::CDesignerEditor* editor, IQuadooTabPage* page, PCWSTR type, BOOL drag)
{
	HRESULT hr = S_OK;
	TStackRef<IQuadooChildWindowHost> host; HWND window; POINT point = { 32,40 }, end;
	Check(page->QueryInterface(__uuidof(IQuadooChildWindowHost),reinterpret_cast<PVOID*>(&host))); Check(host->GetChildHostHandle(&window));
	MapWindowPoints(window,editor->CanvasWindow(),&point,1); end = point;
	if(drag) { end.x += 80; end.y += 24; }
	Check(PickTool(editor,type));
	SendMessageW(editor->OverlayWindow(),WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(point.x,point.y));
	if(drag) SendMessageW(editor->OverlayWindow(),WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(end.x,end.y));
	SendMessageW(editor->OverlayWindow(),WM_LBUTTONUP,0,MAKELPARAM(end.x,end.y));
	CheckIf(PumpPendingMessages(),E_ABORT);
Cleanup:
	return hr;
}
static HRESULT TabPagePlacement()
{
	HRESULT hr;
	HWND host = Host(); TStackRef<QFD::CDesignerEditor> editor;
	CheckIf(!host,HrEnsureLastError()); Check(CreateEditor(host,&editor));
	for(UINT form = 0; form < 2; form++)
	{
		TStackRef<IQuadooControlContainer> root, container, actualParent; TStackRef<IQuadooControl> control, button;
		TStackRef<IQuadooTabControl> tabs; TStackRef<IQuadooTabPage> first, second;
		QuadooForm::Rect bounds = { 16,16,300,180 }; GUID tabsID, firstID, secondID, selected; INT active, saved; UINT count;
		Check(editor->NewDocument(form != 0)); Check(editor->Document().Root()->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&root)));
		Check(editor->AddControl(RSTRING_CAST(L"TabControl"),root,&bounds,&tabsID)); Check(editor->Document().Find(tabsID,&control));
		Check(control->QueryInterface(__uuidof(IQuadooTabControl),reinterpret_cast<PVOID*>(&tabs))); Check(control->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&container))); control.Release();
		bounds.Left = bounds.Top = bounds.Width = bounds.Height = 0;
		Check(editor->AddControl(RSTRING_CAST(L"TabPage"),container,&bounds,&firstID));
		Check(editor->AddControl(RSTRING_CAST(L"TabPage"),container,&bounds,&secondID));
		Check(tabs->GetPage(0,&first)); Check(tabs->GetPage(1,&second)); Check(tabs->GetActivePageIndex(&active)); REQUIRE(active == 1);
		for(UINT scale = 0; scale < 3; scale++)
		{
			editor->SetDPI(scale == 1 ? 144 : 96); editor->SetZoom(scale == 2 ? 150 : 100);
			Check(ClickPageHeader(editor,tabs,0)); Check(tabs->GetActivePageIndex(&active)); REQUIRE(active == 0);
			Check(editor->GetSelectedID(&selected)); REQUIRE(IsEqualGUID(selected,firstID));
			{
				BOOL visible; HWND window;
				Check(second->GetVisible(&visible)); Check(second->GetHandle(&window)); REQUIRE(visible && !IsWindowVisible(window));
			}
			Check(PlacePageControl(editor,first,scale == 1 ? L"Label" : L"Button",scale == 2));
			Check(tabs->GetActivePageIndex(&active)); Check(first->GetChildCount(&count)); REQUIRE(active == 0 && count == 1);
			Check(second->GetChildCount(&count)); REQUIRE(count == 0);
			Check(first->GetChild(0,&button)); Check(button->GetParent(&actualParent)); REQUIRE(QFD::SameObject(actualParent,first)); actualParent.Release();
			Check(button->GetID(&selected)); GUID selection; Check(editor->GetSelectedID(&selection)); REQUIRE(IsEqualGUID(selected,selection));
			Check(editor->Delete()); button.Release();
			{
				HWND window; POINT point = { 140,100 };
				Check(first->GetHandle(&window)); MapWindowPoints(window,editor->CanvasWindow(),&point,1);
				// Pointer selection must also ignore the inactive page's overlapping rectangle.
				SendMessageW(editor->OverlayWindow(),WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(point.x,point.y)); SendMessageW(editor->OverlayWindow(),WM_LBUTTONUP,0,MAKELPARAM(point.x,point.y));
				Check(editor->GetSelectedID(&selected)); REQUIRE(IsEqualGUID(selected,firstID));
			}
			Check(ClickPageHeader(editor,tabs,1)); Check(PlacePageControl(editor,second,L"Button",FALSE));
			Check(tabs->GetActivePageIndex(&active)); Check(second->GetChildCount(&count)); REQUIRE(active == 1 && count == 1);
			Check(first->GetChildCount(&count)); REQUIRE(count == 0);
			// Keep a child on the inactive page while adding into the first page again.
			Check(ClickPageHeader(editor,tabs,0)); Check(PlacePageControl(editor,first,L"Button",FALSE));
			Check(tabs->GetActivePageIndex(&active)); Check(first->GetChildCount(&count)); REQUIRE(active == 0 && count == 1);
			Check(SavedActive(editor,&saved)); REQUIRE(saved == 0);
			Check(editor->Delete()); Check(ClickPageHeader(editor,tabs,1)); Check(second->GetChild(0,&button)); Check(button->GetID(&selected)); Check(editor->SelectID(selected)); Check(editor->Delete()); button.Release();
		}
	}
Cleanup:
	if(host) DestroyWindow(host); editor.Release();
	return hr;
}
static WNDPROC g_canvasProcedure;
static UINT g_canvasPaints;
static LRESULT CALLBACK CountCanvasPaints(HWND window, UINT msg, WPARAM wp, LPARAM lp)
{
	if(msg == WM_PAINT) g_canvasPaints++;
	return CallWindowProcW(g_canvasProcedure,window,msg,wp,lp);
}
static HRESULT PaintingAndResize()
{
	HRESULT hr; HWND host = Host(); TStackRef<QFD::CDesignerEditor> editor;
	TStackRef<IJSONObject> definition, before, after; TStackRef<IQuadooControlContainer> root; TStackRef<IQuadooControl> entry;
	HDC dc = NULL; HBITMAP bitmap = NULL; HGDIOBJ previous = NULL; GUID id; RECT rect; POINT point;
	CheckIf(!host,HrEnsureLastError()); Check(CreateEditor(host,&editor)); Check(Parse(TabDocument,&definition)); Check(editor->LoadDefinition(definition));
	Check(editor->Document().Root()->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&root))); Check(root->FindControl(RSTRING_CAST(L"entry"),&entry)); Check(entry->GetID(&id)); Check(editor->SelectID(id)); Check(editor->Document().Serialize(&before));
	dc = CreateCompatibleDC(NULL); CheckIf(!dc,HrEnsureLastError());
	{ HDC screen = GetDC(editor->Window()); bitmap = CreateCompatibleBitmap(screen,1600,1000); ReleaseDC(editor->Window(),screen); } CheckIf(!bitmap,HrEnsureLastError()); previous = SelectObject(dc,bitmap);
	for(UINT n = 0; n < 8; n++)
	{
		CheckIf(!MoveWindow(editor->Window(),0,0,n & 1 ? 1230 : 870,n & 1 ? 720 : 510,TRUE),HrEnsureLastError());
		CheckIf(PumpPendingMessages(),E_ABORT);
		RECT canvas, list, tabs; GetWindowRect(editor->CanvasWindow(),&canvas); GetWindowRect(GetDlgItem(editor->Window(),104),&list); GetWindowRect(GetDlgItem(editor->Window(),103),&tabs);
		REQUIRE(canvas.right <= list.left && canvas.right <= tabs.left);
		// Recreate stale pixels in a vacated pane area. The editor must paint its
		// own exposed background, including the strip beside the inspector.
		SetDCBrushColor(dc,RGB(255,0,255)); RECT client; GetClientRect(editor->Window(),&client); FillRect(dc,&client,static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
		SendMessageW(editor->Window(),WM_PRINTCLIENT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT);
		POINT gap = { canvas.right + 1,canvas.top + 100 }; ScreenToClient(editor->Window(),&gap);
		REQUIRE(GetPixel(dc,gap.x,gap.y) == GetSysColor(COLOR_BTNFACE));
		GUID selected; Check(editor->GetSelectedID(&selected)); REQUIRE(IsEqualGUID(id,selected) && !editor->Document().Dirty());
	}
	Check(editor->SelectionRect(&rect)); point.x = (rect.left + rect.right) / 2; point.y = (rect.top + rect.bottom) / 2;
	SendMessageW(editor->OverlayWindow(),WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(point.x,point.y)); CheckIf(PumpPendingMessages(),E_ABORT);
	g_canvasProcedure = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(editor->CanvasWindow(),GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(CountCanvasPaints))); CheckIf(!g_canvasProcedure,HrEnsureLastError()); g_canvasPaints = 0;
	for(UINT n = 1; n <= 20; n++) SendMessageW(editor->OverlayWindow(),WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(point.x + n,point.y + n));
	REQUIRE(g_canvasPaints == 0); CheckIf(PumpPendingMessages(),E_ABORT); REQUIRE(g_canvasPaints > 0 && g_canvasPaints <= 2);
	SendMessageW(editor->OverlayWindow(),WM_KEYDOWN,VK_ESCAPE,0); REQUIRE(GetCapture() != editor->OverlayWindow());
	Check(editor->Document().Serialize(&after)); REQUIRE(JSONCompareObjects(before,after) == 0 && !editor->Document().Dirty());
Cleanup:
	if(g_canvasProcedure) { SetWindowLongPtrW(editor->CanvasWindow(),GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(g_canvasProcedure)); g_canvasProcedure = NULL; }
	if(previous) SelectObject(dc,previous); if(bitmap) DeleteObject(bitmap); if(dc) DeleteDC(dc);
	if(host) DestroyWindow(host); editor.Release();
	return hr;
}
static HWND g_previewOwner, g_previewWindow;
static UINT g_previewTicks;
static BOOL g_previewDisabled, g_previewTopLevel;
static BOOL CALLBACK ClickPreviewButton(HWND window, LPARAM)
{
	WCHAR type[32], text[32]; GetClassNameW(window,type,ARRAYSIZE(type)); GetWindowTextW(window,text,ARRAYSIZE(text));
	if(!lstrcmpW(type,L"Button") && !lstrcmpW(text,L"OK")) { SendMessageW(window,BM_CLICK,0,0); return FALSE; } return TRUE;
}
static BOOL CALLBACK FindPreview(HWND window, LPARAM)
{
	WCHAR type[64]; GetClassNameW(window,type,ARRAYSIZE(type));
	if(!lstrcmpW(type,L"QuadooForm.Native.v2") && GetWindow(window,GW_OWNER) == g_previewOwner)
	{ g_previewWindow = window; g_previewDisabled = !IsWindowEnabled(g_previewOwner); g_previewTopLevel = !(GetWindowLongW(window,GWL_STYLE) & WS_CHILD); EnumChildWindows(window,ClickPreviewButton,0); return FALSE; } return TRUE;
}
static VOID CALLBACK PreviewTimer(HWND window, UINT, UINT_PTR id, DWORD)
{
	if(++g_previewTicks > 100) { KillTimer(window,id); PostQuitMessage(1); return; }
	EnumThreadWindows(GetCurrentThreadId(),FindPreview,0);
}
static HRESULT RunPreview(QFD::CDesignerEditor* editor, HWND host)
{
	HRESULT hr; TStackRef<IJSONObject> before, after; GUID selection, returned; RECT first, last; BOOL dirty; LRESULT result;
	Check(editor->Document().Serialize(&before)); Check(editor->GetSelectedID(&selection)); Check(editor->SelectionRect(&first)); dirty = editor->Document().Dirty();
	g_previewOwner = host; g_previewWindow = NULL; g_previewTicks = 0; g_previewDisabled = g_previewTopLevel = FALSE;
	CheckIf(!SetTimer(host,121,10,PreviewTimer),HrEnsureLastError()); hr = editor->PreviewForm(host,&result); KillTimer(host,121); Check(hr);
	REQUIRE(g_previewTicks > 0 && g_previewTicks <= 100 && g_previewWindow && !IsWindow(g_previewWindow) && g_previewDisabled && g_previewTopLevel && IsWindowEnabled(host) && result == IDOK);
	Check(editor->GetSelectedID(&returned)); Check(editor->SelectionRect(&last)); REQUIRE(IsEqualGUID(selection,returned) && EqualRect(&first,&last) && editor->Document().Dirty() == dirty);
	Check(editor->Document().Serialize(&after)); REQUIRE(JSONCompareObjects(before,after) == 0);
Cleanup:
	KillTimer(host,121); g_previewOwner = NULL;
	return hr;
}
static const WCHAR PreviewDocument[] = L"{\"type\":\"Form\",\"name\":\"PreviewEditor\",\"properties\":{\"Caption\":\"Preview test\",\"Width\":420,\"Height\":240,\"AutoScroll\":true},\"events\":{\"OnCreate\":\"UnavailableScript\"},\"children\":[{\"type\":\"Button\",\"name\":\"ok\",\"properties\":{\"Caption\":\"OK\",\"Left\":15,\"Top\":15,\"ModalResult\":\"mrOk\"},\"events\":{\"OnClick\":\"UnavailableClick\"}},{\"type\":\"TextBox\",\"name\":\"outside\",\"properties\":{\"Left\":650,\"Top\":350}}]}";
static HRESULT ExerciseAccessibility(QFD::CDesignerEditor* editor, BOOL warming)
{
	HRESULT hr;
	HWND list = GetDlgItem(editor->Window(),104);
	HWND controls[] = { editor->TreeWindow(),GetDlgItem(editor->Window(),102),list,GetDlgItem(list,120) };
	for(UINT n = 0; n < ARRAYSIZE(controls); n++)
	{
		TStackRef<IAccessible> accessible; LONG children;
		if(!controls[n]) continue;
		Check(AccessibleObjectFromWindow(controls[n],OBJID_CLIENT,__uuidof(IAccessible),reinterpret_cast<PVOID*>(&accessible)));
		Check(accessible->get_accChildCount(&children));
		REQUIRE(n == 3 ? children == 0 : children > 0);
	}
	// Accessibility initializes process-wide adapter windows and font caches
	// asynchronously. Exercise each bounded warm-up cycle while controls are alive;
	// fast Release workloads otherwise freeze a baseline before this work finishes.
	// Live painting/carets need not have fixed counts; the frozen count check is
	// performed after complete teardown, with no measured-phase rebasing.
	{
		DWORD started = GetTickCount(), duration = warming ? 4000 : 100;
		while(GetTickCount() - started < duration)
		{
			CheckIf(PumpPendingMessages(),E_ABORT);
			CheckIf(MsgWaitForMultipleObjects(0,NULL,FALSE,10,QS_ALLINPUT) == WAIT_FAILED,HrEnsureLastError());
		}
	}
Cleanup:
	return hr;
}
#include "EditingTests.h"
#include "NudgeTests.h"
#include "IconImportTests.h"
#include "IconRepairTests.h"
#include "MenuTests.h"
#include "ChoiceTests.h"
#include "ImageListTests.h"
#include "TreeTests.h"
#include "ListViewTests.h"
#include "..\Harness\NonvisualTests.h"
#include "..\Harness\TimerTests.h"
#include "ComponentTests.h"
#include "KeyboardMemoTests.h"
#include "PaletteTests.h"
#include "ListColumnsTests.h"
#include "ImageButtonTests.h"
#include "ProgressBarTests.h"
#include "DateTimePickerTests.h"
#include "CalendarTests.h"
#include "PasswordTests.h"
#include "StatusBarTests.h"
#include "CanvasTests.h"
#include "ImageTests.h"
#include "..\Harness\ColorDialogTests.h"
#include "ColorPickerTests.h"
#include "ColorTests.h"
#include "ColorSwatchTests.h"
#include "CommandTests.h"
#include "MultiSelectionTests.h"
#include "ResizeTests.h"
static HRESULT ExtensionsAndResources()
{
	HRESULT hr; CGuiResourceMonitor resources(L"Designer/Preview resources");
	while(!resources.Complete())
	{
		HWND host = Host(); TStackRef<QFD::CDesignerEditor> editor; TStackRef<IJSONObject> definition; TStackRef<IJSONValue> value;
		TStackRef<IQuadooControlRegistry> registry; TStackRef<IQuadooControlFactory> factory; TStackRef<IQuadooFactoryLifetime> lifetime;
		TStackRef<IQuadooControlContainer> root; TStackRef<IQuadooControl> meter; GUID id; UINT live;
		TArray<HWND> windows; QuadooForm::Rect bounds = { 20,70,120,25 };
		CheckIf(!host,HrEnsureLastError());
		Check(QuadooGetControlRegistry(&registry));
		Check(CreateExternalFactory(&factory));
		{
			TStackRef<IExternalTestFactory> test;
			Check(factory->QueryInterface(__uuidof(IExternalTestFactory),reinterpret_cast<PVOID*>(&test)));
			Check(test->SetSchema(ExternalMemoSchema));
		}
		Check(registry->RegisterFactory(factory));
		Check(factory->QueryInterface(__uuidof(IQuadooFactoryLifetime),reinterpret_cast<PVOID*>(&lifetime)));
		Check(CreateEditor(host,&editor,registry)); Check(Parse(PreviewDocument,&definition)); Check(editor->LoadDefinition(definition)); Check(editor->Document().Root()->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&root)));
		Check(editor->AddControl(RSTRING_CAST(L"ExternalMeter"),root,&bounds,&id)); Check(editor->Document().Find(id,&meter)); Check(JSONCreateInteger(57,&value)); Check(Edit(editor,meter,L"Level",value)); value.Release();
		Check(DrivePropertyDialog(editor,host,L"Legend",L"Independent Memo\r\nSecond line \x03a9",TRUE,FALSE));
		{
			TStackRef<IQuadooProperties> properties; TStackRef<IQuadooPropertyDescriptors> descriptors; TStackRef<IQuadooPropertyDescriptor> descriptor;
			UINT index; QuadooProperty::Type type;
			Check(QFD::Property(meter,L"Legend",&properties,&index));
			Check(properties->GetDescriptors(&descriptors));
			Check(descriptors->GetAt(index,&descriptor));
			Check(descriptor->GetType(&type));
			REQUIRE(type == QuadooProperty::Memo);
		}
		Check(QFD::ImportIcon(PngRGBA,sizeof(PngRGBA),&value));
		Check(Edit(editor,root,L"Icon",value));
		value.Release();
		Check(editor->SelectID(id));
		{
			HWND palette = GetDlgItem(editor->Window(),102);
			REQUIRE(FindTool(palette,L"ExternalMeter") != LB_ERR);
			REQUIRE(FindTool(palette,L"Form") == LB_ERR && FindTool(palette,L"Frame") == LB_ERR);
			Check(JSONCreateStringW(L"Meter_Pulse",11,&value)); Check(Edit(editor,meter,L"OnPulse",value)); value.Release();
			definition.Release(); Check(editor->Document().Serialize(&definition)); Check(editor->LoadDefinition(definition)); meter.Release(); root.Release(); Check(editor->Document().Find(id,&meter)); REQUIRE(QFD::IntProperty(meter,L"Level") == 57);
			CRString handler; Check(QFD::StringProperty(meter,L"OnPulse",&handler)); REQUIRE(handler == RSTRING_CAST(L"Meter_Pulse"));
			CRString legend;
			Check(QFD::StringProperty(meter,L"Legend",&legend));
			REQUIRE(legend == RSTRING_CAST(L"Independent Memo\r\nSecond line \x03a9"));
			Check(editor->Document().Root()->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&root))); Check(editor->SelectID(id)); REQUIRE(FindTree(editor->TreeWindow(),TreeView_GetRoot(editor->TreeWindow()),L"ExternalMeter1 (") != NULL);
		}
		CheckIf(!MoveWindow(editor->Window(),0,0,870,510,TRUE),HrEnsureLastError()); CheckIf(PumpPendingMessages(),E_ABORT);
		CheckIf(!MoveWindow(editor->Window(),0,0,1230,720,TRUE),HrEnsureLastError()); CheckIf(PumpPendingMessages(),E_ABORT);
		{
			HWND field = StartInline(editor,L"Name");
			REQUIRE(field != NULL);
			SetWindowTextW(field,L"CanceledRename");
			RedrawWindow(field,NULL,NULL,RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
			// Exercise all live inspector children on every bounded warm-up cycle,
			// including the new EDIT accessibility adapter, before freezing counts.
			if(!resources.Ready())
			{
				Check(ExerciseAccessibility(editor,TRUE));
			}
			SendMessageW(field,WM_KEYDOWN,VK_ESCAPE,0);
		}
		Check(DriveTabOrder(editor,host,L"ok",2,FALSE)); // Only direct children with TabOrder participate.
		Check(CollectNativeWindows(root,windows)); Check(RunPreview(editor,host)); Check(RunPreview(editor,host));
		meter.Release(); root.Release(); CheckIf(!DestroyWindow(host),HrEnsureLastError()); editor.Release();
		for(sysint n = 0; n < windows.Length(); n++) REQUIRE(!IsWindow(windows[n]));
		Check(lifetime->GetLiveCount(&live)); REQUIRE(live == 0); Check(registry->UnregisterFactory(RSTRING_CAST(L"ExternalMeter"))); lifetime.Release(); factory.Release(); registry.Release();
		REQUIRE(ExternalStatistics().objects == 0);
		Check(resources.AfterCycle());
	}
	Check(UnloadExternalExtension());
Cleanup:
	return hr;
}
#include "PackageTests.h"
static VOID WINAPI AssertTrace(HRESULT hr, PCSTR file, UINT line) { CRString text; if(SUCCEEDED(RStrFormatW(&text,L"check 0x%.8x at %S:%u\r\n",hr,file,line))) Log(text.c_wstr()); }
int wmain(int argc, WCHAR** argv)
{
	#ifdef _DEBUG
	for(INT n = 1; n < argc; n++) if(!lstrcmpW(argv[n],L"--trace")) _SetAssertCallback(AssertTrace);
	#endif
	if(argc>1 && !lstrcmpW(argv[1],L"--package-lease")) return PackageLeaseChild(argc,argv);
	if(argc > 1 && !lstrcmpW(argv[1],L"--selection-clipboard-only"))
	{
		HRESULT result = IsolatedSelectionClipboard();
		CRString summary;
		if(SUCCEEDED(RStrFormatW(&summary,L"Multi-selection clipboard: %u assertions; result 0x%.8x\r\n",g_checks,result)))
		{
			Log(summary.c_wstr());
		}
		return FAILED(result) ? 1 : 0;
	}
	INITCOMMONCONTROLSEX controls = { sizeof(controls),ICC_WIN95_CLASSES }; InitCommonControlsEx(&controls); CoInitialize(NULL);
	if(argc > 1 && !lstrcmpW(argv[1],L"--show-picker"))
	{
		COLORREF color = 0x80000000 | COLOR_HIGHLIGHT;
		HRESULT picked = ShowQuadooColorPicker(NULL,color,&color);
		CoUninitialize(); return FAILED(picked) ? 1 : 0;
	}
	HRESULT hr = S_OK; CRString summary;
	if(argc > 1 && !lstrcmpW(argv[1],L"--canvas-only"))
	{
	#ifdef _DEBUG
		_SetAssertCallback(AssertTrace);
	#endif
		hr = DesignerCanvases();
		CRString report;
		if(SUCCEEDED(RStrFormatW(&report,L"Canvas: %u assertions; result 0x%.8x\r\n",g_checks,hr)))
		{
			Log(report.c_wstr());
		}
		CoUninitialize();
		return FAILED(hr) ? 1 : 0;
	}
	if(argc > 1 && !lstrcmpW(argv[1],L"--status-bar-only"))
	{
	#ifdef _DEBUG
		_SetAssertCallback(AssertTrace);
	#endif
		hr = DesignerStatusBars();
		CRString summary;
		if(SUCCEEDED(RStrFormatW(&summary,L"StatusBar: %u assertions; result 0x%.8x\r\n",g_checks,hr)))
		{
			Log(summary.c_wstr());
		}
		CoUninitialize();
		return FAILED(hr) ? 1 : 0;
	}
	if(argc > 1 && !lstrcmpW(argv[1],L"--progress-bar-only"))
	{
		hr = DesignerProgressBars();
		CRString progressSummary;
		if(SUCCEEDED(RStrFormatW(&progressSummary,L"ProgressBar: %u assertions; result 0x%.8x\r\n",g_checks,hr)))
		{
			Log(progressSummary.c_wstr());
		}
		CoUninitialize();
		return FAILED(hr) ? 1 : 0;
	}
	if(argc > 1 && !lstrcmpW(argv[1],L"--password-only"))
	{
		hr = DesignerPasswords();
		if(SUCCEEDED(RStrFormatW(&summary,L"TextBox Password designer: %u assertions; result 0x%.8x\r\n",g_checks,hr)))
		{
			Log(summary.c_wstr());
		}
		CoUninitialize();
		return FAILED(hr) ? 1 : 0;
	}
	if(argc > 1 && !lstrcmpW(argv[1],L"--calendar-only"))
	{
		hr = DesignerCalendars();
		if(SUCCEEDED(RStrFormatW(&summary,L"Calendar designer: %u assertions, result 0x%.8x\r\n",g_checks,hr)))
		{
			Log(summary.c_wstr());
		}
		CoUninitialize();
		return FAILED(hr) ? 1 : 0;
	}
	if(argc > 1 && !lstrcmpW(argv[1],L"--datetime-picker-only"))
	{
		hr = DesignerDateTimePickers();
		if(SUCCEEDED(RStrFormatW(&summary,L"DateTimePicker designer: %u assertions; result 0x%.8x\r\n",g_checks,hr)))
		{
			Log(summary.c_wstr());
		}
		CoUninitialize();
		return FAILED(hr) ? 1 : 0;
	}
	if(argc > 1 && !lstrcmpW(argv[1],L"--image-button-only"))
	{
		hr = DesignerImageButtons();
		if(SUCCEEDED(RStrFormatW(&summary,L"ImageButton designer: %u assertions; result 0x%.8x\r\n",g_checks,hr)))
		{
			Log(summary.c_wstr());
		}
		CoUninitialize();
		return FAILED(hr) ? 1 : 0;
	}
	if(argc > 1 && !lstrcmpW(argv[1],L"--listview-only"))
	{
		hr = DesignerListViewReferences();
		if(SUCCEEDED(hr))
		{
			hr = DesignerListColumns();
		}
		if(SUCCEEDED(RStrFormatW(&summary,L"ListView designer references: %u assertions; result 0x%.8x\r\n",g_checks,hr)))
		{
			Log(summary.c_wstr());
		}
		CoUninitialize();
		return FAILED(hr) ? 1 : 0;
	}
	if(argc > 1 && !lstrcmpW(argv[1],L"--selection-only"))
	{
		hr = DesignerMultiSelection();
		if(SUCCEEDED(RStrFormatW(&summary,L"Multi-selection: %u assertions; result 0x%.8x\r\n",g_checks,hr)))
		{
			Log(summary.c_wstr());
		}
		CoUninitialize();
		return FAILED(hr) ? 1 : 0;
	}
	if(argc > 1 && !lstrcmpW(argv[1],L"--resize-only"))
	{
		hr = DesignerResizing();
		if(SUCCEEDED(RStrFormatW(&summary,L"Shift+arrow resizing: %u assertions; result 0x%.8x\r\n",g_checks,hr)))
		{
			Log(summary.c_wstr());
		}
		CoUninitialize();
		return FAILED(hr) ? 1 : 0;
	}
	if(argc > 1 && !lstrcmpW(argv[1],L"--packages-only")) { hr=DesignTimePackages(); if(SUCCEEDED(RStrFormatW(&summary,L"Packages: %u assertions; result 0x%.8x\r\n",g_checks,hr))) Log(summary.c_wstr()); CoUninitialize(); return FAILED(hr) ? 1 : 0; }
	if(argc > 1 && !lstrcmpW(argv[1],L"--tree-only"))
	{
		hr = DesignerTrees();
		if(SUCCEEDED(RStrFormatW(&summary,L"TreeView designer: %u assertions; result 0x%.8x\r\n",g_checks,hr)))
		{
			Log(summary.c_wstr());
		}
		CoUninitialize();
		return FAILED(hr) ? 1 : 0;
	}
	if(argc > 1 && !lstrcmpW(argv[1],L"--image-list-only"))
	{
		hr = DesignerImageLists(argc > 2 && argv[2][0] != L'-' ? argv[2] : NULL);
		if(SUCCEEDED(RStrFormatW(&summary,L"ImageList designer: %u assertions; result 0x%.8x\r\n",g_checks,hr)))
		{
			Log(summary.c_wstr());
		}
		CoUninitialize();
		return FAILED(hr) ? 1 : 0;
	}
	if(argc > 1 && !lstrcmpW(argv[1],L"--editing-only"))
	{
		hr = InlineChoices();
		if(SUCCEEDED(hr))
		{
			hr = InlineEditing();
		}
		if(SUCCEEDED(hr))
		{
			hr = DesignerMenus();
		}
		if(SUCCEEDED(RStrFormatW(&summary,L"Inspector editing: %u assertions; result 0x%.8x\r\n",g_checks,hr)))
		{
			Log(summary.c_wstr());
		}
		CoUninitialize();
		return FAILED(hr) ? 1 : 0;
	}
	if(argc > 1 && !lstrcmpW(argv[1],L"--menus-only"))
	{
		hr = DesignerMenus();
		if(SUCCEEDED(RStrFormatW(&summary,L"Designer menus: %u assertions; result 0x%.8x\r\n",g_checks,hr)))
		{
			Log(summary.c_wstr());
		}
		CoUninitialize();
		return FAILED(hr) ? 1 : 0;
	}
	BOOL pngOnly = argc > 1 && !lstrcmpW(argv[1],L"--png-only");
	BOOL componentsOnly = argc > 1 && !lstrcmpW(argv[1],L"--components-only");
	BOOL tabsOnly = argc > 1 && !lstrcmpW(argv[1],L"--tabs-only");
	BOOL keyboardOnly = argc > 1 && !lstrcmpW(argv[1],L"--keyboard-only");
	BOOL paletteOnly = argc > 1 && !lstrcmpW(argv[1],L"--palette-only");
	BOOL imageOnly = argc > 1 && !lstrcmpW(argv[1],L"--image-only");
	BOOL colorsOnly = argc > 1 && !lstrcmpW(argv[1],L"--colors-only");
	BOOL swatchesOnly = argc > 1 && !lstrcmpW(argv[1],L"--swatches-only");
	BOOL pickerOnly = argc > 1 && !lstrcmpW(argv[1],L"--picker-only");
	BOOL commandsOnly = argc > 1 && !lstrcmpW(argv[1],L"--commands-only");
	BOOL nudgeOnly = argc > 1 && !lstrcmpW(argv[1],L"--nudge-only");
	if(commandsOnly) { Log(L"Designer context menus, creation order and clipboard... "); hr = DesignerCommands(); }
	if(pickerOnly) { Log(L"Quadoo Form color picker dialogs... "); hr = ColorPickerDialogs(); }
	if(nudgeOnly) { Log(L"Designer Ctrl+arrow nudging... "); hr = DesignerNudging(); }
	if(swatchesOnly) { Log(L"ColorSwatch palette, inspector and persistence... "); hr = DesignerColorSwatches(); }
	if(colorsOnly) { Log(L"Color swatches, system colors and parent color copies... "); hr = DesignerColors(); }
	if(imageOnly) { Log(L"Windowless Image designer, Picture editor and native Preview... "); hr = DesignerImages(); }
	if(!imageOnly && !colorsOnly && !swatchesOnly && !nudgeOnly && !pickerOnly && !commandsOnly)
	{
		if(paletteOnly) { Log(L"Factory icons, shared palette/tray and resource lifetime... "); hr = PaletteTests(); }
		if(keyboardOnly) { Log(L"Designer keyboard properties and Memo Preview... "); hr = DesignerKeyboardAndMemo(); }
		if(!pngOnly && !componentsOnly && !tabsOnly && !keyboardOnly && !paletteOnly)
		{
			Log(L"Menu editing, simulated bars and native Preview... ");
			hr = DesignerMenus();
			if(SUCCEEDED(hr))
			{
				Log(L"passed\r\nListView image references and persistence... ");
				hr = DesignerListViewReferences();
				if(SUCCEEDED(hr))
				{
					hr = DesignerListColumns();
				}
			}
			if(SUCCEEDED(hr))
			{
				Log(L"passed\r\nImageButton properties, clipboard definitions and persistence... ");
				hr = DesignerImageButtons();
			}
			if(SUCCEEDED(hr))
			{
				Log(L"passed\r\nFactory icons, shared palette/tray and resource lifetime... ");
				hr = PaletteTests();
			}
			if(SUCCEEDED(hr)) Log(L"passed\r\n");
			if(SUCCEEDED(hr)) { Log(L"Designer document workflows... "); hr = DocumentWorkflows(); } if(SUCCEEDED(hr)) { Log(L"passed\r\nSelection, pages, zoom and constraints... "); hr = SelectionAndGeometry(); }
			if(SUCCEEDED(hr)) { Log(L"passed\r\nBuffered painting and pane resize... "); hr = PaintingAndResize(); }
			if(SUCCEEDED(hr))
			{
				Log(L"passed\r\nInline choices, strings, events and multiline fallback... ");
				hr = InlineChoices();
				if(SUCCEEDED(hr))
				{
					hr = InlineEditing();
				}
			}
			if(SUCCEEDED(hr)) { Log(L"passed\r\nDesigner Ctrl+arrow nudging... "); hr = DesignerNudging(); }
			if(SUCCEEDED(hr))
			{
				Log(L"passed\r\nDesigner Shift+arrow resizing... ");
				hr = DesignerResizing();
			}
			if(SUCCEEDED(hr))
			{
				Log(L"passed\r\nSibling multi-selection and shared properties... ");
				hr = DesignerMultiSelection();
			}
			if(SUCCEEDED(hr)) { Log(L"passed\r\nContainer tab order and rollback... "); hr = TabOrderEditing(); }
			if(SUCCEEDED(hr)) { Log(L"passed\r\nDesigner context menus, creation order and clipboard... "); hr = DesignerCommands(); }
			if(SUCCEEDED(hr)) { Log(L"passed\r\nDesigner keyboard properties and Memo Preview... "); hr = DesignerKeyboardAndMemo(); }
			if(SUCCEEDED(hr)) Log(L"passed\r\n");
		}
		if(!pngOnly && !componentsOnly && !keyboardOnly && !paletteOnly && SUCCEEDED(hr)) { Log(L"Tab page selection and placement... "); hr = TabPagePlacement(); if(SUCCEEDED(hr)) Log(L"passed\r\n"); }
		if(!componentsOnly && !tabsOnly && !keyboardOnly && !paletteOnly && SUCCEEDED(hr))
		{
			Log(L"ICO mask repair and persistence... ");
			hr = IcoRepairs();
			if(SUCCEEDED(hr))
			{
				Log(L"passed\r\nICO repair validation... ");
				hr = IcoRepairFailures();
			}
			if(SUCCEEDED(hr))
			{
				Log(L"passed\r\n");
			}
		}
		if(!componentsOnly && !tabsOnly && !keyboardOnly && !paletteOnly && SUCCEEDED(hr)) { Log(L"PNG formats, alpha and ICO conversion... "); hr = PngFormats(); }
		if(!componentsOnly && !tabsOnly && !keyboardOnly && !paletteOnly && SUCCEEDED(hr)) { Log(L"passed\r\nPNG failures, file import and persistence... "); hr = PngFailuresAndPersistence(); }
		if(!pngOnly && !tabsOnly && !keyboardOnly && !paletteOnly && SUCCEEDED(hr)) { Log(L"\r\nNonvisual runtime and independent extension... "); hr = RuntimeComponents(); }
		if(!pngOnly && !tabsOnly && !keyboardOnly && !paletteOnly && SUCCEEDED(hr)) { Log(L"passed\r\nTimer components and UI-thread events... "); hr = TimerComponents(); }
		if(!pngOnly && !tabsOnly && !keyboardOnly && !paletteOnly && SUCCEEDED(hr)) { Log(L"passed\r\nDesigner components and persistence... "); hr = DesignerComponents(); }
		if(!pngOnly && !tabsOnly && !keyboardOnly && !paletteOnly && SUCCEEDED(hr)) { Log(L"passed\r\nLossless design placeholders... "); hr = PlaceholderRoundTrips(); }
		if(!pngOnly && !componentsOnly && !tabsOnly && !keyboardOnly && !paletteOnly && SUCCEEDED(hr)) { Log(L"passed\r\nExtensions, modal Preview and resources... "); hr = ExtensionsAndResources(); }
		if(SUCCEEDED(hr) && !tabsOnly) Log(L"passed\r\n");
		if(!pngOnly && !componentsOnly && !tabsOnly && !keyboardOnly && !paletteOnly && SUCCEEDED(hr)) { Log(L"Windowless Image designer, Picture editor and native Preview... "); hr = DesignerImages(); if(SUCCEEDED(hr)) Log(L"passed\r\n"); }
		if(!pngOnly && !componentsOnly && !tabsOnly && !keyboardOnly && !paletteOnly && SUCCEEDED(hr)) { Log(L"Quadoo Form color picker dialogs... "); hr = ColorPickerDialogs(); if(SUCCEEDED(hr)) Log(L"passed\r\n"); }
		if(!pngOnly && !componentsOnly && !tabsOnly && !keyboardOnly && !paletteOnly && SUCCEEDED(hr)) { Log(L"Color swatches, system colors and parent color copies... "); hr = DesignerColors(); if(SUCCEEDED(hr)) Log(L"passed\r\n"); }
		if(!pngOnly && !componentsOnly && !tabsOnly && !keyboardOnly && !paletteOnly && SUCCEEDED(hr)) { Log(L"ColorSwatch palette, inspector and persistence... "); hr = DesignerColorSwatches(); if(SUCCEEDED(hr)) Log(L"passed\r\n"); }
		if(!pngOnly && !componentsOnly && !tabsOnly && !keyboardOnly && !paletteOnly && SUCCEEDED(hr))
		{
			Log(L"StatusBar designer tree and properties... ");
			hr = DesignerCanvases();
			if(SUCCEEDED(hr))
			{
				hr = DesignerStatusBars();
			}
		}
		if(!pngOnly && !componentsOnly && !tabsOnly && !keyboardOnly && !paletteOnly && SUCCEEDED(hr))
		{
			Log(L"ProgressBar designer... ");
			hr = DesignerProgressBars();
		}
		if(!pngOnly && !componentsOnly && !tabsOnly && !keyboardOnly && !paletteOnly && SUCCEEDED(hr)) { Log(L"Design-time control packages... "); hr=DesignTimePackages(); if(SUCCEEDED(hr)) Log(L"passed\r\n"); }
	}
	if(SUCCEEDED(RStrFormatW(&summary,L"Designer: %u assertions; result 0x%.8x\r\n",g_checks,hr))) Log(summary.c_wstr());
	CoUninitialize(); return FAILED(hr) ? 1 : 0;
}
