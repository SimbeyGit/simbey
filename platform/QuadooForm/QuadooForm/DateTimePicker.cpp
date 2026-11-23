#include "DateTimePicker.h"
#include "DateValues.h"

namespace QF
{
	static PCWSTR const fieldNames[] = {L"DateTime",L"MinDate",L"MaxDate",L"Kind",L"DateFormat",L"DateMode",L"Format",L"ShowCheckBox",L"Checked"};
	static const EnumSpec kinds[] = {{L"Date",0},{L"Time",1}};
	static const EnumSpec formats[] = {{L"Short",0},{L"Long",1}};
	static const EnumSpec modes[] = {{L"DropDown",0},{L"UpDown",1}};
	static const DWORD Stored = QuadooForm::PropertySerializable;
	static const PropertySpec properties[] =
	{
		{L"Width",QuadooProperty::Int,Stored | QuadooForm::PropertyAffectsLayout,170,3},
		{L"Height",QuadooProperty::Int,Stored | QuadooForm::PropertyAffectsLayout,24,4},
		{L"BackColor",QuadooProperty::Color,Stored,static_cast<INT>(0x80000000 | COLOR_WINDOW),21},
		{L"DateTime",QuadooProperty::String,Stored,0,24},
		{L"MinDate",QuadooProperty::String,Stored,0,25},
		{L"MaxDate",QuadooProperty::String,Stored,0,26},
		{L"Kind",QuadooProperty::Enum,Stored,0,27,kinds,ARRAYSIZE(kinds)},
		{L"DateFormat",QuadooProperty::Enum,Stored,0,28,formats,ARRAYSIZE(formats)},
		{L"DateMode",QuadooProperty::Enum,Stored,0,29,modes,ARRAYSIZE(modes)},
		{L"Format",QuadooProperty::String,Stored,0,30},
		{L"ShowCheckBox",QuadooProperty::Boolean,Stored,0,31},
		{L"Checked",QuadooProperty::Boolean,Stored,1,32}
	};
	static const EventSpec events[] =
	{
		{L"OnChange",0,0,NULL,3},
		{L"OnDropDown",0,0,NULL,4},
		{L"OnCloseUp",0,0,NULL,5}
	};
	const TypeSpec* CDateTimePicker::Type()
	{
		static const TypeSpec type = {L"DateTimePicker",NativeWindowType(),properties,ARRAYSIZE(properties),events,ARRAYSIZE(events),IDI_PALETTE_DATETIMEPICKER};
		return &type;
	}
	HRESULT RegisterDateTimePickerFactory(IQuadooControlRegistry* registry)
	{
		HRESULT hr;
		TStackRef<CFactory> factory;
		factory.Attach(__new TFactory<CDateTimePicker>);
		CheckAlloc(factory);
		Check(factory->Initialize());
		Check(registry->RegisterFactory(factory));
	Cleanup:
		return hr;
	}

