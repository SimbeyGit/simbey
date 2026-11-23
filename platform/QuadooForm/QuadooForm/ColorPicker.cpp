#include "Runtime.h"
#include "ColorNames.h"

namespace
{
	HRESULT Property(IQuadooComponent* component, PCWSTR name, IQuadooProperties** properties, UINT* index)
	{
		HRESULT hr;
		TStackRef<IQuadooProperties> values;
		TStackRef<IQuadooPropertyDescriptors> descriptors;
		UINT count;
		*properties = NULL;
		Check(component->GetProperties(&values));
		Check(values->GetDescriptors(&descriptors));
		Check(descriptors->GetCount(&count));
		for(UINT n = 0; n < count; n++)
		{
			TStackRef<IQuadooPropertyDescriptor> descriptor;
			CRString text;
			Check(descriptors->GetAt(n,&descriptor));
			Check(descriptor->GetName(&text));
			if(text == RSTRING_CAST(name))
			{
				*index = n;
				*properties = values.Detach();
				goto Cleanup;
			}
		}
		hr = DISP_E_MEMBERNOTFOUND;
	Cleanup:
		return hr;
	}

	HRESULT Number(IQuadooComponent* component, PCWSTR name, INT* number)
	{
		HRESULT hr;
		TStackRef<IQuadooProperties> properties;
		UINT index;
		Check(Property(component,name,&properties,&index));
		Check(properties->GetValueInt(index,number));
	Cleanup:
		return hr;
	}

	HRESULT SetColor(IQuadooComponent* component, COLORREF color)
	{
		HRESULT hr;
		TStackRef<IQuadooProperties> properties;
		TStackRef<IJSONArray> colors;
		TStackRef<IJSONValue> number, value;
		UINT index;
		Check(JSONCreateArray(&colors));
		Check(JSONCreateInteger(static_cast<INT>(color),&number));
		Check(colors->Add(number));
		Check(JSONWrapArray(colors,&value));
		Check(Property(component,L"Colors",&properties,&index));
		Check(properties->SetValueJSON(index,value));
	Cleanup:
		return hr;
	}

	HRESULT LoadDefinition(IJSONObject** definition)
	{
		HRESULT hr;
		HRSRC resource = FindResourceW(g_hInstance,MAKEINTRESOURCEW(IDR_COLOR_PICKER),RT_RCDATA);
		HGLOBAL loaded = NULL;
		PCSTR bytes = NULL;
		DWORD size;
		INT length;
		PWSTR write;
		CRString text;
		TStackRef<IJSONValue> value;
		*definition = NULL;
		CheckIf(!resource,HrEnsureLastError());
		size = SizeofResource(g_hInstance,resource);
		loaded = LoadResource(g_hInstance,resource);
		CheckIf(!loaded,HrEnsureLastError());
		bytes = static_cast<PCSTR>(LockResource(loaded));
		CheckIf(!bytes || !size || size > INT_MAX,E_UNEXPECTED);
		length = MultiByteToWideChar(CP_UTF8,0,bytes,size,NULL,0);
		CheckIf(!length,HrEnsureLastError());
		Check(RStrAllocW(length,&text,&write));
		CheckIf(MultiByteToWideChar(CP_UTF8,0,bytes,size,write,length) != length,HrEnsureLastError());
		Check(JSONParse(NULL,text.c_wstr(),length,&value));
		Check(value->GetObject(definition));
	Cleanup:
		if(FAILED(hr) && definition) SafeRelease(*definition);
		return hr;
	}

	BOOL ParseColor(RSTRING text, COLORREF* color)
	{
		PCWSTR digits = RStrToWide(text);
		INT count = RStrLen(text);
		DWORD value = 0;
		while(count && (*digits == L' ' || *digits == L'\t')) { digits++; count--; }
		while(count && (digits[count - 1] == L' ' || digits[count - 1] == L'\t')) count--;
		if(count && *digits == L'#') { digits++; count--; }
		else if(count >= 2 && digits[0] == L'0' && (digits[1] == L'x' || digits[1] == L'X')) { digits += 2; count -= 2; }
		if(count != 6 && count != 8) return FALSE;
		for(INT n = 0; n < count; n++)
		{
			WCHAR ch = TUpperCase(digits[n]);
			if(!((ch >= L'0' && ch <= L'9') || (ch >= L'A' && ch <= L'F'))) return FALSE;
			value = (value << 4) | Formatting::THexToLong(&ch,1);
		}
		if(count == 6)
			value = RGB((value >> 16) & 255,(value >> 8) & 255,value & 255);
		else if((value & 0xff000000) != 0x80000000 || !QuadooColors::SystemColorName(value & 0xffffff)[0])
			return FALSE;
		*color = value;
		return TRUE;
	}

