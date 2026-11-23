static HWND g_keyPreviewOwner, g_keyPreviewWindow, g_keyPreviewMemo, g_keyPreviewEdit;
static UINT g_keyPreviewStep, g_keyPreviewTicks;
static BOOL g_keyPreviewEdited;
static BOOL CALLBACK FindKeyboardEdit(HWND window, LPARAM)
{
	WCHAR name[32]; GetClassNameW(window,name,ARRAYSIZE(name));
	if(!lstrcmpiW(name,L"EDIT")) { if(GetWindowLongW(window,GWL_STYLE) & ES_MULTILINE) g_keyPreviewMemo = window; else g_keyPreviewEdit = window; }
	return TRUE;
}
static BOOL CALLBACK FindKeyboardPreview(HWND window, LPARAM)
{
	WCHAR name[64]; GetClassNameW(window,name,ARRAYSIZE(name));
	if(!lstrcmpW(name,L"QuadooForm.Native.v2") && !(GetWindowLongW(window,GWL_STYLE) & WS_CHILD) && GetWindow(window,GW_OWNER) == g_keyPreviewOwner)
	{ g_keyPreviewWindow = window; EnumChildWindows(window,FindKeyboardEdit,0); return FALSE; }
	return TRUE;
}
static VOID CALLBACK KeyboardPreviewTimer(HWND window, UINT, UINT_PTR id, DWORD)
{
	if(++g_keyPreviewTicks > 100) { KillTimer(window,id); PostQuitMessage(1); return; }
	EnumThreadWindows(GetCurrentThreadId(),FindKeyboardPreview,0);
	if(!g_keyPreviewMemo || !g_keyPreviewEdit) return;
	if(!g_keyPreviewStep)
	{
		::SetFocus(g_keyPreviewMemo); SendMessageW(g_keyPreviewMemo,EM_SETSEL,0,0);
		PostMessageW(g_keyPreviewMemo,WM_KEYDOWN,VK_RETURN,1 | (MapVirtualKeyW(VK_RETURN,MAPVK_VK_TO_VSC) << 16)); g_keyPreviewStep = 1;
	}
	else
	{
		g_keyPreviewEdited = SendMessageW(g_keyPreviewMemo,EM_GETLINECOUNT,0,0) > 2;
		::SetFocus(g_keyPreviewEdit); PostMessageW(g_keyPreviewEdit,WM_KEYDOWN,VK_RETURN,1 | (MapVirtualKeyW(VK_RETURN,MAPVK_VK_TO_VSC) << 16)); g_keyPreviewStep = 2;
	}
}
static HRESULT DesignerKeyboardAndMemo()
{
	HRESULT hr; HWND host = Host(), rootWindow; TStackRef<QFD::CDesignerEditor> editor;
	TStackRef<IQuadooControlContainer> root; TStackRef<IQuadooControl> memo, button, edit;
	TStackRef<IQuadooProperties> values; TStackRef<IQuadooPropertyDescriptors> descriptors; TStackRef<IQuadooPropertyDescriptor> descriptor;
	TStackRef<IJSONObject> before, after; TStackRef<IJSONValue> value; TStackRef<IQuadooFormKeyboard> keyboard;
	GUID id; UINT index; DWORD flags; BOOL visible; LRESULT result; QuadooProperty::Type type; CRString text;
	CheckIf(!host,HrEnsureLastError()); Check(CreateEditor(host,&editor)); Check(editor->NewDocument(TRUE));
	Check(editor->Document().Root()->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&root))); Check(root->GetVisible(&visible)); Check(root->GetHandle(&rootWindow)); REQUIRE(!visible && IsWindowVisible(rootWindow));
	Check(QFD::Property(root,L"KeyPreview",&values,&index)); Check(values->GetValueBool(index,&visible)); REQUIRE(!visible); values.Release();
	Check(JSONCreateBool(true,&value)); Check(Edit(editor,root,L"KeyPreview",value)); value.Release();
	{
		Check(PickTool(editor,L"Memo"));
		POINT point = { 24,40 }; MapWindowPoints(rootWindow,editor->CanvasWindow(),&point,1);
		SendMessageW(editor->OverlayWindow(),WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(point.x,point.y)); SendMessageW(editor->OverlayWindow(),WM_LBUTTONUP,0,MAKELPARAM(point.x,point.y));
		Check(editor->GetSelectedID(&id)); Check(editor->Document().Find(id,&memo)); Check(memo->GetTypeName(&text)); REQUIRE(text == RSTRING_CAST(L"Memo")); text = static_cast<RSTRING>(NULL);
	}
	Check(editor->SelectionCapabilities(&flags)); REQUIRE((flags & (QuadooForm::DesignResizeWidth | QuadooForm::DesignResizeHeight)) == (QuadooForm::DesignResizeWidth | QuadooForm::DesignResizeHeight)); Check(GripGestures(editor,memo));
	Check(QFD::Property(memo,L"Text",&values,&index)); Check(values->GetDescriptors(&descriptors)); Check(descriptors->GetAt(index,&descriptor)); Check(descriptor->GetType(&type)); REQUIRE(type == QuadooProperty::Memo); descriptor.Release(); descriptors.Release(); values.Release();
	Check(DrivePropertyDialog(editor,host,L"Text",L"First line\r\nSecond \x03a9",TRUE,FALSE));
	Check(QFD::StringProperty(memo,L"Text",&text)); REQUIRE(text == RSTRING_CAST(L"First line\r\nSecond \x03a9")); text = static_cast<RSTRING>(NULL);
	Check(JSONCreateStringW(L"UnavailableKey",14,&value)); Check(Edit(editor,root,L"OnKeyDown",value)); value.Release();
	Check(JSONCreateStringW(L"UnavailableChange",17,&value)); Check(Edit(editor,memo,L"OnChange",value)); value.Release();
	{
		QuadooForm::Rect bounds = { 220,160,100,28 }; Check(editor->AddControl(RSTRING_CAST(L"Button"),root,&bounds,&id)); Check(editor->Document().Find(id,&button));
		Check(JSONCreateBool(true,&value)); Check(Edit(editor,button,L"Default",value)); Check(Edit(editor,button,L"Cancel",value)); value.Release();
		Check(JSONCreateLongInteger(IDOK,&value)); Check(Edit(editor,button,L"ModalResult",value)); value.Release();
		bounds.Top = 200; Check(editor->AddControl(RSTRING_CAST(L"TextBox"),root,&bounds,&id)); Check(editor->Document().Find(id,&edit));
	}
	Check(root->QueryInterface(__uuidof(IQuadooFormKeyboard),reinterpret_cast<PVOID*>(&keyboard)));
	{ MSG input; ZeroMemory(&input,sizeof(input)); input.hwnd = rootWindow; input.message = WM_KEYDOWN; input.wParam = VK_RETURN; BOOL handled = TRUE; Check(keyboard->PreprocessMessage(&input,&handled)); REQUIRE(!handled); }
	Check(editor->Document().Serialize(&before)); Check(editor->LoadDefinition(before)); Check(editor->Document().Serialize(&after)); REQUIRE(JSONCompareObjects(before,after) == 0); after.Release();
	g_keyPreviewOwner = host; g_keyPreviewWindow = g_keyPreviewMemo = g_keyPreviewEdit = NULL; g_keyPreviewStep = g_keyPreviewTicks = 0; g_keyPreviewEdited = FALSE;
	REQUIRE(SetTimer(host,87,20,KeyboardPreviewTimer)); hr = editor->PreviewForm(host,&result); KillTimer(host,87); Check(hr);
	REQUIRE(result == IDOK && g_keyPreviewEdited && g_keyPreviewStep == 2 && !IsWindow(g_keyPreviewWindow) && IsWindowEnabled(host));
	Check(editor->Document().Serialize(&after)); REQUIRE(JSONCompareObjects(before,after) == 0);
	Check(editor->NewDocument(FALSE)); REQUIRE(QFD::Property(editor->Document().Root(),L"KeyPreview",&values,&index) == DISP_E_MEMBERNOTFOUND);
Cleanup:
	if(host) { KillTimer(host,87); DestroyWindow(host); } editor.Release(); g_keyPreviewOwner = NULL;
	return hr;
}
