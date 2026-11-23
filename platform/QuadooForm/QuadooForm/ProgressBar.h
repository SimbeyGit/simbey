#pragma once
#include "Runtime.h"

namespace QF
{
	class CProgressBar :
		public TWindowInterface<CWinControl, IQuadooWinControl>,
		public IQuadooProgressBar
	{
		typedef TWindowInterface<CWinControl, IQuadooWinControl> Super;
		VOID SynchronizeProgress();
		HRESULT ReadInteger(PCWSTR name, INT* value);
		HRESULT WriteInteger(PCWSTR name, INT value);
	protected:
		virtual HRESULT CreateNative();
		virtual VOID NativeStyle(PCWSTR* name, DWORD* style, DWORD* extended);
		virtual HRESULT ApplyNative(PCWSTR name);
		virtual LRESULT Message(HWND window, UINT message, WPARAM wp, LPARAM lp, BOOL content, BOOL* handled);
		virtual HRESULT SetValue(UINT property, QuadooProperty::Type type, IJSONValue* value);
	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooProgressBar)
			UNK_CHAIN_TO(Super)
		END_UNK_MAP
		CProgressBar(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent);
		static const TypeSpec* Type();
		STDMETHOD(GetPosition)(INT* value);
		STDMETHOD(SetPosition)(INT value);
		STDMETHOD(GetMin)(INT* value);
		STDMETHOD(SetMin)(INT value);
		STDMETHOD(GetMax)(INT* value);
		STDMETHOD(SetMax)(INT value);
		STDMETHOD(GetStep)(INT* value);
		STDMETHOD(SetStep)(INT value);
		STDMETHOD(StepIt)(VOID);
	};
	HRESULT RegisterProgressBarFactory(IQuadooControlRegistry* registry);
}
