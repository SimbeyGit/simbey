#include "Runtime.h"

namespace QF
{
const DWORD Stored = QuadooForm::PropertySerializable;
const DWORD Layout = Stored | QuadooForm::PropertyAffectsLayout;
static const PropertySpec controlProperties[] =
{
	{ L"Name", QuadooProperty::String, Stored, 0, 0 },
	{ L"Left", QuadooProperty::Int, Layout, 0, 1 },
	{ L"Top", QuadooProperty::Int, Layout, 0, 2 },
	{ L"Width", QuadooProperty::Int, Layout, 100, 3 },
	{ L"Height", QuadooProperty::Int, Layout, 28, 4 },
	{ L"Visible", QuadooProperty::Boolean, Layout, 1, 5 },
	{ L"Enabled", QuadooProperty::Boolean, Stored, 1, 6 },
	{ L"Align", QuadooProperty::Enum, Layout, 0, 7 },
	{ L"Anchors", QuadooProperty::Flags, Layout, 3, 8 },
	{ L"Margins", QuadooProperty::Rect, Layout, 0, 10 },
	{ L"MinWidth", QuadooProperty::Int, Layout, 0, 12 },
	{ L"MinHeight", QuadooProperty::Int, Layout, 0, 13 },
	{ L"MaxWidth", QuadooProperty::Int, Layout, 0, 14 },
	{ L"MaxHeight", QuadooProperty::Int, Layout, 0, 15 },
	{ L"Font", QuadooProperty::Font, Layout, 0, 19 },
	{ L"ForeColor", QuadooProperty::Color, Stored, static_cast<INT>(0x80000000 | COLOR_WINDOWTEXT), 20 },
	{ L"BackColor", QuadooProperty::Color, Stored, static_cast<INT>(0x80000000 | COLOR_BTNFACE), 21 },
	{ L"PopupMenu", QuadooProperty::ObjectReference, Stored, 0, 1000 }
};
static const PropertySpec windowProperties[] =
{
	{ L"TabOrder", QuadooProperty::Int, Layout, 0, 18 }
};
static const PropertySpec containerProperties[] =
{
	{ L"Padding", QuadooProperty::Rect, Layout, 0, 9 },
	{ L"Caption", QuadooProperty::String, Layout, 0, 16 }
};
static const PropertySpec containerSize[] = { { L"Width", QuadooProperty::Int, Layout, 200, 3 }, { L"Height", QuadooProperty::Int, Layout, 150, 4 }, { L"AutoSize", QuadooProperty::Boolean, Layout, 0, 11 } };
static const EnumSpec borders[] = { { L"None",0 }, { L"Single",1 }, { L"Sizeable",2 }, { L"Dialog",3 } };
static const EnumSpec states[] = { { L"Normal",0 }, { L"Minimized",1 }, { L"Maximized",2 } };
static const EnumSpec formStyles[] = { { L"Normal",0 }, { L"StayOnTop",1 } };
static const EnumSpec borderIcons[] = { { L"SystemMenu",1 }, { L"Minimize",2 }, { L"Maximize",4 }, { L"Close",8 } };
static const PropertySpec formProperties[] =
{
	{ L"Width",QuadooProperty::Int,Layout,620,3 }, { L"Height",QuadooProperty::Int,Layout,420,4 },
	{ L"Visible",QuadooProperty::Boolean,Layout,0,5 }, { L"ModalResult",QuadooProperty::Long,QuadooForm::PropertyRuntimeOnly,0,23 },
	{ L"BorderStyle",QuadooProperty::Enum,Layout,QuadooForm::BorderSizeable,24,borders,ARRAYSIZE(borders) },
	{ L"BorderIcons",QuadooProperty::Flags,Layout,15,25,borderIcons,ARRAYSIZE(borderIcons) },
	{ L"WindowState",QuadooProperty::Enum,Stored,0,26,states,ARRAYSIZE(states) },
	{ L"FormStyle",QuadooProperty::Enum,Stored,0,27,formStyles,ARRAYSIZE(formStyles) },
	{ L"AutoScroll",QuadooProperty::Boolean,Layout,0,28 }, { L"Icon",QuadooProperty::Icon,Stored,0,29 },
	{ L"KeyPreview",QuadooProperty::Boolean,Stored,0,30 },
	{ L"MainMenu",QuadooProperty::ObjectReference,Stored,0,1001 }
};
static const PropertySpec labelProperties[] =
{
	{ L"Caption", QuadooProperty::String, Layout, 0, 16 },
	{ L"AutoSize", QuadooProperty::Boolean, Layout, 0, 11 }
};
static const PropertySpec buttonProperties[] =
{
	{ L"Caption", QuadooProperty::String, Layout, 0, 16 }
};
static const EnumSpec modalResults[] = { { L"mrNone",0 }, { L"mrOk",IDOK }, { L"mrCancel",IDCANCEL }, { L"mrAbort",IDABORT }, { L"mrRetry",IDRETRY }, { L"mrIgnore",IDIGNORE }, { L"mrYes",IDYES }, { L"mrNo",IDNO } };
static const PropertySpec modalButton[] = { { L"ModalResult",QuadooProperty::Long,Stored,0,24,modalResults,ARRAYSIZE(modalResults) }, { L"Default",QuadooProperty::Boolean,Stored,0,25 }, { L"Cancel",QuadooProperty::Boolean,Stored,0,26 } };
static const PropertySpec textProperties[] =
{
	{ L"BackColor", QuadooProperty::Color, Stored, static_cast<INT>(0x80000000 | COLOR_WINDOW), 21 },
	{ L"Text", QuadooProperty::String, Stored, 0, 17 },
	{ L"Password", QuadooProperty::Boolean, Stored, 0, 24 }
};
static const PropertySpec scrollProperties[] =
{
	{ L"AutoScroll", QuadooProperty::Boolean, Layout, 1, 22 }
};
static const EnumSpec checkStates[] = { { L"Unchecked", BST_UNCHECKED }, { L"Checked", BST_CHECKED }, { L"Indeterminate", BST_INDETERMINATE } };
static const PropertySpec checkProperties[] =
{
	{ L"CheckState", QuadooProperty::Enum, Stored, BST_UNCHECKED, 24, checkStates, ARRAYSIZE(checkStates) },
	{ L"ThreeState", QuadooProperty::Boolean, Stored, 0, 25 }
};
static const PropertySpec radioProperties[] = { { L"Checked", QuadooProperty::Boolean, Stored, 0, 24 } };
static const ArgumentSpec clickArgs[] = { { L"Handled", QuadooForm::ValueBool, TRUE } };
static const ArgumentSpec closeArgs[] = { { L"Cancel", QuadooForm::ValueBool, TRUE } };
static const ArgumentSpec keyArgs[] = { { L"Key", QuadooForm::ValueInt, FALSE }, { L"Handled", QuadooForm::ValueBool, TRUE } };
static const EventSpec controlEvents[] = { { L"OnCreate",0,0,NULL,0 }, { L"OnDestroy",0,0,NULL,1 }, { L"OnShow",0,0,NULL,6 }, { L"OnHide",0,0,NULL,7 } };
static const EventSpec windowEvents[] =
{
	{ L"OnFocus",0,0,NULL,8 },
	{ L"OnBlur",0,0,NULL,9 },
	{ L"OnKeyDown",0,2,keyArgs,10 },
	{ L"OnKeyUp",0,2,keyArgs,11 },
	{ L"OnKeyPress",0,2,keyArgs,15 },
	{ L"OnEnter",0,0,NULL,16 },
	{ L"OnLeave",0,0,NULL,17 }
};
static const EventSpec clickEvents[] = { { L"OnClick",0,1,clickArgs,2 } };
static const EventSpec textEvents[] = { { L"OnChange",0,0,NULL,3 } };
static const EventSpec formEvents[] = { { L"OnCloseQuery",QuadooForm::EventCancellable,1,closeArgs,4 }, { L"OnClose",0,0,NULL,5 } };
static const TypeSpec controlType = { NULL,NULL,controlProperties,ARRAYSIZE(controlProperties),controlEvents,ARRAYSIZE(controlEvents),0 };
static const TypeSpec windowType = { NULL,&controlType,windowProperties,ARRAYSIZE(windowProperties),windowEvents,ARRAYSIZE(windowEvents),0 };
static const TypeSpec containerType = { NULL,&windowType,containerProperties,ARRAYSIZE(containerProperties),NULL,0,0 };
static const TypeSpec sizedContainerType = { NULL,&containerType,containerSize,ARRAYSIZE(containerSize),NULL,0,0 };
const TypeSpec FormType = { L"Form",&containerType,formProperties,ARRAYSIZE(formProperties),formEvents,ARRAYSIZE(formEvents),IDI_PALETTE_FORM };
const TypeSpec FrameType = { L"Frame",&sizedContainerType,NULL,0,NULL,0,IDI_PALETTE_FRAME };
const TypeSpec PanelType = { L"Panel",&sizedContainerType,NULL,0,NULL,0,IDI_PALETTE_PANEL };
const TypeSpec ScrollingPanelType = { L"ScrollingPanel",&PanelType,scrollProperties,ARRAYSIZE(scrollProperties),NULL,0,IDI_PALETTE_SCROLLINGPANEL };
const TypeSpec LabelType = { L"Label",&controlType,labelProperties,ARRAYSIZE(labelProperties),clickEvents,ARRAYSIZE(clickEvents),IDI_PALETTE_LABEL };
static const EnumSpec imageModes[] = { {L"Normal",0},{L"Stretch",1},{L"Contain",2},{L"Cover",3},{L"Tile",4} };
static const EnumSpec imageAlignments[] = { {L"TopLeft",0},{L"TopCenter",1},{L"TopRight",2},{L"MiddleLeft",3},{L"Center",4},{L"MiddleRight",5},{L"BottomLeft",6},{L"BottomCenter",7},{L"BottomRight",8} };
static const EnumSpec imageFilters[] = { {L"Nearest",0},{L"Bilinear",1},{L"Bicubic",2},{L"HighQuality",3} };
static const PropertySpec imageProperties[] =
{
	{ L"Width",QuadooProperty::Int,Layout,100,3 }, { L"Height",QuadooProperty::Int,Layout,100,4 },
	{ L"AutoSize",QuadooProperty::Boolean,Layout,0,11 },
	{ L"Picture",QuadooProperty::Picture,Layout,0,24 },
	{ L"SizeMode",QuadooProperty::Enum,Stored,2,25,imageModes,ARRAYSIZE(imageModes) },
	{ L"ImageAlignment",QuadooProperty::Enum,Stored,4,26,imageAlignments,ARRAYSIZE(imageAlignments) },
	{ L"Interpolation",QuadooProperty::Enum,Stored,3,27,imageFilters,ARRAYSIZE(imageFilters) },
	{ L"Transparent",QuadooProperty::Boolean,Stored,1,28 },
	{ L"Opacity",QuadooProperty::Int,Stored,255,29,NULL,0,NULL,0,255,TRUE },
	{ L"TileOffsetX",QuadooProperty::Int,Stored,0,30,NULL,0,NULL,-1000000,1000000,TRUE },
	{ L"TileOffsetY",QuadooProperty::Int,Stored,0,31,NULL,0,NULL,-1000000,1000000,TRUE },
	{ L"ImageWidth",QuadooProperty::Int,QuadooForm::PropertyReadOnly,0,32 },
	{ L"ImageHeight",QuadooProperty::Int,QuadooForm::PropertyReadOnly,0,33 },
	{ L"HasAlpha",QuadooProperty::Boolean,QuadooForm::PropertyReadOnly,0,34 },
	{ L"ImageFormat",QuadooProperty::String,QuadooForm::PropertyReadOnly,0,35,NULL,0,L"None" },
	{ L"ImageSize",QuadooProperty::Int,QuadooForm::PropertyReadOnly,0,36 }
};
const TypeSpec ImageType = { L"Image",&controlType,imageProperties,ARRAYSIZE(imageProperties),clickEvents,ARRAYSIZE(clickEvents),IDI_PALETTE_IMAGE };
static const EnumSpec swatchModes[] = { {L"Fixed",0},{L"Editable",1} };
static const PropertySpec swatchProperties[] =
{
	{ L"Width",QuadooProperty::Int,Layout,160,3 }, { L"Height",QuadooProperty::Int,Layout,36,4 },
	{ L"Mode",QuadooProperty::Enum,Stored,0,24,swatchModes,ARRAYSIZE(swatchModes) },
	{ L"Colors",QuadooProperty::Collection,Stored,0,25,NULL,0,L"[-2147483633]" },
	{ L"Selected",QuadooProperty::Int,Stored,-1,26,NULL,0,NULL,-1,255,TRUE },
	{ L"Labels",QuadooProperty::List,Stored,0,27 }
};
const TypeSpec ColorSwatchType = { L"ColorSwatch",&controlType,swatchProperties,ARRAYSIZE(swatchProperties),clickEvents,ARRAYSIZE(clickEvents),IDI_PALETTE_COLORSWATCH };
static const TypeSpec captionButtonType = { NULL,&windowType,buttonProperties,ARRAYSIZE(buttonProperties),clickEvents,ARRAYSIZE(clickEvents),0 };
const TypeSpec ButtonType = { L"Button",&captionButtonType,modalButton,ARRAYSIZE(modalButton),NULL,0,IDI_PALETTE_BUTTON };
const TypeSpec TextBoxType = { L"TextBox",&windowType,textProperties,ARRAYSIZE(textProperties),textEvents,ARRAYSIZE(textEvents),IDI_PALETTE_TEXTBOX };
static const EnumSpec memoScrollBars[] = { { L"None",0 }, { L"Horizontal",1 }, { L"Vertical",2 }, { L"Both",3 } };
static const PropertySpec memoProperties[] =
{
	{ L"Text",QuadooProperty::Memo,Stored,0,17 }, { L"BackColor",QuadooProperty::Color,Stored,static_cast<INT>(0x80000000 | COLOR_WINDOW),21 },
	{ L"Width",QuadooProperty::Int,Layout,150,3 }, { L"Height",QuadooProperty::Int,Layout,100,4 },
	{ L"ReadOnly",QuadooProperty::Boolean,Stored,0,24 }, { L"WordWrap",QuadooProperty::Boolean,Stored,1,25 },
	{ L"ScrollBars",QuadooProperty::Enum,Stored,2,26,memoScrollBars,ARRAYSIZE(memoScrollBars) },
	{ L"WantTabs",QuadooProperty::Boolean,Stored,0,27 }
};
const TypeSpec MemoType = { L"Memo",&windowType,memoProperties,ARRAYSIZE(memoProperties),textEvents,ARRAYSIZE(textEvents),IDI_PALETTE_MEMO };
static const TypeSpec checkedButtonType = { NULL,&captionButtonType,NULL,0,textEvents,ARRAYSIZE(textEvents),0 };
const TypeSpec CheckBoxType = { L"CheckBox",&checkedButtonType,checkProperties,ARRAYSIZE(checkProperties),NULL,0,IDI_PALETTE_CHECKBOX };
const TypeSpec RadioButtonType = { L"RadioButton",&checkedButtonType,radioProperties,ARRAYSIZE(radioProperties),NULL,0,IDI_PALETTE_RADIOBUTTON };
const TypeSpec GroupBoxType = { L"GroupBox",&sizedContainerType,NULL,0,NULL,0,IDI_PALETTE_GROUPBOX };
static const PropertySpec itemProperties[] =
{
	{ L"Items", QuadooProperty::List, Stored, 0, 24 },
	{ L"ItemIndex", QuadooProperty::Int, Stored, -1, 25 },
	{ L"BackColor", QuadooProperty::Color, Stored, static_cast<INT>(0x80000000 | COLOR_WINDOW), 21 },
	{ L"Width", QuadooProperty::Int, Layout, 150, 3 }
};
static const EventSpec selectionEvents[] = { { L"OnSelectionChange",0,0,NULL,12 } };
static const TypeSpec itemType = { NULL,&windowType,itemProperties,ARRAYSIZE(itemProperties),selectionEvents,ARRAYSIZE(selectionEvents),0 };
static const EnumSpec comboStyles[] = { { L"DropDown", QuadooForm::DropDown }, { L"DropDownList", QuadooForm::DropDownList } };
static const PropertySpec comboProperties[] =
{
	{ L"Text", QuadooProperty::String, Stored, 0, 17 },
	{ L"Style", QuadooProperty::Enum, Stored, QuadooForm::DropDown, 26, comboStyles, ARRAYSIZE(comboStyles) },
	{ L"DropDownHeight", QuadooProperty::Int, Layout, 160, 27 }
};
static const PropertySpec listProperties[] = { { L"Height", QuadooProperty::Int, Layout, 100, 4 } };
const TypeSpec ComboBoxType = { L"ComboBox",&itemType,comboProperties,ARRAYSIZE(comboProperties),textEvents,ARRAYSIZE(textEvents),IDI_PALETTE_COMBOBOX };
const TypeSpec ListBoxType = { L"ListBox",&itemType,listProperties,ARRAYSIZE(listProperties),NULL,0,IDI_PALETTE_LISTBOX };
static const PropertySpec tabProperties[] =
{
	{ L"Width", QuadooProperty::Int, Layout, 300, 3 }, { L"Height", QuadooProperty::Int, Layout, 200, 4 },
	{ L"ActivePageIndex", QuadooProperty::Int, Stored, -1, 24 },
	{ L"MultiLine", QuadooProperty::Boolean, Layout, 0, 25 }, { L"HotTrack", QuadooProperty::Boolean, Stored, 0, 26 }
};
static const EventSpec tabEvents[] = { { L"OnChanging",QuadooForm::EventCancellable,1,closeArgs,12 }, { L"OnChange",0,0,NULL,3 } };
static const PropertySpec pageProperties[] = { { L"TabVisible",QuadooProperty::Boolean,Layout,1,24 }, { L"Width",QuadooProperty::Int,Layout,200,3 }, { L"Height",QuadooProperty::Int,Layout,150,4 } };
const TypeSpec TabControlType = { L"TabControl",&containerType,tabProperties,ARRAYSIZE(tabProperties),tabEvents,ARRAYSIZE(tabEvents),IDI_PALETTE_TABCONTROL };
const TypeSpec TabPageType = { L"TabPage",&containerType,pageProperties,ARRAYSIZE(pageProperties),NULL,0,IDI_PALETTE_TABPAGE };
static PCWSTR const alignNames[] = { L"None", L"Top", L"Bottom", L"Left", L"Right", L"Client" };
static PCWSTR const anchorNames[] = { L"Left", L"Top", L"Right", L"Bottom" };
BOOL SameString(RSTRING a, RSTRING b)
{
	INT result;
	return SUCCEEDED(RStrCompareRStr(a, b, &result)) && 0 == result;
}

UINT SystemDPI(HWND hwnd)
{
	typedef UINT (WINAPI* GET_DPI)(HWND);
	GET_DPI getDPI = reinterpret_cast<GET_DPI>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
	UINT dpi = hwnd && getDPI ? getDPI(hwnd) : 0;
	if(!dpi)
	{
		HDC dc = GetDC(hwnd);
		if(dc) { dpi = GetDeviceCaps(dc, LOGPIXELSX); ReleaseDC(hwnd, dc); }
	}
	return dpi ? dpi : 96;
}
BOOL AdjustForWindowDPI(RECT* rect, DWORD style, DWORD extended, HWND window)
{
	typedef BOOL (WINAPI* ADJUST_DPI)(LPRECT, DWORD, BOOL, DWORD, UINT);
	ADJUST_DPI adjust = reinterpret_cast<ADJUST_DPI>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "AdjustWindowRectExForDpi"));
	// Nonclient metrics follow the native window DPI, even when a context uses a
	// different explicit control scale for previews or deterministic layout tests.
	return adjust ? adjust(rect, style, FALSE, extended, SystemDPI(window)) : AdjustWindowRectEx(rect, style, FALSE, extended);
}

