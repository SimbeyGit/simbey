#pragma once
#include "Runtime.h"
#include "ButtonDrawing.h"

namespace QF
{
	class CImageButton : public CButton
	{
		CButtonTheme m_theme;
		BOOL m_hot;
		BOOL m_default;
		HRESULT PaintButton(HDC dc, const RECT* bounds, UINT state);
	protected:
		TStackRef<QFI::CImageResource> m_picture;
		TStackRef<QFI::CImageResource> m_disabledPicture;
		HRESULT LoadPictures();
		HRESULT UpdatePicture(PCWSTR name);
		virtual HRESULT CreateNative();
		virtual HRESULT DestroyNative();
		virtual VOID NativeDestroyed(BOOL content);
		virtual VOID NativeStyle(PCWSTR* name, DWORD* style, DWORD* extended);
		virtual HRESULT ApplyNative(PCWSTR name);
		virtual LRESULT Message(HWND window, UINT message, WPARAM wp, LPARAM lp, BOOL content, BOOL* handled);
	public:
		CImageButton(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent);
		static const TypeSpec* Type();
		STDMETHOD(SetDefaultAppearance)(BOOL selected);
		STDMETHOD(HandleMessage)(HWND window, UINT message, WPARAM wp, LPARAM lp, BOOL* handled, LRESULT* result);
	};
	HRESULT RegisterImageButtonFactory(IQuadooControlRegistry* registry);
}