	class CColorPicker;

	class CColorEvent : public CBaseUnknown, public IQuadooDelegate
	{
		CColorPicker* m_picker;
		UINT m_action;
	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooDelegate)
		END_UNK_MAP
		CColorEvent(CColorPicker* picker, UINT action);
		~CColorEvent();
		STDMETHOD(Invoke)(IQuadooEventArgs* args);
	};

	class CColorPicker : public CBaseUnknown, public IQuadooEventsBinder, public IQuadooConstructionReport
	{
		IQuadooForm* m_form;
		BOOL m_updating, m_valid;
		COLORREF m_color;
		HRESULT m_error;
		HRESULT Text(PCWSTR name, PCWSTR property, RSTRING text)
		{
			HRESULT hr;
			TStackRef<IQuadooControl> control;
			TStackRef<IQuadooProperties> values;
			UINT index;
			Check(m_form->FindControl(RSTRING_CAST(name),&control));
			Check(Property(control,property,&values,&index));
			Check(values->SetValueString(index,text));
		Cleanup:
			return hr;
		}

		HRESULT SetCurrent(COLORREF color, BOOL writeText)
		{
			HRESULT hr;
			TStackRef<IQuadooControl> preview;
			CRString text, description;
			m_updating = TRUE;
			Check(m_form->FindControl(RSTRING_CAST(L"CurrentColor"),&preview));
			Check(SetColor(preview,color));
			if((color & 0xff000000) == 0x80000000)
			{
				Check(RStrFormatW(&text,L"0x%.8X",color));
				PCWSTR name = QuadooColors::SystemColorName(color & 0xffffff);
				Check(RStrCreateW(TStrLenAssert(name),name,&description));
			}
			else
			{
				Check(RStrFormatW(&text,L"#%.2X%.2X%.2X",GetRValue(color),GetGValue(color),GetBValue(color)));
				Check(RStrFormatW(&description,L"RGB: %u, %u, %u",GetRValue(color),GetGValue(color),GetBValue(color)));
			}
			if(writeText)
			{
				Check(Text(L"HexColor",L"Text",text));
			}
			Check(Text(L"ColorName",L"Caption",description));
			static PCWSTR const groups[] = { L"DesktopWorkspace",L"WindowsText",L"Buttons3D",L"Menus",L"CaptionsBorders",L"SelectionInfo" };
			for(UINT n = 0; n < ARRAYSIZE(groups); n++)
			{
				TStackRef<IQuadooControl> swatch;
				TStackRef<IQuadooProperties> values;
				TStackRef<IJSONValue> palette;
				TStackRef<IJSONArray> colors;
				UINT index;
				INT selected = -1;
				Check(m_form->FindControl(RSTRING_CAST(groups[n]),&swatch));
				Check(Property(swatch,L"Colors",&values,&index));
				Check(values->GetValueJSON(index,&palette));
				Check(palette->GetArray(&colors));
				for(INT c = 0; c < colors->Count(); c++)
				{
					TStackRef<IJSONValue> item;
					INT raw;
					Check(colors->GetValue(c,&item));
					Check(item->GetInteger(&raw));
					if(static_cast<COLORREF>(raw) == color) selected = c;
				}
				values.Release();
				Check(Property(swatch,L"Selected",&values,&index));
				Check(values->SetValueInt(index,selected));
			}
			m_color = color;
		Cleanup:
			m_updating = FALSE;
			return hr;
		}

		HRESULT Validate(BOOL valid)
		{
			HRESULT hr;
			TStackRef<IQuadooControl> button;
			TStackRef<IQuadooProperties> values;
			UINT index;
			Check(m_form->FindControl(RSTRING_CAST(L"AcceptColor"),&button));
			Check(Property(button,L"Enabled",&values,&index));
			Check(values->SetValueBool(index,valid));
			Check(Text(L"HexHint",L"Caption",RSTRING_CAST(valid ? L"#RRGGBB or 0x800000nn (system color)" : L"Invalid color. Enter six RGB hex digits or a system color.")));
			m_valid = valid;
		Cleanup:
			return hr;
		}

		HRESULT Position(HWND owner)
		{
			HRESULT hr;
			HWND window;
			RECT bounds, anchor, client;
			MONITORINFO monitor = { sizeof(monitor) };
			INT width, height, left, top;
			Check(m_form->GetHandle(&window));
			CheckIf(!GetWindowRect(window,&bounds) || !GetClientRect(window,&client),HrEnsureLastError());
			CheckIf(!GetMonitorInfoW(MonitorFromWindow(owner ? owner : window,MONITOR_DEFAULTTONEAREST),&monitor),HrEnsureLastError());
			anchor = monitor.rcWork;
			if(owner && IsWindowVisible(owner) && !IsIconic(owner)) GetWindowRect(owner,&anchor);
			// The authored dimensions describe the dialog contents. Include the current
			// theme/DPI nonclient frame before applying the monitor work-area limit.
			width = min(2*(bounds.right - bounds.left) - client.right,monitor.rcWork.right - monitor.rcWork.left - 16);
			height = min(2*(bounds.bottom - bounds.top) - client.bottom,monitor.rcWork.bottom - monitor.rcWork.top - 16);
			left = max(monitor.rcWork.left + 8,min((anchor.left + anchor.right - width) / 2,monitor.rcWork.right - width - 8));
			top = max(monitor.rcWork.top + 8,min((anchor.top + anchor.bottom - height) / 2,monitor.rcWork.bottom - height - 8));
			CheckIf(!SetWindowPos(window,NULL,left,top,width,height,SWP_NOZORDER | SWP_NOACTIVATE),HrEnsureLastError());
		Cleanup:
			return hr;
		}

	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooEventsBinder)
			UNK_INTERFACE(IQuadooConstructionReport)
		END_UNK_MAP

		CColorPicker () :
			m_form(NULL), m_updating(TRUE), m_valid(TRUE), m_color(0), m_error(S_OK)
		{
		}

		~CColorPicker ()
		{
			Assert(!m_form);
		}

		STDMETHOD(OnError)(HRESULT error, RSTRING path, RSTRING detail)
		{
			CRString text;
			if(SUCCEEDED(RStrFormatW(&text,L"Color picker: %ls: %ls (0x%.8x)\r\n",RStrToWide(path),RStrToWide(detail),error)))
			{
				OutputDebugStringW(text.c_wstr());
			}
			return S_OK;
		}

		STDMETHOD(PostBind)(IQuadooControl*)
		{
			return S_OK;
		}

		STDMETHOD(BindEvent)(RSTRING name, IQuadooDelegate** handler)
		{
			HRESULT hr = S_OK;
			UINT action;
			*handler = NULL;
			if(CRString(name) == RSTRING_CAST(L"ChooseSwatch")) action = 0;
			else if(CRString(name) == RSTRING_CAST(L"EditHex")) action = 1;
			else if(CRString(name) == RSTRING_CAST(L"CloseQuery")) action = 2;
			else if(CRString(name) == RSTRING_CAST(L"FocusHex")) action = 3;
			else { hr = DISP_E_MEMBERNOTFOUND; goto Cleanup; }
			*handler = __new CColorEvent(this,action);
			CheckAlloc(*handler);
		Cleanup:
			return hr;
		}

		HRESULT Event (UINT action, IQuadooEventArgs* args)
		{
			HRESULT hr = S_OK;
			TStackRef<IQuadooComponent> sender;
			TStackRef<IQuadooProperties> values;
			TStackRef<IJSONValue> value, item;
			TStackRef<IJSONArray> colors;
			CRString text;
			INT number, selected;
			UINT index;
			CheckIfIgnore(m_updating || !m_form,S_OK);
			if(action == 3)
			{
				TStackRef<IQuadooControl> control;
				TStackRef<IQuadooWinControl> edit;
				HWND window;
				Check(m_form->FindControl(RSTRING_CAST(L"HexColor"),&control));
				Check(control->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&edit)));
				Check(edit->GetHandle(&window));
				SetFocus(window);
				SendMessageW(window,EM_SETSEL,0,-1);
			}
			else if(action == 2)
			{
				LONGLONG result;
				Check(Property(m_form,L"ModalResult",&values,&index));
				Check(values->GetValueLong(index,&result));
				if(result == IDOK && !m_valid)
				{
					Check(args->SetBool(0,TRUE));
				}
			}
			else
			{
				Check(args->GetSender(&sender));
				if(action == 0)
				{
					Check(Property(sender,L"Colors",&values,&index));
					Check(values->GetValueJSON(index,&value));
					Check(value->GetArray(&colors));
					Check(Number(sender,L"Selected",&selected));
					// Editable swatches may have no selection; their sole color changed.
					Check(colors->GetValue(max(0,selected),&item));
					Check(item->GetInteger(&number));
					Check(SetCurrent(number,TRUE));
					Check(Validate(TRUE));
				}
				else
				{
					COLORREF color;
					Check(Property(sender,L"Text",&values,&index));
					Check(values->GetValueString(index,&text));
					BOOL valid = ParseColor(text,&color);
					if(valid)
					{
						Check(SetCurrent(color,FALSE));
					}
					Check(Validate(valid));
				}
			}
		Cleanup:
			if(FAILED(hr)) m_error = hr;
			return hr;
		}

		HRESULT Show(HWND owner, COLORREF initial, COLORREF* selected)
		{
			HRESULT hr;
			TStackRef<IJSONObject> definition;
			TStackRef<IQuadooConstructionContext> context;
			TStackRef<IQuadooControl> root;
			QuadooForm::ContextOptions options;
			LRESULT result = 0;
			Check(LoadDefinition(&definition));
			ZeroMemory(&options,sizeof(options));
			options.cbSize = sizeof(options);
			options.Mode = QuadooForm::Runtime;
			options.hwndHost = owner;
			options.pBinder = this;
			options.pReport = this;
			Check(QuadooCreateConstructionContext(&options,&context));
			Check(QuadooCreateControl(context,NULL,definition,&root));
			Check(root->QueryInterface(__uuidof(IQuadooForm),reinterpret_cast<PVOID*>(&m_form)));
			Check(SetCurrent(initial,TRUE));
			Check(Position(owner));
			Check(m_form->ShowModal(&result));
			Check(m_error);
			if(result != IDOK || !m_valid)
				hr = S_FALSE;
		Cleanup:
			if(root)
			{
				HRESULT destroyed = root->Destroy();
				if(FAILED(destroyed) && SUCCEEDED(hr))
					hr = destroyed;
			}
			SafeRelease(m_form);
			if(hr == S_OK)
				*selected = m_color;
			return hr;
		}
	};

	CColorEvent::CColorEvent (CColorPicker* picker, UINT action) : m_picker(picker), m_action(action)
	{
		m_picker->AddRef();
	}

	CColorEvent::~CColorEvent ()
	{
		m_picker->Release();
	}

	STDMETHODIMP CColorEvent::Invoke (IQuadooEventArgs* args)
	{
		return m_picker->Event(m_action,args);
	}
}

extern "C" HRESULT WINAPI ShowQuadooColorPicker (HWND owner, COLORREF initial, COLORREF* selected)
{
	HRESULT hr;
	TStackRef<CColorPicker> picker;
	CheckIf(!selected,E_POINTER);
	CheckIf(owner && (!IsWindow(owner) || GetWindowThreadProcessId(owner,NULL) != GetCurrentThreadId()),E_INVALIDARG);
	CheckIf((initial & 0xff000000) != 0 && ((initial & 0xff000000) != 0x80000000 || !QuadooColors::SystemColorName(initial & 0xffffff)[0]),E_INVALIDARG);
	if(owner) owner = GetAncestor(owner,GA_ROOT);
	picker.Attach(__new CColorPicker);
	CheckAlloc(picker);
	Check(picker->Show(owner,initial,selected));
Cleanup:
	return hr;
}
