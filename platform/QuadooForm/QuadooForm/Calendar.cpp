#include "Calendar.h"
#include "DateValues.h"

namespace QF
{
	static const DWORD Stored = QuadooForm::PropertySerializable;
	static const PropertySpec properties[] =
	{
		{L"Width",QuadooProperty::Int,Stored | QuadooForm::PropertyAffectsLayout,260,3},
		{L"Height",QuadooProperty::Int,Stored | QuadooForm::PropertyAffectsLayout,205,4},
		{L"BackColor",QuadooProperty::Color,Stored,static_cast<INT>(0x80000000 | COLOR_WINDOW),21},
		{L"Date",QuadooProperty::String,Stored,0,24},
		{L"MinDate",QuadooProperty::String,Stored,0,25},
		{L"MaxDate",QuadooProperty::String,Stored,0,26},
		{L"ShowWeekNumbers",QuadooProperty::Boolean,Stored,0,27},
		{L"ShowToday",QuadooProperty::Boolean,Stored,1,28},
		{L"ShowTodayCircle",QuadooProperty::Boolean,Stored,1,29}
	};
	static const EventSpec events[] =
	{
		{L"OnChange",0,0,NULL,3},
		{L"OnSelect",0,0,NULL,4}
	};
	PCWSTR const CCalendar::s_dateNames[] = {L"Date",L"MinDate",L"MaxDate"};

	CCalendar::CCalendar(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) :
		Super(factory,context,parent), m_sync(FALSE), m_dateVersion(0)
	{
	}

	const TypeSpec* CCalendar::Type()
	{
		static const TypeSpec type = {L"Calendar",NativeWindowType(),properties,ARRAYSIZE(properties),events,ARRAYSIZE(events),IDI_PALETTE_CALENDAR};
		return &type;
	}

	HRESULT RegisterCalendarFactory(IQuadooControlRegistry* registry)
	{
		HRESULT hr;
		TStackRef<CFactory> factory;
		factory.Attach(__new TFactory<CCalendar>);
		CheckAlloc(factory);
		Check(factory->Initialize());
		Check(registry->RegisterFactory(factory));
	Cleanup:
		return hr;
	}

	HRESULT CCalendar::ReadDates(SYSTEMTIME* dates, UINT edited, IJSONValue* replacement)
	{
		HRESULT hr = S_OK;
		static const SYSTEMTIME first = {1753,1,1,1};
		static const SYSTEMTIME last = {9999,12,5,31};
		GetLocalTime(&dates[SelectedDate]);
		dates[SelectedDate].wHour = 0;
		dates[SelectedDate].wMinute = 0;
		dates[SelectedDate].wSecond = 0;
		dates[SelectedDate].wMilliseconds = 0;
		dates[MinDate] = first;
		dates[MaxDate] = last;
		for(UINT field = 0; field < DateCount; field++)
		{
			CRString text;
			if(field == edited)
			{
				Check(replacement->GetString(&text));
			}
			else
			{
				Check(StoredString(s_dateNames[field],&text));
			}
			if(!text.IsEmpty())
			{
				Check(ParseTime(text,TRUE,&dates[field]));
				CheckIf(DayNumber(dates[field]) < DayNumber(first),E_INVALIDARG);
			}
			else
			{
				CheckIf(field == SelectedDate && field == edited,E_INVALIDARG);
			}
		}
		CheckIf(DayNumber(dates[MinDate]) > DayNumber(dates[MaxDate]),E_INVALIDARG);
		for(UINT field = MinDate; field <= MaxDate; field++)
		{
			UINT day = DayNumber(dates[SelectedDate]);
			UINT limit = DayNumber(dates[field]);
			if(field == MinDate ? day < limit : day > limit)
			{
				CheckIf(edited == SelectedDate,E_INVALIDARG);
				dates[SelectedDate] = dates[field];
			}
		}
		// Include the entire last day on common-control versions that compare time.
		dates[MaxDate].wHour = 23;
		dates[MaxDate].wMinute = 59;
		dates[MaxDate].wSecond = 59;
		dates[MaxDate].wMilliseconds = 999;
	Cleanup:
		return hr;
	}