HRESULT CloneObject(IJSONObject* source, IJSONObject** result)
{
	HRESULT hr;
	TStackRef<IJSONValue> clone;
	CheckIf(!result, E_POINTER);
	*result = NULL;
	CheckIf(!source, E_INVALIDARG);
	Check(JSONCloneObject(source, &clone, TRUE));
	Check(clone->GetObject(result));
Cleanup:
	return hr;
}
HRESULT GuidString(REFGUID id, RSTRING* text)
{
	HRESULT hr;
	CRString raw;
	CheckIf(!text, E_POINTER); *text = NULL;
	Check(RStrFromGUID(id, &raw));
	// The repository helper counts the terminator in its RSTRING length.
	// JSON GUID parsing requires 36 or 38 characters, excluding the terminator.
	Check(RStrCreateW(TStrLenAssert(raw.c_wstr()), raw.c_wstr(), text));
Cleanup:
	return hr;
}
HRESULT PutInt(IJSONObject* object, PCWSTR name, INT value)
{
	HRESULT hr;
	TStackRef<IJSONValue> json;
	Check(JSONCreateInteger(value, &json));
	Check(object->AddValueW(name, json));
Cleanup:
	return hr;
}
HRESULT PutString(IJSONObject* object, PCWSTR name, RSTRING value)
{
	HRESULT hr;
	TStackRef<IJSONValue> json;
	Check(JSONCreateString(value, &json));
	Check(object->AddValueW(name, json));
Cleanup:
	return hr;
}
HRESULT PutObject(IJSONObject* object, PCWSTR name, IJSONObject* value)
{
	HRESULT hr;
	TStackRef<IJSONValue> json;
	Check(JSONWrapObject(value, &json));
	Check(object->AddValueW(name, json));
Cleanup:
	return hr;
}
HRESULT ReadInt(IJSONObject* object, PCWSTR name, INT initial, INT* value)
{
	HRESULT hr = S_OK;
	TStackRef<IJSONValue> json;
	*value = initial;
	if(object->HasField(RSTRING_CAST(name)))
	{
		Check(object->FindNonNullValueW(name, &json));
		Check(json->GetInteger(value));
	}
Cleanup:
	return hr;
}
HRESULT ReadString(IJSONObject* object, PCWSTR name, PCWSTR initial, RSTRING* value)
{
	HRESULT hr;
	TStackRef<IJSONValue> json;
	*value = NULL;
	if(object->HasField(RSTRING_CAST(name)))
	{
		Check(object->FindNonNullValueW(name, &json));
		Check(json->GetString(value));
	}
	else
		Check(RStrCreateW(TStrLenAssert(initial), initial, value));
Cleanup:
	return hr;
}

