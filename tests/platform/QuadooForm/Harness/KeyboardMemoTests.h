static HRESULT KeyBool(IQuadooComponent* control, PCWSTR name, BOOL value)
{
	HRESULT hr; TStackRef<IQuadooProperties> properties; UINT index;
	Check(ComponentProperty(control,name,&properties,&index)); Check(properties->SetValueBool(index,value));
Cleanup:
	return hr;
}
static HRESULT KeyText(IQuadooComponent* control, PCWSTR text)
{
	HRESULT hr; TStackRef<IQuadooProperties> properties; UINT index;
	Check(ComponentProperty(control,L"Text",&properties,&index)); Check(properties->SetValueString(index,RSTRING_CAST(text)));
Cleanup:
	return hr;
}
static HRESULT KeyWindow(IQuadooControl* control, HWND* window)
{
	HRESULT hr; TStackRef<IQuadooWinControl> native;
	Check(control->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&native))); Check(native->GetHandle(window));
Cleanup:
	return hr;
}
static HRESULT KeyDispatch(IQuadooFormKeyboard* keyboard, HWND target, UINT message, WPARAM key, BOOL translate = FALSE)
{
	HRESULT hr; MSG input; BOOL handled;
	ZeroMemory(&input,sizeof(input)); input.hwnd = target; input.message = message; input.wParam = key;
	input.lParam = 1 | (MapVirtualKeyW(static_cast<UINT>(key),MAPVK_VK_TO_VSC) << 16); input.time = GetTickCount();
	if(message == WM_CHAR) input.lParam = 1 | (MapVirtualKeyW(static_cast<UINT>(VkKeyScanW(static_cast<WCHAR>(key)) & 0xff),MAPVK_VK_TO_VSC) << 16);
	Check(keyboard->PreprocessMessage(&input,&handled));
	if(!handled) { if(translate) TranslateMessage(&input); DispatchMessageW(&input); }
Cleanup:
	return hr;
}
static HRESULT KeyPump(IQuadooFormKeyboard* keyboard)
{
	HRESULT hr = S_OK; MSG message; UINT count = 0;
	while(PeekMessageW(&message,NULL,0,0,PM_REMOVE))
	{
		BOOL handled; CheckIf(message.message == WM_QUIT || ++count > 4096,E_ABORT);
		Check(keyboard->PreprocessMessage(&message,&handled));
		if(!handled) { TranslateMessage(&message); DispatchMessageW(&message); }
	}
Cleanup:
	return hr;
}
static HRESULT KeyWithControl(IQuadooFormKeyboard* keyboard, HWND target, UINT key)
{
	HRESULT hr; BYTE saved[256], state[256]; BOOL changed = FALSE;
	CheckIf(!GetKeyboardState(saved),HrEnsureLastError()); CopyMemory(state,saved,sizeof(state)); state[VK_CONTROL] = 0x80;
	CheckIf(!SetKeyboardState(state),HrEnsureLastError()); changed = TRUE;
	Check(KeyDispatch(keyboard,target,WM_KEYDOWN,key)); Check(KeyPump(keyboard));
Cleanup:
	if(changed) SetKeyboardState(saved);
	return hr;
}
struct KeyboardThreadCall { IQuadooFormKeyboard* keyboard; HWND target; HRESULT result; BOOL handled; };
static DWORD WINAPI KeyboardThread(PVOID argument)
{
	KeyboardThreadCall* call = static_cast<KeyboardThreadCall*>(argument); MSG message; ZeroMemory(&message,sizeof(message)); message.hwnd = call->target; message.message = WM_KEYDOWN; message.wParam = 'A';
	call->result = call->keyboard->PreprocessMessage(&message,&call->handled); return 0;
}
class CKeyboardBinder : public CBaseUnknown, public IQuadooEventsBinder, public IQuadooDelegate
{
public:
	struct Key { BOOL form; UINT stage; INT key; };
	TArray<Key> keys; CRString clicked;
	UINT clicks, changes, closes;
	BOOL handleDown, handleChar, handleChild, destroyForm, destroyTarget, destroyClick, cancelClose;
	IQuadooControl* target;
	IQuadooFormKeyboard* reenter;
	IQuadooControl* destroyOwner;
	BOOL rewriteText;
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooEventsBinder)
		UNK_INTERFACE(IQuadooDelegate)
	END_UNK_MAP
	CKeyboardBinder() : clicks(0), changes(0), closes(0), handleDown(FALSE), handleChar(FALSE), handleChild(FALSE), destroyForm(FALSE), destroyTarget(FALSE), destroyClick(FALSE), cancelClose(FALSE), target(NULL), reenter(NULL), destroyOwner(NULL), rewriteText(FALSE) {}
	STDMETHOD(BindEvent)(RSTRING name, IQuadooDelegate** result) { if(!result) return E_POINTER; *result = NULL; if(!lstrcmpW(RStrToWide(name),L"Missing")) return S_FALSE; *result = this; AddRef(); return S_OK; }
	STDMETHOD(PostBind)(IQuadooControl*) { return S_OK; }
	STDMETHOD(Invoke)(IQuadooEventArgs* args)
	{
		HRESULT hr; CRString event; TStackRef<IQuadooComponent> sender; TStackRef<IQuadooForm> form;
		Check(args->GetEventName(&event)); Check(args->GetSender(&sender));
		if(event == RSTRING_CAST(L"OnKeyDown") || event == RSTRING_CAST(L"OnKeyPress") || event == RSTRING_CAST(L"OnKeyUp"))
		{
			Key key; key.form = SUCCEEDED(sender->QueryInterface(__uuidof(IQuadooForm),reinterpret_cast<PVOID*>(&form)));
			key.stage = event == RSTRING_CAST(L"OnKeyDown") ? WM_KEYDOWN : event == RSTRING_CAST(L"OnKeyPress") ? WM_CHAR : WM_KEYUP;
			Check(args->GetInt(0,&key.key)); Check(keys.Append(key));
			if((key.form && ((handleDown && key.stage == WM_KEYDOWN) || (handleChar && key.stage == WM_CHAR))) || (!key.form && handleChild && key.stage == WM_KEYDOWN)) { Check(args->SetBool(1,TRUE)); }
			if(key.form && destroyTarget && target) { Check(target->Destroy()); }
			if(key.form && destroyForm) { Check(sender->Destroy()); }
		}
		else if(event == RSTRING_CAST(L"OnClick"))
		{
			clicks++; clicked = static_cast<RSTRING>(NULL); Check(sender->GetName(&clicked));
			if(reenter) { Check(KeyDispatch(reenter,GetFocus(),WM_KEYDOWN,VK_RETURN)); }
			if(destroyOwner) { Check(destroyOwner->Destroy()); }
			if(destroyClick) { Check(sender->Destroy()); }
		}
		else if(event == RSTRING_CAST(L"OnChange")) { changes++; if(rewriteText) { rewriteText = FALSE; Check(KeyText(sender,L"Rewritten \x03a9")); } }
		else if(event == RSTRING_CAST(L"OnCloseQuery")) { closes++; if(cancelClose) { cancelClose = FALSE; Check(args->SetBool(0,TRUE)); } }
		hr = S_OK;
Cleanup:
		return hr;
	}
};
static const WCHAR KeyboardDefinition[] =
	L"{\"type\":\"Form\",\"name\":\"Keyboard\",\"properties\":{\"Width\":500,\"Height\":350},\"events\":{\"OnKeyDown\":\"Key\",\"OnKeyPress\":\"Key\",\"OnKeyUp\":\"Key\",\"OnCloseQuery\":\"Close\"},\"children\":["
	L"{\"type\":\"TextBox\",\"name\":\"edit\",\"properties\":{\"Left\":10,\"Top\":10,\"TabOrder\":0},\"events\":{\"OnKeyDown\":\"Key\",\"OnKeyPress\":\"Key\",\"OnKeyUp\":\"Key\"}},"
	L"{\"type\":\"Memo\",\"name\":\"memo\",\"properties\":{\"Left\":10,\"Top\":50,\"Text\":\"One\\nTwo\\rThree\\r\\n\\u03a9\",\"TabOrder\":1},\"events\":{\"OnKeyDown\":\"Key\",\"OnKeyPress\":\"Key\",\"OnKeyUp\":\"Key\",\"OnChange\":\"Changed\"}},"
	L"{\"type\":\"Button\",\"name\":\"default\",\"properties\":{\"Left\":210,\"Top\":10,\"Default\":true,\"TabOrder\":2},\"events\":{\"OnClick\":\"Click\"}},"
	L"{\"type\":\"Button\",\"name\":\"cancel\",\"properties\":{\"Left\":210,\"Top\":50,\"Cancel\":true,\"TabOrder\":3},\"events\":{\"OnClick\":\"Click\"}},"
	L"{\"type\":\"Button\",\"name\":\"ordinary\",\"properties\":{\"Left\":210,\"Top\":90,\"TabOrder\":4},\"events\":{\"OnClick\":\"Click\"}},"
	L"{\"type\":\"ComboBox\",\"name\":\"combo\",\"properties\":{\"Left\":10,\"Top\":180,\"Items\":[\"One\",\"Two\"],\"TabOrder\":5},\"events\":{\"OnKeyDown\":\"Key\",\"OnKeyPress\":\"Key\"}}]}";