	HRESULT CCalendar::ApplyDates(SYSTEMTIME* dates, BOOL applyLimits)
	{
		HRESULT hr = S_OK;
		BOOL previousSync = m_sync;
		SYSTEMTIME current;
		m_sync = TRUE;
		if(applyLimits)
		{
			CheckIf(!MonthCal_SetRange(m_hwnd,GDTR_MIN | GDTR_MAX,&dates[MinDate]),E_FAIL);
		}
		CheckIf(!MonthCal_GetCurSel(m_hwnd,&current),E_FAIL);
		if(DayNumber(current) != DayNumber(dates[SelectedDate]))
		{
			CheckIf(!MonthCal_SetCurSel(m_hwnd,&dates[SelectedDate]),E_FAIL);
			CheckIf(!MonthCal_GetCurSel(m_hwnd,&current) || DayNumber(current) != DayNumber(dates[SelectedDate]),E_FAIL);
		}
	Cleanup:
		m_sync = previousSync;
		return hr;
	}

	HRESULT CCalendar::CreateNative()
	{
		HRESULT hr;
		INITCOMMONCONTROLSEX controls = {sizeof(controls),ICC_DATE_CLASSES};
		CheckIf(!InitCommonControlsEx(&controls),HrEnsureLastError());
		Check(Super::CreateNative());
	Cleanup:
		return hr;
	}

	VOID CCalendar::NativeStyle(PCWSTR* name, DWORD* style, DWORD* extended)
	{
		Super::NativeStyle(name,style,extended);
		*name = MONTHCAL_CLASSW;
		*style |= WS_TABSTOP;
		if(Boolean(L"ShowWeekNumbers"))
		{
			*style |= MCS_WEEKNUMBERS;
		}
		if(!Boolean(L"ShowToday"))
		{
			*style |= MCS_NOTODAY;
		}
		if(!Boolean(L"ShowTodayCircle"))
		{
			*style |= MCS_NOTODAYCIRCLE;
		}
	}

	HRESULT CCalendar::NativeCreated()
	{
		HRESULT hr;
		SYSTEMTIME dates[DateCount];
		TStackRef<IJSONValue> previous;
		TStackRef<IJSONValue> selected;
		Check(Super::NativeCreated());
		Check(ReadDates(dates,DateCount,NULL));
		Check(TimeJSON(dates[SelectedDate],TRUE,&selected));
		Check(ApplyDates(dates,TRUE));
		Check(StoredValue(s_dateNames[SelectedDate],&previous));
		if(JSONCompare(previous,selected))
		{
			Check(SetStoredValue(s_dateNames[SelectedDate],selected));
			Check(Dirty(s_dateNames[SelectedDate]));
		}
		ApplyColors();
	Cleanup:
		return hr;
	}

	VOID CCalendar::ApplyColors()
	{
		COLORREF background = ResolveQuadooColor(Integer(L"BackColor"));
		MonthCal_SetColor(m_hwnd,MCSC_BACKGROUND,background);
		MonthCal_SetColor(m_hwnd,MCSC_MONTHBK,background);
		MonthCal_SetColor(m_hwnd,MCSC_TEXT,ResolveQuadooColor(Integer(L"ForeColor")));
	}

	HRESULT CCalendar::ApplyNative(PCWSTR name)
	{
		HRESULT hr;
		Check(Super::ApplyNative(name));
		if(m_hwnd)
		{
			if(!lstrcmpW(name,L"ShowWeekNumbers") || !lstrcmpW(name,L"ShowToday") || !lstrcmpW(name,L"ShowTodayCircle"))
			{
				Check(ReplaceNative());
			}
			else if(!lstrcmpW(name,L"BackColor") || !lstrcmpW(name,L"ForeColor"))
			{
				ApplyColors();
			}
		}
	Cleanup:
		return hr;
	}