static BOOL IsProperty(const PropertySpec* spec, PCWSTR name) { return 0 == TStrCmpAssert(spec->name, name); }
HRESULT DefaultValue(const PropertySpec* spec, IJSONValue** value)
{
	HRESULT hr;
	TStackRef<IJSONObject> object;
	INT initial;
	CheckIf(!value, E_POINTER);
	*value = NULL;
	CheckIf(!spec || !spec->name, E_INVALIDARG);
	initial = spec->initial;
	if(IsProperty(spec, L"Padding") || IsProperty(spec, L"Margins"))
	{
		Check(JSONCreateObject(&object));
		Check(PutInt(object, L"Left", 0)); Check(PutInt(object, L"Top", 0));
		Check(PutInt(object, L"Right", 0)); Check(PutInt(object, L"Bottom", 0));
		Check(JSONWrapObject(object, value));
	}
	else if(IsProperty(spec, L"Font"))
	{
		TStackRef<IJSONValue> bold;
		Check(JSONCreateObject(&object));
		Check(PutString(object, L"Name", RSTRING_CAST(L"Segoe UI")));
		Check(PutInt(object, L"Size", 9));
		Check(JSONCreateBool(false, &bold));
		Check(object->AddValueW(L"Bold", bold)); Check(object->AddValueW(L"Italic", bold));
		Check(JSONWrapObject(object, value));
	}
	else if(spec->type == QuadooProperty::Collection && spec->initialText)
		Check(JSONParse(NULL,spec->initialText,TStrLenAssert(spec->initialText),value));
	else if(spec->type == QuadooProperty::List || spec->type == QuadooProperty::Collection)
	{
		TStackRef<IJSONArray> array; Check(JSONCreateArray(&array)); Check(JSONWrapArray(array, value));
	}
	else if(spec->type == QuadooProperty::Icon || spec->type == QuadooProperty::Picture || spec->type == QuadooProperty::ObjectReference)
		{ *value = NULL; hr = S_OK; }
	else if(spec->type == QuadooProperty::String || spec->type == QuadooProperty::Memo)
		Check(JSONCreateString(RSTRING_CAST(spec->initialText ? spec->initialText : L""),value));
	else if(spec->type == QuadooProperty::Boolean)
		Check(JSONCreateBool(initial != 0, value));
	else if(spec->type == QuadooProperty::Long)
		Check(JSONCreateLongInteger(initial, value));
	else
		Check(JSONCreateInteger(initial, value));
Cleanup:
	return hr;
}

