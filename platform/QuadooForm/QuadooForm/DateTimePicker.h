#pragma once
#include "Runtime.h"

namespace QF
{
	class CDateTimePicker : public TWindowInterface<CWinControl,IQuadooWinControl>, public IQuadooDateTimePicker
	{
		typedef TWindowInterface<CWinControl,IQuadooWinControl> Super;
		enum Field { DateTime, MinDate, MaxDate, Kind, DateFormat, DateMode, Format, ShowCheckBox, Checked, FieldCount };
		BOOL m_sync;
		BOOL m_mutating;
		ULONG m_version;
		HRESULT ReadFields(TStackRef<IJSONValue>* values);
		HRESULT NormalizeFields(TStackRef<IJSONValue>* values, BOOL initial, BOOL clamp);
		HRESULT InstallFields(TStackRef<IJSONValue>* values);
		HRESULT CommitFields(TStackRef<IJSONValue>* values, BOOL updateNative = TRUE);
		HRESULT Synchronize(BOOL valueOnly = FALSE);
		VOID CalendarColors();
	protected:
		virtual HRESULT CreateNative();
		virtual VOID NativeStyle(PCWSTR* name, DWORD* style, DWORD* extended);
		virtual HRESULT ApplyNative(PCWSTR name);
		virtual HRESULT SetValue(UINT property, QuadooProperty::Type type, IJSONValue* value);
		virtual LRESULT Message(HWND window, UINT message, WPARAM wp, LPARAM lp, BOOL content, BOOL* handled);
	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooDateTimePicker)
			UNK_CHAIN_TO(Super)
		END_UNK_MAP
		CDateTimePicker(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent);
		static const TypeSpec* Type();
		STDMETHOD(GetDateTime)(SYSTEMTIME* value);
		STDMETHOD(SetDateTime)(const SYSTEMTIME* value);
		STDMETHOD(GetChecked)(BOOL* value);
		STDMETHOD(SetChecked)(BOOL value);
		STDMETHOD(HandleMessage)(HWND window, UINT message, WPARAM wp, LPARAM lp, BOOL* handled, LRESULT* result);
	};
	HRESULT RegisterDateTimePickerFactory(IQuadooControlRegistry* registry);
}