	HRESULT CCalendar::SetValue(UINT property, QuadooProperty::Type type, IJSONValue* value)
	{
		HRESULT hr;
		TStackRef<CCalendar> keepAlive(this);
		TStackRef<IJSONValue> normalized;
		TStackRef<IJSONValue> stored;
		TStackRef<IJSONValue> clamped;
		SYSTEMTIME previous[DateCount];
		SYSTEMTIME proposed[DateCount];
		UINT field;
		Check(CheckAlive());
		CheckIf(m_sync || m_replacing,QuadooForm::EInvalidState);
		CheckIf(property >= static_cast<UINT>(m_values.Length()),DISP_E_MEMBERNOTFOUND);
		PCWSTR name = m_values[property].spec->name;
		for(field = 0; field < DateCount; field++)
		{
			if(!lstrcmpW(name,s_dateNames[field]))
			{
				break;
			}
		}
		if(field == DateCount)
		{
			Check(Super::SetValue(property,type,value));
			goto Cleanup;
		}
		CheckIf(type != QuadooProperty::String,DISP_E_TYPEMISMATCH);
		Check(NormalizeValue(m_values[property].spec,value,&normalized));
		Check(StoredValue(name,&stored));
		CheckIfIgnore(!JSONCompare(stored,normalized),S_FALSE);
		Check(ReadDates(previous,DateCount,NULL));
		Check(ReadDates(proposed,field,normalized));
		BOOL changed = DayNumber(previous[SelectedDate]) != DayNumber(proposed[SelectedDate]);
		BOOL limitsChanged = field != SelectedDate;
		if(limitsChanged && changed)
		{
			Check(TimeJSON(proposed[SelectedDate],TRUE,&clamped));
		}
		hr = ApplyDates(proposed,limitsChanged);
		if(FAILED(hr))
		{
			ApplyDates(previous,limitsChanged);
			goto Cleanup;
		}
		Check(SetStoredValue(name,normalized));
		Check(Dirty(name));
		if(clamped)
		{
			Check(SetStoredValue(s_dateNames[SelectedDate],clamped));
			Check(Dirty(s_dateNames[SelectedDate]));
		}
		ULONG version = changed ? ++m_dateVersion : m_dateVersion;
		Check(FlushNotifications());
		Check(CheckAlive());
		if(changed && version == m_dateVersion)
		{
			Check(Dispatch(L"OnChange"));
		}
		hr = S_OK;
	Cleanup:
		return hr;
	}

	STDMETHODIMP CCalendar::HandleMessage(HWND window, UINT message, WPARAM wp, LPARAM lp, BOOL* handled, LRESULT* result)
	{
		HRESULT hr;
		TStackRef<CCalendar> keepAlive(this);
		CheckIf(!handled || !result,E_POINTER);
		*handled = FALSE;
		*result = 0;
		Check(CheckAlive());
		if(window == m_hwnd && message == WM_NOTIFY && lp && reinterpret_cast<NMHDR*>(lp)->hwndFrom == m_hwnd)
		{
			UINT code = reinterpret_cast<NMHDR*>(lp)->code;
			if(code == MCN_SELCHANGE || code == MCN_SELECT)
			{
				*handled = TRUE;
				CheckIfIgnore(m_sync || m_replacing || m_state != QuadooForm::Ready,S_OK);
				SYSTEMTIME current;
				TStackRef<IJSONValue> value;
				// Read the live selection. A preceding notification's callback may have changed it.
				CheckIf(!MonthCal_GetCurSel(m_hwnd,&current),E_FAIL);
				Check(TimeJSON(current,TRUE,&value));
				Check(SetNamedValue(s_dateNames[SelectedDate],QuadooProperty::String,value));
				if(code == MCN_SELECT && m_state == QuadooForm::Ready && m_hwnd == window)
				{
					Check(Dispatch(L"OnSelect"));
				}
				goto Cleanup;
			}
		}
		Check(Super::HandleMessage(window,message,wp,lp,handled,result));
	Cleanup:
		return hr;
	}
}