HRESULT NormalizeValue(const PropertySpec* spec, IJSONValue* value, IJSONValue** normalized)
{
	HRESULT hr;
	INT number = 0;
	LONGLONG longNumber;
	bool boolean;
	TStackRef<IJSONObject> object, clone;
	CRString text;
	CheckIf(!normalized, E_POINTER);
	*normalized = NULL;
	CheckIf(!spec || !spec->name || (!value && spec->type != QuadooProperty::Icon && spec->type != QuadooProperty::Picture && spec->type != QuadooProperty::ObjectReference), E_INVALIDARG);
	if(spec->type == QuadooProperty::ObjectReference)
	{
		Check(JSONClone(value,normalized,TRUE));
	}
	else if(spec->type == QuadooProperty::Picture)
	{
		CheckIf(value && value->GetType() != JSON::Object,E_INVALIDARG);
		Check(JSONClone(value,normalized,TRUE));
	}
	else if(spec->type == QuadooProperty::Icon)
		Check(NormalizeIcon(value,normalized));
	else if(spec->type == QuadooProperty::Collection)
	{
		CheckIf(value->GetType() != JSON::Array && value->GetType() != JSON::Object,E_INVALIDARG);
		Check(JSONClone(value,normalized,TRUE));
	}
	else if(spec->type == QuadooProperty::List)
		Check(NormalizeItems(value, normalized));
	else if(spec->choiceCount && value->GetType() == JSON::String)
	{
		Check(value->GetString(&text));
		UINT n;
		for(n = 0; n < spec->choiceCount; n++) if(SameString(text, RSTRING_CAST(spec->choices[n].name))) break;
		CheckIf(n == spec->choiceCount, E_INVALIDARG);
		if(spec->type == QuadooProperty::Long) { Check(JSONCreateLongInteger(spec->choices[n].value,normalized)); }
		else { Check(JSONCreateInteger(spec->choices[n].value, normalized)); }
	}
	else if(IsProperty(spec, L"Align") && value->GetType() == JSON::String)
	{
		Check(value->GetString(&text));
		for(number = 0; number < ARRAYSIZE(alignNames); number++)
			if(SameString(text, RSTRING_CAST(alignNames[number]))) break;
		CheckIf(number == ARRAYSIZE(alignNames), E_INVALIDARG);
		Check(JSONCreateInteger(number, normalized));
	}
	else if(IsProperty(spec, L"Padding") || IsProperty(spec, L"Margins") || IsProperty(spec, L"Font"))
	{
		Check(value->GetObject(&object));
		for(sysint field = 0; field < object->Count(); field++)
		{
			CRString fieldName; Check(object->GetValueName(field, &fieldName));
			BOOL known = IsProperty(spec, L"Font") ?
				SameString(fieldName, RSTRING_CAST(L"Name")) || SameString(fieldName, RSTRING_CAST(L"Size")) || SameString(fieldName, RSTRING_CAST(L"Bold")) || SameString(fieldName, RSTRING_CAST(L"Italic")) :
				SameString(fieldName, RSTRING_CAST(L"Left")) || SameString(fieldName, RSTRING_CAST(L"Top")) || SameString(fieldName, RSTRING_CAST(L"Right")) || SameString(fieldName, RSTRING_CAST(L"Bottom"));
			CheckIf(!known, E_INVALIDARG);
		}
		if(!IsProperty(spec, L"Font"))
		{
			static PCWSTR const names[] = { L"Left", L"Top", L"Right", L"Bottom" };
			CheckIf(object->Count() > 4, E_INVALIDARG);
			Check(JSONCreateObject(&clone));
			for(UINT n = 0; n < 4; n++)
			{
				Check(ReadInt(object, names[n], 0, &number));
				CheckIf(number < 0 || number > 1000000, E_INVALIDARG);
				Check(PutInt(clone, names[n], number));
			}
		}
		else
		{
			TStackRef<IJSONValue> flag;
			Check(ReadString(object, L"Name", L"Segoe UI", &text));
			CheckIf(!RStrIsWide(static_cast<RSTRING>(text)) || RStrLen(text) >= LF_FACESIZE, E_INVALIDARG);
			Check(ReadInt(object, L"Size", 9, &number));
			CheckIf(number < 1 || number > 256, E_INVALIDARG);
			Check(CloneObject(object, &clone));
			Check(PutString(clone, L"Name", text)); Check(PutInt(clone, L"Size", number));
			static PCWSTR const flags[] = { L"Bold", L"Italic" };
			for(UINT n = 0; n < 2; n++)
			{
				boolean = false;
				if(object->HasField(RSTRING_CAST(flags[n])))
				{
					Check(object->FindNonNullValueW(flags[n], &flag));
					Check(flag->GetBoolean(&boolean)); flag.Release();
				}
				Check(JSONCreateBool(boolean, &flag)); Check(clone->AddValueW(flags[n], flag)); flag.Release();
			}
		}
		Check(JSONWrapObject(clone, normalized));
	}
	else if(spec->type == QuadooProperty::Color)
	{
		// JSON parses -2147483648 (COLOR_SCROLLBAR) as a LongInteger.
		if(value->GetType() == JSON::LongInteger)
		{
			Check(value->GetLongInteger(&longNumber));
			CheckIf(longNumber < INT_MIN || longNumber > INT_MAX,E_INVALIDARG);
			number = static_cast<INT>(longNumber);
		}
		else
		{
			Check(value->GetInteger(&number));
		}
		DWORD tag = static_cast<DWORD>(number) & 0xff000000;
		CheckIf(tag != 0 && tag != 0x80000000,E_INVALIDARG);
		Check(JSONCreateInteger(number,normalized));
	}
	else if(spec->type == QuadooProperty::String || spec->type == QuadooProperty::Memo || spec->type == QuadooProperty::Event)
	{
		Check(value->GetString(&text));
		CheckIf(!RStrIsWide(static_cast<RSTRING>(text)), E_INVALIDARG);
		Check(JSONCreateString(text, normalized));
	}
	else if(spec->type == QuadooProperty::Boolean)
	{
		Check(value->GetBoolean(&boolean)); Check(JSONCreateBool(boolean, normalized));
	}
	else if(spec->type == QuadooProperty::Long)
	{
		if(value->GetType() == JSON::Integer) { Check(value->GetInteger(&number)); longNumber = number; }
		else { Check(value->GetLongInteger(&longNumber)); }
		CheckIf(longNumber < LONG_MIN || longNumber > LONG_MAX, E_INVALIDARG);
		Check(JSONCreateLongInteger(longNumber, normalized));
	}
	else
	{
		// Explicit integer ranges own their limits, rather than inheriting the
		// default layout-sized limits. JSON may represent INT_MIN as a long.
		if(spec->type == QuadooProperty::Int && spec->hasRange && value->GetType() == JSON::LongInteger)
		{
			Check(value->GetLongInteger(&longNumber));
			CheckIf(longNumber < spec->minimum || longNumber > spec->maximum,E_INVALIDARG);
			number = static_cast<INT>(longNumber);
		}
		else
		{
			Check(value->GetInteger(&number));
		}
		if(spec->choiceCount && spec->type != QuadooProperty::Flags)
		{
			UINT n;
			for(n = 0; n < spec->choiceCount; n++) if(number == spec->choices[n].value) break;
			CheckIf(n == spec->choiceCount, E_INVALIDARG);
		}
		if(IsProperty(spec,L"BorderIcons")) { CheckIf(number < 0 || (number & ~15),E_INVALIDARG); }
		if(IsProperty(spec, L"Align")) { CheckIf(number < 0 || number > QuadooForm::AlignClient, E_INVALIDARG); }
		else if(IsProperty(spec, L"Anchors")) { CheckIf(number < 0 || (number & ~15), E_INVALIDARG); }
		else if(!spec->hasRange)
		{
			CheckIf(number < -1000000 || number > 1000000,E_INVALIDARG);
		}
		if(spec->hasRange)
		{
			CheckIf(number < spec->minimum || number > spec->maximum,E_INVALIDARG);
		}
		else if(IsProperty(spec, L"ItemIndex") || IsProperty(spec,L"ActivePageIndex")) { CheckIf(number < -1, E_INVALIDARG); }
		else if(!IsProperty(spec, L"Left") && !IsProperty(spec, L"Top")) { CheckIf(number < 0, E_INVALIDARG); }
		Check(JSONCreateInteger(number, normalized));
	}
Cleanup:
	return hr;
}

