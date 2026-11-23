#pragma once
#include "Runtime.h"

namespace QF
{
	class CCalendar : public TWindowInterface<CWinControl,IQuadooWinControl>
	{
		typedef TWindowInterface<CWinControl,IQuadooWinControl> Super;
		enum DateField
		{
			SelectedDate,
			MinDate,
			MaxDate,
			DateCount
		};
		static PCWSTR const s_dateNames[DateCount];
		BOOL m_sync;
		ULONG m_dateVersion;

		HRESULT ReadDates(SYSTEMTIME* dates, UINT edited, IJSONValue* replacement);
		HRESULT ApplyDates(SYSTEMTIME* dates, BOOL applyLimits);
		VOID ApplyColors();
	protected:
		virtual HRESULT CreateNative();
		virtual VOID NativeStyle(PCWSTR* name, DWORD* style, DWORD* extended);
		virtual HRESULT NativeCreated();
		virtual HRESULT ApplyNative(PCWSTR name);
		virtual HRESULT SetValue(UINT property, QuadooProperty::Type type, IJSONValue* value);
	public:
		CCalendar(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent);
		static const TypeSpec* Type();
		STDMETHOD(HandleMessage)(HWND window, UINT message, WPARAM wp, LPARAM lp, BOOL* handled, LRESULT* result);
	};

	HRESULT RegisterCalendarFactory(IQuadooControlRegistry* registry);
}