	CDateTimePicker::CDateTimePicker(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) :
		Super(factory,context,parent), m_sync(FALSE), m_mutating(FALSE), m_version(0)
	{
	}
	HRESULT CDateTimePicker::ReadFields(TStackRef<IJSONValue>* values)
	{
		HRESULT hr = S_OK;
		for(UINT n = 0; n < FieldCount; n++)
		{
			values[n].Release();
			Check(StoredValue(fieldNames[n],&values[n]));
		}
	Cleanup:
		return hr;
	}
	HRESULT CDateTimePicker::NormalizeFields(TStackRef<IJSONValue>* values, BOOL initial, BOOL clamp)
	{
		HRESULT hr;
		CRString format;
		SYSTEMTIME times[3] = {{0},{0},{0}};
		BOOL present[3] = {FALSE,FALSE,FALSE};
		for(UINT n = 0; n < 3; n++)
		{
			CRString text;
			Check(values[n]->GetString(&text));
			present[n] = !text.IsEmpty();
			if(n == DateTime && !present[n])
			{
				CheckIf(!initial,E_INVALIDARG);
				GetLocalTime(&times[DateTime]);
				present[n] = TRUE;
			}
			else if(present[n])
			{
				Check(ParseTime(text,n != DateTime,&times[n]));
			}
		}
		CheckIf(present[MinDate] && present[MaxDate] && DayNumber(times[MinDate]) > DayNumber(times[MaxDate]),E_INVALIDARG);
		for(UINT n = MinDate; n <= MaxDate; n++)
		{
			BOOL outside = present[n] && (n == MinDate ? DayNumber(times[DateTime]) < DayNumber(times[n]) : DayNumber(times[DateTime]) > DayNumber(times[n]));
			if(outside)
			{
				CheckIf(!clamp,E_INVALIDARG);
				times[DateTime].wYear = times[n].wYear;
				times[DateTime].wMonth = times[n].wMonth;
				times[DateTime].wDay = times[n].wDay;
			}
		}
		for(UINT n = 0; n < 3; n++)
		{
			if(present[n])
			{
				TStackRef<IJSONValue> canonical;
				Check(TimeJSON(times[n],n != DateTime,&canonical));
				values[n] = canonical;
			}
		}
		Check(values[Format]->GetString(&format));
		CheckIf((!format.IsEmpty() && !RStrIsWide(static_cast<RSTRING>(format))) || format.Length() > 256,E_INVALIDARG);
		BOOL quote = FALSE;
		for(INT n = 0; n < format.Length(); n++)
		{
			WCHAR ch = format.c_wstr()[n];
			CheckIf(!ch || ch == L'\r' || ch == L'\n' || (!quote && ch == L'X'),E_INVALIDARG);
			if(ch == L'\'')
			{
				quote = !quote;
			}
		}
		CheckIf(quote,E_INVALIDARG);
		bool show;
		bool checked;
		Check(values[ShowCheckBox]->GetBoolean(&show));
		Check(values[Checked]->GetBoolean(&checked));
		CheckIf(!show && !checked,E_INVALIDARG);
	Cleanup:
		return hr;
	}
	HRESULT CDateTimePicker::InstallFields(TStackRef<IJSONValue>* values)
	{
		HRESULT hr = S_OK;
		for(UINT n = 0; n < FieldCount; n++)
		{
			Check(SetStoredValue(fieldNames[n],values[n]));
		}
	Cleanup:
		return hr;
	}
	HRESULT CDateTimePicker::CreateNative()
	{
		HRESULT hr;
		TStackRef<IJSONValue> values[FieldCount];
		INITCOMMONCONTROLSEX controls = {sizeof(controls),ICC_DATE_CLASSES};
		Check(ReadFields(values));
		Check(NormalizeFields(values,TRUE,TRUE));
		for(UINT n = 0; n < FieldCount; n++)
		{
			TStackRef<IJSONValue> previous;
			Check(StoredValue(fieldNames[n],&previous));
			if(JSONCompare(previous,values[n]))
			{
				Check(Dirty(fieldNames[n]));
			}
		}
		Check(InstallFields(values));
		CheckIf(!InitCommonControlsEx(&controls),HrEnsureLastError());
		Check(Super::CreateNative());
		Check(Synchronize());
	Cleanup:
		return hr;
	}
	VOID CDateTimePicker::NativeStyle(PCWSTR* name, DWORD* style, DWORD* extended)
	{
		Super::NativeStyle(name,style,extended);
		*name = DATETIMEPICK_CLASSW;
		*style |= WS_TABSTOP;
		if(Integer(L"Kind") == 1)
		{
			*style |= DTS_TIMEFORMAT;
		}
		else if(Integer(L"DateFormat") == 1)
		{
			*style |= DTS_LONGDATEFORMAT;
		}
		if(Integer(L"DateMode") == 1)
		{
			*style |= DTS_UPDOWN;
		}
		if(Boolean(L"ShowCheckBox"))
		{
			*style |= DTS_SHOWNONE;
		}
	}
	VOID CDateTimePicker::CalendarColors()
	{
		SendMessageW(m_hwnd,DTM_SETMCCOLOR,MCSC_BACKGROUND,ResolveQuadooColor(Integer(L"BackColor")));
		SendMessageW(m_hwnd,DTM_SETMCCOLOR,MCSC_MONTHBK,ResolveQuadooColor(Integer(L"BackColor")));
		SendMessageW(m_hwnd,DTM_SETMCCOLOR,MCSC_TEXT,ResolveQuadooColor(Integer(L"ForeColor")));
	}
	HRESULT CDateTimePicker::Synchronize(BOOL valueOnly)
	{
		HRESULT hr;
		BOOL previous = m_sync;
		SYSTEMTIME range[2] = {{0},{0}};
		SYSTEMTIME value;
		DWORD flags = 0;
		CRString text;
		CheckIf(!m_hwnd,QuadooForm::EDestroyed);
		m_sync = TRUE;
		if(!valueOnly)
		{
			Check(StoredString(L"MinDate",&text));
			if(!text.IsEmpty())
			{
				Check(ParseTime(text,TRUE,range));
				flags |= GDTR_MIN;
			}
			text = static_cast<RSTRING>(NULL);
			Check(StoredString(L"MaxDate",&text));
			if(!text.IsEmpty())
			{
				Check(ParseTime(text,TRUE,range + 1));
				range[1].wHour = 23;
				range[1].wMinute = 59;
				range[1].wSecond = 59;
				range[1].wMilliseconds = 999;
				flags |= GDTR_MAX;
			}
			CheckIf(!SendMessageW(m_hwnd,DTM_SETRANGE,flags,reinterpret_cast<LPARAM>(range)),E_FAIL);
			text = static_cast<RSTRING>(NULL);
			Check(StoredString(L"Format",&text));
			CheckIf(!SendMessageW(m_hwnd,DTM_SETFORMATW,0,text.IsEmpty() ? 0 : reinterpret_cast<LPARAM>(text.c_wstr())),E_FAIL);
		}
		Check(GetDateTime(&value));
		CheckIf(!SendMessageW(m_hwnd,DTM_SETSYSTEMTIME,GDT_VALID,reinterpret_cast<LPARAM>(&value)),E_FAIL);
		if(!Boolean(L"Checked"))
		{
			CheckIf(!SendMessageW(m_hwnd,DTM_SETSYSTEMTIME,GDT_NONE,0),E_FAIL);
		}
		SendMessageW(m_hwnd,DTM_SETMCFONT,reinterpret_cast<WPARAM>(m_font),TRUE);
		CalendarColors();
		hr = S_OK;
	Cleanup:
		m_sync = previous;
		return hr;
	}
	HRESULT CDateTimePicker::CommitFields(TStackRef<IJSONValue>* values, BOOL updateNative)
	{
		HRESULT hr;
		TStackRef<CDateTimePicker> keepAlive(this);
		TStackRef<IJSONValue> previous[FieldCount];
		BOOL changed[FieldCount] = {FALSE};
		BOOL any = FALSE;
		BOOL rebuild = FALSE;
		BOOL valueOnly = TRUE;
		BOOL entered = FALSE;
		CheckIf(m_mutating,QuadooForm::EInvalidState);
		Check(ReadFields(previous));
		for(UINT n = 0; n < FieldCount; n++)
		{
			changed[n] = JSONCompare(previous[n],values[n]) != 0;
			any |= changed[n];
			if(n != DateTime && n != Checked && changed[n])
			{
				valueOnly = FALSE;
			}
		}
		CheckIfIgnore(!any,S_FALSE);
		m_mutating = TRUE;
		entered = TRUE;
		rebuild = changed[Kind] || changed[DateFormat] || changed[DateMode] || changed[ShowCheckBox];
		Check(InstallFields(values));
		hr = !updateNative ? S_OK : rebuild ? ReplaceNative() : Synchronize(valueOnly);
		if(FAILED(hr))
		{
			InstallFields(previous);
			if(m_state < QuadooForm::Destroying && m_hwnd)
			{
				Synchronize();
			}
			goto Cleanup;
		}
		Check(CheckAlive());
		ULONG version = m_version;
		if(changed[DateTime] || changed[Checked])
		{
			version = ++m_version;
		}
		for(UINT n = 0; n < FieldCount; n++)
		{
			if(changed[n])
			{
				Check(Dirty(fieldNames[n]));
			}
		}
		m_mutating = FALSE;
		entered = FALSE;
		Check(FlushNotifications());
		Check(CheckAlive());
		if(version == m_version && (changed[DateTime] || changed[Checked]))
		{
			Check(Dispatch(L"OnChange"));
		}
		hr = S_OK;
	Cleanup:
		if(entered)
		{
			m_mutating = FALSE;
		}
		return hr;
	}
	HRESULT CDateTimePicker::SetValue(UINT property, QuadooProperty::Type type, IJSONValue* value)
	{
		HRESULT hr;
		TStackRef<IJSONValue> values[FieldCount];
		TStackRef<IJSONValue> normalized;
		UINT field = FieldCount;
		Check(CheckAlive());
		CheckIf(m_mutating || m_replacing,QuadooForm::EInvalidState);
		CheckIf(property >= static_cast<UINT>(m_values.Length()),DISP_E_MEMBERNOTFOUND);
		PCWSTR name = m_values[property].spec->name;
		for(UINT n = 0; n < FieldCount; n++)
		{
			if(!lstrcmpW(name,fieldNames[n]))
			{
				field = n;
				break;
			}
		}
		if(field == FieldCount)
		{
			Check(Super::SetValue(property,type,value));
			goto Cleanup;
		}
		CheckIf(type != m_values[property].spec->type && !(type == QuadooProperty::Int && m_values[property].spec->type == QuadooProperty::Enum),DISP_E_TYPEMISMATCH);
		Check(NormalizeValue(m_values[property].spec,value,&normalized));
		Check(ReadFields(values));
		values[field] = normalized;
		if(field == ShowCheckBox)
		{
			bool show;
			Check(normalized->GetBoolean(&show));
			if(!show)
			{
				values[Checked].Release();
				Check(JSONCreateBool(TRUE,&values[Checked]));
			}
		}
		Check(NormalizeFields(values,FALSE,field == MinDate || field == MaxDate));
		Check(CommitFields(values));
	Cleanup:
		return hr;
	}
	HRESULT CDateTimePicker::ApplyNative(PCWSTR name)
	{
		HRESULT hr;
		Check(Super::ApplyNative(name));
		if(m_hwnd && (!lstrcmpW(name,L"BackColor") || !lstrcmpW(name,L"ForeColor")))
		{
			CalendarColors();
		}
		if(m_hwnd && !lstrcmpW(name,L"Font"))
		{
			SendMessageW(m_hwnd,DTM_SETMCFONT,reinterpret_cast<WPARAM>(m_font),TRUE);
		}
	Cleanup:
		return hr;
	}
	LRESULT CDateTimePicker::Message(HWND window, UINT message, WPARAM wp, LPARAM lp, BOOL content, BOOL* handled)
	{
		if(message == WM_SYSCOLORCHANGE || message == WM_THEMECHANGED)
		{
			CalendarColors();
		}
		return Super::Message(window,message,wp,lp,content,handled);
	}
	STDMETHODIMP CDateTimePicker::HandleMessage(HWND window, UINT message, WPARAM wp, LPARAM lp, BOOL* handled, LRESULT* result)
	{
		HRESULT hr;
		TStackRef<CDateTimePicker> keepAlive(this);
		CheckIf(!handled || !result,E_POINTER);
		*handled = FALSE;
		*result = 0;
		Check(CheckAlive());
		if(window == m_hwnd && message == WM_NOTIFY && lp && reinterpret_cast<NMHDR*>(lp)->hwndFrom == m_hwnd)
		{
			UINT code = reinterpret_cast<NMHDR*>(lp)->code;
			if(code == DTN_DATETIMECHANGE || code == DTN_DROPDOWN || code == DTN_CLOSEUP)
			{
				*handled = TRUE;
				CheckIfIgnore(m_sync || m_replacing || m_state != QuadooForm::Ready,S_OK);
				if(code == DTN_DATETIMECHANGE)
				{
					const NMDATETIMECHANGE* change = reinterpret_cast<const NMDATETIMECHANGE*>(lp);
					TStackRef<IJSONValue> values[FieldCount];
					CheckIf(change->dwFlags != GDT_VALID && change->dwFlags != GDT_NONE,E_INVALIDARG);
					Check(ReadFields(values));
					if(change->dwFlags == GDT_VALID)
					{
						values[DateTime].Release();
						Check(TimeJSON(change->st,FALSE,&values[DateTime]));
					}
					values[Checked].Release();
					Check(JSONCreateBool(change->dwFlags == GDT_VALID,&values[Checked]));
					Check(NormalizeFields(values,FALSE,FALSE));
					Check(CommitFields(values,FALSE));
				}
				else
				{
					Check(Dispatch(code == DTN_DROPDOWN ? L"OnDropDown" : L"OnCloseUp"));
				}
				goto Cleanup;
			}
		}
		Check(Super::HandleMessage(window,message,wp,lp,handled,result));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CDateTimePicker::GetDateTime(SYSTEMTIME* value)
	{
		HRESULT hr;
		CRString text;
		CheckIf(!value,E_POINTER);
		ZeroMemory(value,sizeof(*value));
		Check(CheckAlive());
		Check(StoredString(L"DateTime",&text));
		Check(ParseTime(text,FALSE,value));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CDateTimePicker::SetDateTime(const SYSTEMTIME* value)
	{
		HRESULT hr;
		TStackRef<IJSONValue> json;
		Check(CheckAlive());
		CheckIf(!value,E_POINTER);
		Check(TimeJSON(*value,FALSE,&json));
		Check(SetNamedValue(L"DateTime",QuadooProperty::String,json));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CDateTimePicker::GetChecked(BOOL* value)
	{
		HRESULT hr;
		CheckIf(!value,E_POINTER);
		*value = FALSE;
		Check(CheckAlive());
		*value = Boolean(L"Checked");
		hr = S_OK;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CDateTimePicker::SetChecked(BOOL value)
	{
		HRESULT hr;
		TStackRef<IJSONValue> json;
		Check(CheckAlive());
		Check(JSONCreateBool(value != FALSE,&json));
		Check(SetNamedValue(L"Checked",QuadooProperty::Boolean,json));
	Cleanup:
		return hr;
	}
}