class CPropertyDescriptor : public CBaseUnknown, public IQuadooPropertyDescriptor
{
	const PropertySpec* m_property;
	const EventSpec* m_event;
	PCWSTR NameW() { return m_event ? m_event->name : m_property->name; }
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooPropertyDescriptor)
	END_UNK_MAP
	CPropertyDescriptor(const PropertySpec* property, const EventSpec* event) : m_property(property), m_event(event) {}
	STDMETHOD(GetName)(RSTRING* name) { return name ? RStrCreateW(TStrLenAssert(NameW()), NameW(), name) : E_POINTER; }
	STDMETHOD(GetDisplayName)(RSTRING* name) { return GetName(name); }
	STDMETHOD(GetCategory)(RSTRING* name) { PCWSTR category = m_event ? L"Events" : (m_property->flags & QuadooForm::PropertyAffectsLayout) ? L"Layout" : L"Appearance"; return name ? RStrCreateW(TStrLenAssert(category), category, name) : E_POINTER; }
	STDMETHOD(GetDescription)(RSTRING* name) { return GetName(name); }
	STDMETHOD(GetType)(QuadooProperty::Type* type) { if(type) *type = m_event ? QuadooProperty::Event : m_property->type; return type ? S_OK : E_POINTER; }
	STDMETHOD(GetFlags)(DWORD* flags) { if(flags) *flags = m_event ? Stored : m_property->flags; return flags ? S_OK : E_POINTER; }
	STDMETHOD(GetDefaultValue)(IJSONValue** value) { return !value ? E_POINTER : m_event ? JSONCreateStringW(L"", 0, value) : DefaultValue(m_property, value); }
	STDMETHOD(GetEnumCount)(UINT* count) { if(count) *count = m_property && m_property->choiceCount ? m_property->choiceCount : (m_property && IsProperty(m_property, L"Align")) ? 6 : (m_property && IsProperty(m_property, L"Anchors")) ? 4 : 0; return count ? S_OK : E_POINTER; }
	STDMETHOD(GetEnumValue)(UINT index, RSTRING* name, LONGLONG* value)
	{
		HRESULT hr;
		UINT count;
		CheckIf(!name || !value, E_POINTER); *name = NULL;
		Check(GetEnumCount(&count)); CheckIf(index >= count, HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
		PCWSTR text = m_property->choiceCount ? m_property->choices[index].name : IsProperty(m_property, L"Align") ? alignNames[index] : anchorNames[index];
		Check(RStrCreateW(TStrLenAssert(text), text, name));
		*value = m_property->choiceCount ? m_property->choices[index].value : IsProperty(m_property, L"Align") ? index : (1 << index);
Cleanup:
		return hr;
	}
};