static HRESULT KeyboardAndMemo()
{
	HRESULT hr; TStackRef<IJSONObject> definition, saved; TStackRef<IQuadooForm> form;
	TStackRef<IQuadooFormKeyboard> keyboard; TStackRef<CKeyboardBinder> binder;
	TStackRef<IQuadooControl> edit, memo, defaultButton, cancel, ordinary, combo;
	TStackRef<IQuadooProperties> values; TStackRef<IQuadooFormSerializer> serializer;
	HWND formWindow, editWindow, memoWindow, window; UINT index; WCHAR text[256]; CRString logical;
	binder.Attach(__new CKeyboardBinder); CheckAlloc(binder); Check(Parse(KeyboardDefinition,&definition));
	Check(QuadooCreateForm(binder,NULL,definition,&form)); Check(form->ShowForm(SW_SHOW)); Check(form->GetHandle(&formWindow));
	Check(form->QueryInterface(__uuidof(IQuadooFormKeyboard),reinterpret_cast<PVOID*>(&keyboard)));
	Check(form->FindControl(RSTRING_CAST(L"edit"),&edit)); Check(form->FindControl(RSTRING_CAST(L"memo"),&memo)); Check(form->FindControl(RSTRING_CAST(L"default"),&defaultButton));
	Check(form->FindControl(RSTRING_CAST(L"cancel"),&cancel)); Check(form->FindControl(RSTRING_CAST(L"ordinary"),&ordinary)); Check(form->FindControl(RSTRING_CAST(L"combo"),&combo));
	Check(KeyWindow(edit,&editWindow)); Check(KeyWindow(memo,&memoWindow));
	GetWindowTextW(memoWindow,text,ARRAYSIZE(text)); REQUIRE(!lstrcmpW(text,L"One\r\nTwo\r\nThree\r\n\x03a9"));
	Check(ComponentProperty(memo,L"Text",&values,&index)); Check(values->GetValueString(index,&logical)); REQUIRE(logical == RSTRING_CAST(L"One\nTwo\rThree\r\n\x03a9")); values.Release();
	REQUIRE((GetWindowLongW(memoWindow,GWL_STYLE) & (ES_MULTILINE | ES_WANTRETURN | WS_VSCROLL)) == (ES_MULTILINE | ES_WANTRETURN | WS_VSCROLL));
	{
		const IID* paths[] = { &__uuidof(IQuadooComponent),&__uuidof(IQuadooControl),&__uuidof(IQuadooWinControl),&__uuidof(IQuadooProperties),&__uuidof(IQuadooMessageHandler),&__uuidof(IQuadooConstructionParticipant) }; Check(CheckCOMIdentity(memo,paths,ARRAYSIZE(paths)));
		const IID* buttonPaths[] = { &__uuidof(IQuadooButton),&__uuidof(IQuadooWinControl),&__uuidof(IQuadooControl) }; Check(CheckCOMIdentity(defaultButton,buttonPaths,ARRAYSIZE(buttonPaths)));
		const IID* formPaths[] = { &__uuidof(IQuadooFormKeyboard),&__uuidof(IQuadooForm),&__uuidof(IQuadooComponentHost) }; Check(CheckCOMIdentity(form,formPaths,ARRAYSIZE(formPaths)));
	}
	::SetFocus(editWindow); REQUIRE(GetFocus() == editWindow); Check(KeyPump(keyboard)); binder->keys.Clear();
	{
		KeyboardThreadCall call = { keyboard,editWindow,S_OK,TRUE }; HANDLE thread = CreateThread(NULL,0,KeyboardThread,&call,0,NULL); REQUIRE(thread != NULL);
		DWORD wait = WaitForSingleObject(thread,5000); CloseHandle(thread); REQUIRE(wait == WAIT_OBJECT_0 && call.result == QuadooForm::EWrongThread && !call.handled);
	}
	Check(KeyDispatch(keyboard,editWindow,WM_KEYDOWN,'A')); Check(KeyDispatch(keyboard,editWindow,WM_CHAR,'a')); Check(KeyDispatch(keyboard,editWindow,WM_KEYUP,'A'));
	REQUIRE(binder->keys.Length() == 3 && !binder->keys[0].form && !binder->keys[1].form && !binder->keys[2].form);
	Check(KeyBool(form,L"KeyPreview",TRUE)); binder->keys.Clear();
	Check(KeyDispatch(keyboard,editWindow,WM_KEYDOWN,'B')); Check(KeyDispatch(keyboard,editWindow,WM_CHAR,'b')); Check(KeyDispatch(keyboard,editWindow,WM_KEYUP,'B'));
	REQUIRE(binder->keys.Length() == 6 && binder->keys[0].form && !binder->keys[1].form && binder->keys[2].form && !binder->keys[3].form && binder->keys[4].form && !binder->keys[5].form);
	REQUIRE(binder->keys[0].key == 'B' && binder->keys[2].key == 'b' && GetFocus() == editWindow);
	binder->handleDown = TRUE; binder->keys.Clear(); Check(KeyDispatch(keyboard,editWindow,WM_KEYDOWN,'X')); Check(KeyDispatch(keyboard,editWindow,WM_CHAR,'x')); Check(KeyDispatch(keyboard,editWindow,WM_KEYUP,'X'));
	REQUIRE(binder->keys.Length() == 3 && binder->keys[0].stage == WM_KEYDOWN && binder->keys[1].stage == WM_KEYUP && binder->keys[2].stage == WM_KEYUP);
	GetWindowTextW(editWindow,text,ARRAYSIZE(text)); REQUIRE(!lstrcmpW(text,L"ab")); binder->handleDown = FALSE;
	binder->handleChar = TRUE; binder->keys.Clear(); Check(KeyDispatch(keyboard,editWindow,WM_CHAR,'z')); REQUIRE(binder->keys.Length() == 1 && binder->keys[0].form); binder->handleChar = FALSE;
	binder->handleChild = TRUE; binder->keys.Clear(); Check(KeyDispatch(keyboard,editWindow,WM_KEYDOWN,'Q')); Check(KeyDispatch(keyboard,editWindow,WM_CHAR,'q')); REQUIRE(binder->keys.Length() == 2); binder->handleChild = FALSE;
	Check(KeyText(edit,L"")); binder->keys.Clear(); Check(KeyDispatch(keyboard,editWindow,WM_CHAR,0x03a9)); Check(KeyDispatch(keyboard,editWindow,WM_CHAR,0xd83d)); Check(KeyDispatch(keyboard,editWindow,WM_CHAR,0xde00)); Check(KeyDispatch(keyboard,editWindow,WM_CHAR,0x00e5));
	GetWindowTextW(editWindow,text,ARRAYSIZE(text)); REQUIRE(!lstrcmpW(text,L"\x03a9\xd83d\xde00\x00e5") && binder->keys.Length() == 8 && binder->keys[0].key == 0x03a9 && binder->keys[4].key == 0xde00 && binder->keys[6].form && binder->keys[6].key == 0x00e5);
	binder->keys.Clear(); Check(KeyDispatch(keyboard,editWindow,WM_KEYDOWN,VK_PROCESSKEY)); REQUIRE(binder->keys.Length() == 1 && !binder->keys[0].form);
	UINT oldClicks = binder->clicks; Check(KeyWithControl(keyboard,editWindow,VK_RETURN)); REQUIRE(binder->clicks == oldClicks);
	binder->keys.Clear(); binder->reenter = keyboard; Check(KeyDispatch(keyboard,editWindow,WM_KEYDOWN,VK_RETURN)); binder->reenter = NULL; REQUIRE(binder->clicks == 1 && binder->clicked == RSTRING_CAST(L"default") && binder->keys.Length() == 0);
	Check(KeyDispatch(keyboard,editWindow,WM_KEYDOWN,VK_ESCAPE)); REQUIRE(binder->clicks == 2 && binder->clicked == RSTRING_CAST(L"cancel") && IsWindow(formWindow));
	Check(KeyWindow(ordinary,&window)); ::SetFocus(window); Check(KeyDispatch(keyboard,window,WM_KEYDOWN,VK_RETURN)); REQUIRE(binder->clicks == 3 && binder->clicked == RSTRING_CAST(L"ordinary"));
	REQUIRE((GetWindowLongW(window,GWL_STYLE) & BS_TYPEMASK) == BS_DEFPUSHBUTTON);
	Check(KeyBool(ordinary,L"Default",TRUE)); Check(KeyBool(ordinary,L"Cancel",TRUE)); ::SetFocus(window); Check(KeyDispatch(keyboard,window,WM_KEYDOWN,VK_ESCAPE)); REQUIRE(binder->clicks == 4 && binder->clicked == RSTRING_CAST(L"ordinary"));
	Check(KeyText(memo,L"")); ::SetFocus(memoWindow); UINT clicks = binder->clicks, changes = binder->changes;
	Check(KeyDispatch(keyboard,memoWindow,WM_KEYDOWN,VK_RETURN,TRUE)); Check(KeyPump(keyboard));
	GetWindowTextW(memoWindow,text,ARRAYSIZE(text));
	if(lstrcmpW(text,L"\r\n") || binder->clicks != clicks || binder->changes != changes + 1)
	{ CRString diagnostic; if(SUCCEEDED(RStrFormatW(&diagnostic,L"Memo Return: length=%u first=%u clicks=%u/%u changes=%u/%u focus=%p/%p modifiers=%x,%x,%x\r\n",lstrlenW(text),text[0],binder->clicks,clicks,binder->changes,changes + 1,GetFocus(),memoWindow,GetKeyState(VK_SHIFT),GetKeyState(VK_CONTROL),GetKeyState(VK_MENU)))) Log(diagnostic.c_wstr()); }
	REQUIRE(!lstrcmpW(text,L"\r\n") && binder->clicks == clicks && binder->changes == changes + 1);
	Check(KeyDispatch(keyboard,memoWindow,WM_KEYDOWN,VK_ESCAPE)); REQUIRE(binder->clicks == clicks + 1 && binder->clicked == RSTRING_CAST(L"cancel")); clicks++;
	Check(KeyDispatch(keyboard,memoWindow,WM_KEYDOWN,VK_TAB)); REQUIRE(GetFocus() != memoWindow && binder->clicks == clicks);
	::SetFocus(memoWindow); Check(KeyBool(memo,L"WantTabs",TRUE)); Check(KeyDispatch(keyboard,memoWindow,WM_KEYDOWN,VK_TAB,TRUE)); Check(KeyPump(keyboard)); GetWindowTextW(memoWindow,text,ARRAYSIZE(text)); REQUIRE(TStrChr(text,L'\t') != NULL && GetFocus() == memoWindow);
	Check(KeyBool(memo,L"ReadOnly",TRUE)); changes = binder->changes; Check(KeyDispatch(keyboard,memoWindow,WM_CHAR,'r')); REQUIRE(binder->changes == changes); Check(KeyBool(memo,L"ReadOnly",FALSE));
	Check(KeyText(memo,L"First\nSecond\r\n\x03a9")); changes = binder->changes; SendMessageW(memoWindow,EM_SETSEL,2,7);
	Check(KeyWithControl(keyboard,memoWindow,'A')); DWORD allStart, allEnd; SendMessageW(memoWindow,EM_GETSEL,reinterpret_cast<WPARAM>(&allStart),reinterpret_cast<LPARAM>(&allEnd)); REQUIRE(allStart == 0 && allEnd == static_cast<DWORD>(GetWindowTextLengthW(memoWindow))); SendMessageW(memoWindow,EM_SETSEL,2,7);
	Check(KeyBool(memo,L"WordWrap",FALSE)); Check(KeyWindow(memo,&window)); REQUIRE(window != memoWindow && !IsWindow(memoWindow) && GetFocus() == window); memoWindow = window;
	DWORD start, end; SendMessageW(window,EM_GETSEL,reinterpret_cast<WPARAM>(&start),reinterpret_cast<LPARAM>(&end)); REQUIRE(start == 2 && end == 7 && binder->changes == changes);
	Check(SetInt(memo,L"ScrollBars",3)); Check(KeyWindow(memo,&memoWindow)); REQUIRE((GetWindowLongW(memoWindow,GWL_STYLE) & WS_HSCROLL) != 0);
	Check(KeyBool(memo,L"WordWrap",TRUE)); Check(KeyWindow(memo,&memoWindow)); REQUIRE(!(GetWindowLongW(memoWindow,GWL_STYLE) & WS_HSCROLL) && binder->changes == changes);
	for(UINT rewritten = 0; rewritten < 2; rewritten++)
	{
		IQuadooControl* control = rewritten ? static_cast<IQuadooControl*>(edit) : static_cast<IQuadooControl*>(memo);
		TStackRef<IQuadooProperties> properties; UINT property; HWND native;
		Check(ComponentProperty(control,L"OnChange",&properties,&property)); Check(properties->SetValueEvent(property,RSTRING_CAST(L"Changed"))); properties.Release();
		Check(KeyText(control,L"")); Check(KeyWindow(control,&native)); ::SetFocus(native); changes = binder->changes; binder->rewriteText = TRUE;
		SendMessageW(native,WM_CHAR,L'v',1); GetWindowTextW(native,text,ARRAYSIZE(text));
		REQUIRE(!lstrcmpW(text,L"Rewritten \x03a9") && binder->changes == changes + 2);
		Check(ComponentProperty(control,L"Text",&properties,&property)); logical = static_cast<RSTRING>(NULL); Check(properties->GetValueString(property,&logical)); REQUIRE(logical == RSTRING_CAST(L"Rewritten \x03a9"));
	}
	Check(QuadooGetFormSerializer(&serializer)); Check(serializer->Serialize(form,&saved));
	Check(KeyWindow(combo,&window)); COMBOBOXINFO info; ZeroMemory(&info,sizeof(info)); info.cbSize = sizeof(info); REQUIRE(GetComboBoxInfo(window,&info));
	::SetFocus(info.hwndItem); SendMessageW(window,CB_SHOWDROPDOWN,TRUE,0); clicks = binder->clicks; binder->keys.Clear(); Check(KeyDispatch(keyboard,info.hwndItem,WM_KEYDOWN,VK_ESCAPE)); REQUIRE(!SendMessageW(window,CB_GETDROPPEDSTATE,0,0) && binder->clicks == clicks && binder->keys.Length() == 1 && !binder->keys[0].form);
	Check(form->Destroy()); form.Release(); keyboard.Release();
	// Preview retains native keyboard/edit semantics without invoking the binder.
	{
		TStackRef<IQuadooConstructionContext> context; TStackRef<IQuadooControl> root; TStackRef<IQuadooForm> preview; TStackRef<IQuadooFormKeyboard> previewKeys;
		TStackRef<IJSONObject> roundtrip;
		Check(Context(96,binder,&context,NULL,QuadooForm::Preview)); Check(QuadooCreateControl(context,NULL,saved,&root)); Check(root->QueryInterface(__uuidof(IQuadooForm),reinterpret_cast<PVOID*>(&preview))); Check(preview->ShowForm(SW_SHOW));
		Check(preview->QueryInterface(__uuidof(IQuadooFormKeyboard),reinterpret_cast<PVOID*>(&previewKeys))); edit.Release(); Check(preview->FindControl(RSTRING_CAST(L"edit"),&edit)); Check(KeyWindow(edit,&editWindow)); ::SetFocus(editWindow);
		clicks = binder->clicks; Check(KeyDispatch(previewKeys,editWindow,WM_KEYDOWN,VK_RETURN)); REQUIRE(binder->clicks == clicks);
		Check(serializer->Serialize(preview,&roundtrip)); REQUIRE(JSONCompareObjects(saved,roundtrip) == 0); Check(preview->Destroy());
	}
Cleanup:
	if(form) form->Destroy();
	return hr;
}
static HRESULT KeyboardScopes()
{
	HRESULT hr; TStackRef<IJSONObject> definition, frameDefinition; TStackRef<IQuadooForm> form, other;
	TStackRef<IQuadooFormKeyboard> keyboard; TStackRef<CKeyboardBinder> binder;
	TStackRef<IQuadooFrame> outer, inner; TStackRef<IQuadooControl> edit, innerEdit, innerButton, second, rootButton;
	TStackRef<IQuadooControlContainer> outerContainer; HWND rootWindow, focus, otherWindow; UINT clicks;
	binder.Attach(__new CKeyboardBinder); CheckAlloc(binder); Check(Parse(KeyboardDefinition,&definition)); Check(QuadooCreateForm(binder,NULL,definition,&form)); Check(form->ShowForm(SW_SHOW));
	Check(form->QueryInterface(__uuidof(IQuadooFormKeyboard),reinterpret_cast<PVOID*>(&keyboard))); Check(form->GetHandle(&rootWindow));
	Check(form->FindControl(RSTRING_CAST(L"edit"),&edit)); Check(form->FindControl(RSTRING_CAST(L"default"),&rootButton));
	Check(Parse(L"{\"type\":\"Frame\",\"name\":\"Outer\",\"properties\":{\"Left\":10,\"Top\":210,\"Width\":350,\"Height\":100},\"children\":[{\"type\":\"Button\",\"name\":\"OuterDefault\",\"properties\":{\"Default\":true,\"Cancel\":true},\"events\":{\"OnClick\":\"Click\"}}]}",&frameDefinition));
	Check(QuadooCreateFrame(binder,form,frameDefinition,&outer)); frameDefinition.Release();
	Check(outer->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&outerContainer)));
	Check(Parse(L"{\"type\":\"Frame\",\"name\":\"Inner\",\"properties\":{\"Left\":110,\"Width\":220,\"Height\":90},\"children\":[{\"type\":\"TextBox\",\"name\":\"InnerEdit\",\"properties\":{\"TabOrder\":0}},{\"type\":\"Button\",\"name\":\"First\",\"properties\":{\"Default\":true,\"Cancel\":true,\"TabOrder\":2,\"Top\":30},\"events\":{\"OnClick\":\"Click\"}},{\"type\":\"Button\",\"name\":\"Second\",\"properties\":{\"Default\":true,\"Cancel\":true,\"TabOrder\":1,\"Left\":110,\"Top\":30},\"events\":{\"OnClick\":\"Click\"}}]}",&frameDefinition));
	Check(QuadooCreateFrame(binder,outerContainer,frameDefinition,&inner));
	Check(inner->FindControl(RSTRING_CAST(L"InnerEdit"),&innerEdit)); Check(inner->FindControl(RSTRING_CAST(L"First"),&innerButton)); Check(inner->FindControl(RSTRING_CAST(L"Second"),&second));
	rootButton.Release(); REQUIRE(form->FindControl(RSTRING_CAST(L"InnerEdit"),&rootButton) == HRESULT_FROM_WIN32(ERROR_NOT_FOUND)); // Namespace boundaries remain intact.
	Check(KeyWindow(innerEdit,&focus)); ::SetFocus(focus); Check(KeyDispatch(keyboard,focus,WM_KEYDOWN,VK_RETURN)); REQUIRE(binder->clicked == RSTRING_CAST(L"Second"));
	Check(SetInt(innerButton,L"TabOrder",1)); Check(KeyDispatch(keyboard,focus,WM_KEYDOWN,VK_RETURN)); REQUIRE(binder->clicked == RSTRING_CAST(L"First"));
	Check(innerButton->SetEnabled(FALSE)); Check(KeyDispatch(keyboard,focus,WM_KEYDOWN,VK_ESCAPE)); REQUIRE(binder->clicked == RSTRING_CAST(L"Second"));
	Check(second->SetVisible(FALSE)); Check(KeyDispatch(keyboard,focus,WM_KEYDOWN,VK_RETURN)); REQUIRE(binder->clicked == RSTRING_CAST(L"OuterDefault"));
	Check(second->SetVisible(TRUE)); Check(innerButton->SetEnabled(TRUE));
	{
		TStackRef<IQuadooConstructionContext> context; TStackRef<IJSONObject> tabDefinition; TStackRef<IQuadooControl> tabControl, cancelButton;
		TStackRef<IQuadooTabControl> tabs;
		Check(Context(96,binder,&context)); Check(Parse(L"{\"type\":\"TabControl\",\"name\":\"KeysTabs\",\"properties\":{\"Left\":300,\"Top\":40,\"Width\":180,\"Height\":140,\"ActivePageIndex\":0},\"children\":[{\"type\":\"TabPage\",\"name\":\"Page0\",\"children\":[{\"type\":\"Button\",\"name\":\"PageDefault0\",\"properties\":{\"Default\":true,\"Cancel\":true},\"events\":{\"OnClick\":\"Click\"}}]},{\"type\":\"TabPage\",\"name\":\"Page1\",\"children\":[{\"type\":\"Button\",\"name\":\"PageDefault1\",\"properties\":{\"Default\":true,\"Cancel\":true},\"events\":{\"OnClick\":\"Click\"}}]}]}",&tabDefinition));
		Check(QuadooCreateControl(context,form,tabDefinition,&tabControl)); Check(tabControl->QueryInterface(__uuidof(IQuadooTabControl),reinterpret_cast<PVOID*>(&tabs)));
		Check(form->FindControl(RSTRING_CAST(L"default"),&rootButton)); Check(KeyBool(rootButton,L"Default",FALSE)); Check(form->FindControl(RSTRING_CAST(L"cancel"),&cancelButton)); Check(KeyBool(cancelButton,L"Cancel",FALSE));
		Check(outer->SetEnabled(FALSE)); Check(KeyWindow(edit,&focus)); ::SetFocus(focus);
		Check(KeyDispatch(keyboard,focus,WM_KEYDOWN,VK_RETURN)); REQUIRE(binder->clicked == RSTRING_CAST(L"PageDefault0"));
		Check(tabs->SetActivePageIndex(1)); Check(KeyDispatch(keyboard,focus,WM_KEYDOWN,VK_ESCAPE)); REQUIRE(binder->clicked == RSTRING_CAST(L"PageDefault1"));
		Check(tabControl->SetEnabled(FALSE)); clicks = binder->clicks; Check(KeyDispatch(keyboard,focus,WM_KEYDOWN,VK_RETURN)); REQUIRE(binder->clicks == clicks);
		Check(KeyBool(rootButton,L"Default",TRUE)); Check(KeyBool(cancelButton,L"Cancel",TRUE)); Check(outer->SetEnabled(TRUE)); Check(tabControl->Destroy());
		Check(KeyWindow(innerEdit,&focus)); ::SetFocus(focus);
	}
	{
		TStackRef<IQuadooWinControl> native; Check(innerEdit->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&native)));
		Check(ConfigureExternalKeyClaims(native,TRUE)); REQUIRE(ExternalCanUnload() == S_FALSE);
		UINT messages = ExternalStatistics().messages; clicks = binder->clicks;
		Check(KeyDispatch(keyboard,focus,WM_KEYDOWN,VK_RETURN)); Check(KeyDispatch(keyboard,focus,WM_KEYDOWN,VK_ESCAPE)); REQUIRE(binder->clicks == clicks && ExternalStatistics().messages == messages + 2);
		Check(ConfigureExternalKeyClaims(native,FALSE)); REQUIRE(ExternalCanUnload() == S_OK);
	}
	// Another active Form must not be intercepted by this Form's dispatcher.
	Check(QuadooCreateForm(binder,NULL,definition,&other)); Check(other->ShowForm(SW_SHOW)); Check(other->GetHandle(&otherWindow)); ::SetFocus(otherWindow);
	MSG input; ZeroMemory(&input,sizeof(input)); input.hwnd = otherWindow; input.message = WM_KEYDOWN; input.wParam = VK_RETURN; BOOL handled = TRUE;
	Check(keyboard->PreprocessMessage(&input,&handled)); REQUIRE(!handled && binder->clicks == clicks); Check(other->Destroy()); other.Release();
	SetActiveWindow(rootWindow); Check(KeyWindow(edit,&focus)); ::SetFocus(focus); Check(KeyBool(form,L"KeyPreview",TRUE));
	{ TStackRef<IQuadooWinControl> native; Check(edit->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&native))); Check(ConfigureExternalKeyClaims(native,TRUE)); }
	binder->target = edit; binder->destroyTarget = TRUE; Check(KeyDispatch(keyboard,focus,WM_KEYDOWN,'D')); REQUIRE(!IsWindow(focus)); binder->target = NULL; binder->destroyTarget = FALSE;
	REQUIRE(ExternalCanUnload() == S_OK);
	// A click may remove its own candidate; subsequent resolution must rescan.
	Check(KeyWindow(innerEdit,&focus)); ::SetFocus(focus); binder->destroyClick = TRUE; Check(KeyDispatch(keyboard,focus,WM_KEYDOWN,VK_RETURN)); REQUIRE(binder->clicked == RSTRING_CAST(L"First")); binder->destroyClick = FALSE;
	Check(KeyDispatch(keyboard,focus,WM_KEYDOWN,VK_RETURN)); REQUIRE(binder->clicked == RSTRING_CAST(L"Second"));
	binder->destroyOwner = inner; Check(KeyDispatch(keyboard,focus,WM_KEYDOWN,VK_RETURN)); binder->destroyOwner = NULL; REQUIRE(!IsWindow(focus));
	Check(outer->GetHandle(&focus)); ::SetFocus(focus); binder->destroyForm = TRUE; Check(KeyDispatch(keyboard,focus,WM_KEYDOWN,'F')); REQUIRE(!IsWindow(rootWindow));
