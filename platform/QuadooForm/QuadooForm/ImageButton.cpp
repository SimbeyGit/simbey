#include "ImageButton.h"

namespace QF
{
	static const EnumSpec positions[] = {{L"Left",0},{L"Top",1}};
	static const EnumSpec filters[] = {{L"Nearest",0},{L"Bilinear",1},{L"Bicubic",2},{L"HighQuality",3}};
	static const PropertySpec properties[] =
	{
		{L"Picture",QuadooProperty::Picture,QuadooForm::PropertySerializable,0,27},
		{L"DisabledPicture",QuadooProperty::Picture,QuadooForm::PropertySerializable,0,28},
		{L"ImagePosition",QuadooProperty::Enum,QuadooForm::PropertySerializable,0,29,positions,ARRAYSIZE(positions)},
		{L"ImageWidth",QuadooProperty::Int,QuadooForm::PropertySerializable,16,30,NULL,0,NULL,1,256,TRUE},
		{L"ImageHeight",QuadooProperty::Int,QuadooForm::PropertySerializable,16,31,NULL,0,NULL,1,256,TRUE},
		{L"Spacing",QuadooProperty::Int,QuadooForm::PropertySerializable,4,32,NULL,0,NULL,0,1024,TRUE},
		{L"Interpolation",QuadooProperty::Enum,QuadooForm::PropertySerializable,3,33,filters,ARRAYSIZE(filters)}
	};
	const TypeSpec* CImageButton::Type()
	{
		static const TypeSpec type = {L"ImageButton",&ButtonType,properties,ARRAYSIZE(properties),NULL,0,IDI_PALETTE_IMAGEBUTTON};
		return &type;
	}
	HRESULT RegisterImageButtonFactory(IQuadooControlRegistry* registry)
	{
		HRESULT hr;
		TStackRef<CFactory> factory;
		factory.Attach(__new TFactory<CImageButton>);
		CheckAlloc(factory);
		Check(factory->Initialize());
		Check(registry->RegisterFactory(factory));
	Cleanup:
		return hr;
	}
	CImageButton::CImageButton(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) :
		CButton(factory,context,parent), m_hot(FALSE), m_default(FALSE)
	{
	}
	HRESULT CImageButton::LoadPictures()
	{
		HRESULT hr;
		TStackRef<IJSONValue> value;
		TStackRef<QFI::CImageResource> picture;
		TStackRef<QFI::CImageResource> disabled;
		Check(StoredValue(L"Picture",&value));
		Check(QFI::CImageResource::Create(value,&picture));
		value.Release();
		Check(StoredValue(L"DisabledPicture",&value));
		Check(QFI::CImageResource::Create(value,&disabled));
		m_picture = picture;
		m_disabledPicture = disabled;
	Cleanup:
		return hr;
	}
	HRESULT CImageButton::UpdatePicture(PCWSTR name)
	{
		HRESULT hr;
		TStackRef<IJSONValue> value;
		TStackRef<QFI::CImageResource> picture;
		Check(StoredValue(name,&value));
		Check(QFI::CImageResource::Create(value,&picture));
		if(!lstrcmpW(name,L"Picture"))
		{
			m_picture = picture;
		}
		else
		{
			m_disabledPicture = picture;
		}
	Cleanup:
		return hr;
	}
	HRESULT CImageButton::CreateNative()
	{
		HRESULT hr;
		Check(LoadPictures());
		Check(CButton::CreateNative());
	Cleanup:
		return hr;
	}
	VOID CImageButton::NativeStyle(PCWSTR* name, DWORD* style, DWORD* extended)
	{
		CButton::NativeStyle(name,style,extended);
		*style = (*style & ~BS_TYPEMASK) | BS_OWNERDRAW;
	}
	HRESULT CImageButton::DestroyNative()
	{
		HRESULT hr = CButton::DestroyNative();
		m_theme.Close();
		m_picture.Release();
		m_disabledPicture.Release();
		return hr;
	}
	VOID CImageButton::NativeDestroyed(BOOL content)
	{
		m_theme.Close();
		m_hot = FALSE;
		m_picture.Release();
		m_disabledPicture.Release();
		CButton::NativeDestroyed(content);
	}
	HRESULT CImageButton::ApplyNative(PCWSTR name)
	{
		HRESULT hr;
		if(!lstrcmpW(name,L"Picture") || !lstrcmpW(name,L"DisabledPicture"))
		{
			// The property layer restores its JSON slot if decoding fails.
			Check(UpdatePicture(name));
		}
		Check(CButton::ApplyNative(name));
		if(m_hwnd)
		{
			InvalidateRect(m_hwnd,NULL,FALSE);
		}
	Cleanup:
		return hr;
	}
	STDMETHODIMP CImageButton::SetDefaultAppearance(BOOL selected)
	{
		HRESULT hr;
		Check(CheckAlive());
		if(m_default != (selected != FALSE))
		{
			m_default = selected != FALSE;
			if(m_hwnd)
			{
				InvalidateRect(m_hwnd,NULL,FALSE);
			}
		}
	Cleanup:
		return hr;
	}
	HRESULT CImageButton::PaintButton(HDC dc, const RECT* bounds, UINT state)
	{
		HRESULT hr;
		ImageButtonAppearance a = {0};
		CRString caption;
		TStackRef<QFI::CImageResource> picture(m_picture);
		TStackRef<QFI::CImageResource> disabled(m_disabledPicture);
		Check(StoredString(L"Caption",&caption));
		a.dpi = m_dpi;
		a.position = Integer(L"ImagePosition");
		a.width = Integer(L"ImageWidth");
		a.height = Integer(L"ImageHeight");
		a.spacing = Integer(L"Spacing");
		a.filter = Integer(L"Interpolation");
		a.enabled = !(state & ODS_DISABLED);
		for(HWND window = m_hwnd; window; window = ::GetParent(window))
		{
			if(!IsWindowEnabled(window))
			{
				a.enabled = FALSE;
			}
			if(!(GetWindowLongW(window,GWL_STYLE) & WS_CHILD))
			{
				break;
			}
		}
		a.pressed = (state & ODS_SELECTED) != 0;
		a.hot = m_hot;
		a.focused = (state & ODS_FOCUS) != 0;
		a.isDefault = m_default;
		LRESULT cues = SendMessageW(m_hwnd,WM_QUERYUISTATE,0,0);
		a.hideFocus = (cues & UISF_HIDEFOCUS) != 0 || (state & ODS_NOFOCUSRECT) != 0;
		a.hideAccel = (cues & UISF_HIDEACCEL) != 0 || (state & ODS_NOACCEL) != 0;
		a.font = m_font;
		a.fore = ResolveQuadooColor(Integer(L"ForeColor"));
		a.back = ResolveQuadooColor(Integer(L"BackColor"));
		a.caption = caption.c_wstr();
		a.length = min(static_cast<UINT>(RStrLen(caption)),32767U);
		a.picture = picture;
		a.disabledPicture = disabled;
		m_theme.Open(m_hwnd);
		Check(PaintImageButton(dc,bounds,&a,&m_theme));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CImageButton::HandleMessage(HWND window, UINT message, WPARAM wp, LPARAM lp, BOOL* handled, LRESULT* result)
	{
		HRESULT hr;
		TStackRef<CImageButton> keepAlive(this);
		CheckIf(!handled || !result,E_POINTER);
		*handled = FALSE;
		*result = 0;
		Check(CheckAlive());
		if(window == m_hwnd && message == WM_DRAWITEM && lp)
		{
			const DRAWITEMSTRUCT* item = reinterpret_cast<const DRAWITEMSTRUCT*>(lp);
			if(item->CtlType == ODT_BUTTON && item->hwndItem == m_hwnd)
			{
				*handled = TRUE;
				*result = TRUE;
				Check(PaintButton(item->hDC,&item->rcItem,item->itemState));
			}
		}
		else
		{
			Check(CButton::HandleMessage(window,message,wp,lp,handled,result));
		}
	Cleanup:
		return hr;
	}
	LRESULT CImageButton::Message(HWND window, UINT message, WPARAM wp, LPARAM lp, BOOL content, BOOL* handled)
	{
		if(message == BM_SETSTYLE)
		{
			UINT type = wp & BS_TYPEMASK;
			if(type == BS_PUSHBUTTON || type == BS_DEFPUSHBUTTON)
			{
				ReportCallback(SetDefaultAppearance(type == BS_DEFPUSHBUTTON),L"Default appearance");
			}
			*handled = TRUE;
			return 0;
		}
		if(message == WM_GETDLGCODE)
		{
			*handled = TRUE;
			return DLGC_BUTTON | (m_default ? DLGC_DEFPUSHBUTTON : DLGC_UNDEFPUSHBUTTON);
		}
		if(message == WM_PRINTCLIENT)
		{
			RECT bounds;
			UINT state = 0;
			GetClientRect(window,&bounds);
			if(SendMessageW(window,BM_GETSTATE,0,0) & BST_PUSHED)
			{
				state |= ODS_SELECTED;
			}
			if(GetFocus() == window)
			{
				state |= ODS_FOCUS;
			}
			ReportCallback(PaintButton(reinterpret_cast<HDC>(wp),&bounds,state),L"Button printing");
			*handled = TRUE;
			return 0;
		}
		if(message == WM_MOUSEMOVE && !m_hot && IsWindowEnabled(window))
		{
			TRACKMOUSEEVENT track = {sizeof(track),TME_LEAVE,window,0};
			m_hot = TrackMouseEvent(&track);
			InvalidateRect(window,NULL,FALSE);
		}
		if(message == WM_MOUSELEAVE || (message == WM_ENABLE && !wp))
		{
			m_hot = FALSE;
			InvalidateRect(window,NULL,FALSE);
		}
		if(message == WM_THEMECHANGED || message == WM_SYSCOLORCHANGE || message == WM_SETTINGCHANGE)
		{
			m_theme.Close();
			InvalidateRect(window,NULL,FALSE);
		}
		if(message == WM_UPDATEUISTATE || message == WM_CHANGEUISTATE || message == WM_SETFOCUS || message == WM_KILLFOCUS)
		{
			InvalidateRect(window,NULL,FALSE);
		}
		return CButton::Message(window,message,wp,lp,content,handled);
	}
}