class CPropertyDescriptors : public CBaseUnknown, public IQuadooPropertyDescriptors
{
	TArray<IQuadooPropertyDescriptor*> m_items;
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooPropertyDescriptors)
	END_UNK_MAP
	~CPropertyDescriptors() { for(sysint n = 0; n < m_items.Length(); n++) m_items[n]->Release(); }
	HRESULT Initialize(const TArray<const PropertySpec*>& properties, const TArray<const EventSpec*>& events)
	{
		HRESULT hr = S_OK;
		for(sysint n = 0; n < properties.Length() + events.Length(); n++)
		{
			TStackRef<IQuadooPropertyDescriptor> descriptor;
			descriptor.Attach(__new CPropertyDescriptor(n < properties.Length() ? properties[n] : NULL,
				n < properties.Length() ? NULL : events[n - properties.Length()])); CheckAlloc(descriptor);
			Check(m_items.Append(descriptor)); descriptor.Detach();
		}
Cleanup:
		return hr;
	}
	STDMETHOD(GetCount)(UINT* count) { if(count) *count = static_cast<UINT>(m_items.Length()); return count ? S_OK : E_POINTER; }
	STDMETHOD(GetAt)(UINT index, IQuadooPropertyDescriptor** item)
	{
		HRESULT hr = S_OK;
		CheckIf(!item, E_POINTER); *item = NULL;
		CheckIf(index >= static_cast<UINT>(m_items.Length()), HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
		*item = m_items[index]; (*item)->AddRef();
Cleanup:
		return hr;
	}
	STDMETHOD(Find)(RSTRING name, IQuadooPropertyDescriptor** item)
	{
		HRESULT hr = DISP_E_MEMBERNOTFOUND;
		CheckIf(!item, E_POINTER); *item = NULL;
		for(sysint n = 0; n < m_items.Length(); n++)
		{
			CRString text; Check(m_items[n]->GetName(&text));
			if(SameString(name, text)) { Check(GetAt(static_cast<UINT>(n), item)); goto Cleanup; }
		}
		hr = DISP_E_MEMBERNOTFOUND;
Cleanup:
		return hr;
	}
};