Cleanup:
	if(binder) binder->target = NULL; if(other) other->Destroy(); if(form) form->Destroy();
	return hr;
}
static IQuadooForm* g_keyboardModal;
static HWND g_keyboardFocus;
static UINT g_keyboardModalTicks;
static VOID CALLBACK KeyboardModalTimer(HWND window, UINT, UINT_PTR, DWORD)
{
	if(!g_keyboardModal) return;
	if(++g_keyboardModalTicks > 5) { KillTimer(window,86); g_keyboardModal->Destroy(); return; }
	::SetFocus(g_keyboardFocus); PostMessageW(g_keyboardFocus,WM_KEYDOWN,VK_RETURN,1 | (MapVirtualKeyW(VK_RETURN,MAPVK_VK_TO_VSC) << 16));
}
static HRESULT KeyboardModalAndResources()
{
	HRESULT hr; TStackRef<IJSONObject> definition; TStackRef<IQuadooForm> form; TStackRef<CKeyboardBinder> binder;
	TStackRef<IQuadooControl> edit; HWND window; LRESULT result; CGuiResourceMonitor resources(L"Memo and keyboard teardown");
	TStackRef<IQuadooControlRegistry> registry; TStackRef<IQuadooConstructionContext> context;
	TStackRef<IQuadooControlFactory> factory; TStackRef<IQuadooFactoryLifetime> lifetime;
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"ModalKeys\",\"events\":{\"OnCloseQuery\":\"Close\"},\"children\":[{\"type\":\"Memo\",\"name\":\"memo\",\"properties\":{\"Text\":\"Resource\\nTest\"}},{\"type\":\"TextBox\",\"name\":\"edit\",\"properties\":{\"Top\":120}},{\"type\":\"Button\",\"name\":\"OK\",\"properties\":{\"Default\":true,\"Cancel\":true,\"ModalResult\":1,\"Top\":160},\"events\":{\"OnClick\":\"Click\"}}]}",&definition));
	binder.Attach(__new CKeyboardBinder); CheckAlloc(binder); binder->cancelClose = TRUE;
	Check(QuadooCreateForm(binder,NULL,definition,&form)); Check(form->FindControl(RSTRING_CAST(L"edit"),&edit)); Check(KeyWindow(edit,&g_keyboardFocus)); Check(form->GetHandle(&window));
	g_keyboardModal = form; g_keyboardModalTicks = 0; REQUIRE(SetTimer(window,86,15,KeyboardModalTimer));
	Check(form->ShowModal(&result)); REQUIRE(result == IDOK && binder->clicks == 2 && binder->closes == 2 && g_keyboardModalTicks == 2); KillTimer(window,86); g_keyboardModal = NULL; form.Release(); edit.Release();
	Check(QuadooGetControlRegistry(&registry)); Check(Context(96,binder,&context,registry)); Check(registry->FindFactory(RSTRING_CAST(L"Memo"),&factory)); Check(factory->QueryInterface(__uuidof(IQuadooFactoryLifetime),reinterpret_cast<PVOID*>(&lifetime)));
	while(!resources.Complete())
	{
		TArray<HWND> windows;
		{
			TStackRef<IQuadooForm> cycle; TStackRef<IQuadooControl> memo; TStackRef<IQuadooFormKeyboard> keyboard; HWND native, old;
			TStackRef<IQuadooControl> cycleRoot; Check(QuadooCreateControl(context,NULL,definition,&cycleRoot)); Check(cycleRoot->QueryInterface(__uuidof(IQuadooForm),reinterpret_cast<PVOID*>(&cycle)));
			Check(cycle->ShowForm(SW_SHOWNOACTIVATE)); Check(cycle->FindControl(RSTRING_CAST(L"memo"),&memo)); Check(KeyWindow(memo,&old)); ::SetFocus(old);
			Check(cycle->QueryInterface(__uuidof(IQuadooFormKeyboard),reinterpret_cast<PVOID*>(&keyboard))); Check(KeyPump(keyboard));
			Check(KeyBool(memo,L"WordWrap",FALSE)); Check(KeyWindow(memo,&native)); REQUIRE(!IsWindow(old));
			Check(KeyText(memo,L"Unicode \x03a9\nSecond line")); Check(CollectNativeWindows(cycle,windows));
			Check(cycle->Destroy());
		}
		for(sysint n = 0; n < windows.Length(); n++) REQUIRE(!IsWindow(windows[n]));
		UINT live; Check(lifetime->GetLiveCount(&live)); REQUIRE(live == 0);
		Check(resources.AfterCycle());
	}
Cleanup:
	g_keyboardModal = NULL; if(form) form->Destroy();
	return hr;
}