class CEventDescriptor : public CBaseUnknown, public IQuadooEventDescriptor
{
	const EventSpec* m_spec;
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooEventDescriptor)
	END_UNK_MAP
	CEventDescriptor(const EventSpec* spec) : m_spec(spec) {}
	STDMETHOD(GetName)(RSTRING* name) { return name ? RStrCreateW(TStrLenAssert(m_spec->name), m_spec->name, name) : E_POINTER; }
	STDMETHOD(GetCategory)(RSTRING* name) { return name ? RStrCreateW(6, L"Events", name) : E_POINTER; }
	STDMETHOD(GetFlags)(DWORD* flags) { if(flags) *flags = m_spec->flags; return flags ? S_OK : E_POINTER; }
	STDMETHOD(GetArgumentCount)(UINT* count) { if(count) *count = m_spec->count; return count ? S_OK : E_POINTER; }
	STDMETHOD(GetArgument)(UINT index, RSTRING* name, QuadooForm::EventValueType* type, BOOL* writable)
	{
		HRESULT hr;
		CheckIf(!name || !type || !writable, E_POINTER); *name = NULL;
		CheckIf(index >= m_spec->count, HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
		Check(RStrCreateW(TStrLenAssert(m_spec->args[index].name), m_spec->args[index].name, name));
		*type = m_spec->args[index].type; *writable = m_spec->args[index].writable;
Cleanup:
		return hr;
	}
};
class CEventDescriptors : public CBaseUnknown, public IQuadooEventDescriptors
{
	TArray<const EventSpec*> m_items;
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooEventDescriptors)
	END_UNK_MAP
	HRESULT Initialize(const TArray<const EventSpec*>& events)
	{
		HRESULT hr = S_OK;
		for(sysint n = 0; n < events.Length(); n++) { Check(m_items.Append(events[n])); }
Cleanup:
		return hr;
	}
	STDMETHOD(GetCount)(UINT* count) { if(count) *count = static_cast<UINT>(m_items.Length()); return count ? S_OK : E_POINTER; }
	STDMETHOD(GetAt)(UINT index, IQuadooEventDescriptor** item)
	{
		HRESULT hr = S_OK;
		CheckIf(!item, E_POINTER); *item = NULL;
		CheckIf(index >= static_cast<UINT>(m_items.Length()), HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
		*item = __new CEventDescriptor(m_items[index]); CheckAlloc(*item);
Cleanup:
		return hr;
	}
	STDMETHOD(Find)(RSTRING name, IQuadooEventDescriptor** item)
	{
		HRESULT hr = DISP_E_MEMBERNOTFOUND;
		CheckIf(!item, E_POINTER); *item = NULL;
		for(sysint n = 0; n < m_items.Length(); n++)
			if(SameString(name, RSTRING_CAST(m_items[n]->name))) { hr = GetAt(static_cast<UINT>(n), item); break; }
Cleanup:
		return hr;
	}
};
static HRESULT ComposeMetadataLevel(const TypeSpec* type, TArray<const PropertySpec*>& properties, TArray<const EventSpec*>& events, UINT depth)
{
	HRESULT hr = S_OK;
	CheckIf(!type || depth >= 64 || (type->propertyCount && !type->properties) || (type->eventCount && !type->events), QuadooForm::EInvalidDefinition);
	CheckIf(type->propertyCount > 4096 || type->eventCount > 4096, QuadooForm::EInvalidDefinition);
	if(type->base) { Check(ComposeMetadataLevel(type->base, properties, events, depth + 1)); }
	for(UINT p = 0; p < type->propertyCount; p++)
	{
		const PropertySpec* spec = &type->properties[p];
		sysint n;
		CheckIf(!spec->name || !*spec->name, QuadooForm::EInvalidDefinition);
		CheckIf(spec->hasRange && (spec->type != QuadooProperty::Int || spec->minimum > spec->maximum || spec->initial < spec->minimum || spec->initial > spec->maximum),QuadooForm::EInvalidDefinition);
		CheckIf(spec->choiceCount > 4096 || (spec->choiceCount && (!spec->choices || (spec->type != QuadooProperty::Enum && spec->type != QuadooProperty::Flags && spec->type != QuadooProperty::Long))), QuadooForm::EInvalidDefinition);
		for(UINT choice = 0; choice < spec->choiceCount; choice++)
		{
			CheckIf(!spec->choices[choice].name || !*spec->choices[choice].name, QuadooForm::EInvalidDefinition);
			for(UINT previous = 0; previous < choice; previous++)
				CheckIf(spec->choices[previous].value == spec->choices[choice].value || 0 == TStrCmpAssert(spec->choices[previous].name, spec->choices[choice].name), QuadooForm::EInvalidDefinition);
		}
		for(UINT previous = 0; previous < p; previous++) { CheckIf(0 == TStrCmpAssert(type->properties[previous].name, spec->name), QuadooForm::EInvalidDefinition); }
		for(n = 0; n < properties.Length(); n++) if(0 == TStrCmpAssert(properties[n]->name, spec->name)) break;
		if(n < properties.Length())
		{
			CheckIf(properties[n]->type != spec->type || properties[n]->flags != spec->flags || properties[n]->order != spec->order, QuadooForm::EInvalidDefinition);
			CheckIf(properties[n]->choiceCount != spec->choiceCount, QuadooForm::EInvalidDefinition);
			for(UINT choice = 0; choice < spec->choiceCount; choice++)
				CheckIf(properties[n]->choices[choice].value != spec->choices[choice].value || 0 != TStrCmpAssert(properties[n]->choices[choice].name, spec->choices[choice].name), QuadooForm::EInvalidDefinition);
			properties[n] = spec;
		}
		else
		{
			for(n = 0; n < properties.Length() && properties[n]->order < spec->order; n++);
			Check(properties.InsertAt(&spec, n));
		}
	}
	for(UINT e = 0; e < type->eventCount; e++)
	{
		const EventSpec* spec = &type->events[e];
		sysint n;
		CheckIf(!spec->name || !*spec->name || spec->count > 256 || (spec->count && !spec->args), QuadooForm::EInvalidDefinition);
		for(UINT previous = 0; previous < e; previous++) { CheckIf(0 == TStrCmpAssert(type->events[previous].name, spec->name), QuadooForm::EInvalidDefinition); }
		for(n = 0; n < properties.Length(); n++) { CheckIf(0 == TStrCmpAssert(properties[n]->name, spec->name), QuadooForm::EInvalidDefinition); }
		for(n = 0; n < events.Length(); n++) if(0 == TStrCmpAssert(events[n]->name, spec->name)) break;
		if(n < events.Length())
		{
			CheckIf(events[n]->flags != spec->flags || events[n]->order != spec->order || events[n]->count != spec->count, QuadooForm::EInvalidDefinition);
			for(UINT a = 0; a < spec->count; a++)
				CheckIf(!spec->args[a].name || 0 != TStrCmpAssert(events[n]->args[a].name, spec->args[a].name) || events[n]->args[a].type != spec->args[a].type || events[n]->args[a].writable != spec->args[a].writable, QuadooForm::EInvalidDefinition);
			events[n] = spec;
		}
		else
		{
			for(n = 0; n < events.Length() && events[n]->order < spec->order; n++);
			Check(events.InsertAt(&spec, n));
		}
	}
Cleanup:
	return hr;
}
HRESULT ComposeMetadata(const TypeSpec* type, TArray<const PropertySpec*>& properties, TArray<const EventSpec*>& events, BOOL visual)
{
	HRESULT hr;
	Check(ComposeMetadataLevel(type, properties, events, 0));
	// Native storage helpers require the complete shared native descriptor set.
	// Independent controls use only the smaller published base contract.
	for(UINT required = 0; visual && required < ARRAYSIZE(controlProperties); required++)
	{
		sysint n;
		for(n = 0; n < properties.Length(); n++) if(0 == TStrCmpAssert(properties[n]->name, controlProperties[required].name)) break;
		CheckIf(n == properties.Length() || properties[n]->type != controlProperties[required].type || properties[n]->flags != controlProperties[required].flags, QuadooForm::EInvalidDefinition);
	}
	for(sysint n = 0; n < properties.Length(); n++)
	{
		if(properties[n]->choiceCount)
		{
			TStackRef<IJSONValue> initial, normalized;
			Check(DefaultValue(properties[n], &initial)); Check(NormalizeValue(properties[n], initial, &normalized));
		}
	}
Cleanup:
	return hr;
}
HRESULT CreateDescriptors(const TArray<const PropertySpec*>& properties, const TArray<const EventSpec*>& events, IQuadooPropertyDescriptors** descriptors)
{
	HRESULT hr;
	TStackRef<CPropertyDescriptors> items;
	*descriptors = NULL; items.Attach(__new CPropertyDescriptors); CheckAlloc(items);
	Check(items->Initialize(properties, events)); *descriptors = items.Detach();
Cleanup:
	return hr;
}
HRESULT CreateEventDescriptors(const TArray<const EventSpec*>& events, IQuadooEventDescriptors** descriptors)
{
	HRESULT hr;
	TStackRef<CEventDescriptors> items;
	*descriptors = NULL; items.Attach(__new CEventDescriptors); CheckAlloc(items);
	Check(items->Initialize(events)); *descriptors = items.Detach();
Cleanup:
	return hr;
}
}

namespace QF
{
	const TypeSpec* NativeWindowType()
	{
		return &windowType;
	}
}
