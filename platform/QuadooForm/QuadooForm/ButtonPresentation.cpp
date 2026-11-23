#include "Runtime.h"

namespace QF
{
	HRESULT SetDefaultButtonAppearance(IUnknown* button, BOOL selected)
	{
		HRESULT hr;
		TStackRef<IQuadooButtonPresentation> presentation;
		Check(QueryOptional(button,&presentation));
		if(presentation)
		{
			Check(presentation->SetDefaultAppearance(selected));
		}
	Cleanup:
		return hr;
	}
	STDMETHODIMP CButton::SetDefaultAppearance(BOOL selected)
	{
		HRESULT hr;
		Check(CheckAlive());
		if(m_hwnd)
		{
			DWORD type = GetWindowLongW(m_hwnd,GWL_STYLE) & BS_TYPEMASK;
			DWORD wanted = selected ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON;
			if((type == BS_PUSHBUTTON || type == BS_DEFPUSHBUTTON) && type != wanted)
			{
				SendMessageW(m_hwnd,BM_SETSTYLE,wanted,TRUE);
			}
		}
	Cleanup:
		return hr;
	}
	HRESULT ForwardDrawItem(IQuadooControlContainer* parent, const DRAWITEMSTRUCT* item, WPARAM id, BOOL* handled, LRESULT* result)
	{
		HRESULT hr;
		UINT count;
		CheckIf(!handled || !result,E_POINTER);
		*handled = FALSE;
		*result = 0;
		CheckIf(!parent || !item,E_INVALIDARG);
		CheckIfIgnore(item->CtlType == ODT_MENU || !item->hwndItem,S_OK);
		Check(parent->GetChildCount(&count));
		CheckIf(count > 4096,QuadooForm::EInvalidDefinition);
		for(UINT n = 0; n < count; n++)
		{
			TStackRef<IQuadooControl> child;
			TStackRef<IQuadooWinControl> window;
			TStackRef<IQuadooMessageHandler> handler;
			HWND native;
			Check(parent->GetChild(n,&child));
			Check(QueryOptional(child,&window));
			if(!window)
			{
				continue;
			}
			hr = window->GetHandle(&native);
			if(hr == QuadooForm::EDestroyed)
			{
				continue;
			}
			Check(hr);
			if(native != item->hwndItem)
			{
				continue;
			}
			Check(QueryOptional(child,&handler));
			if(handler)
			{
				Check(handler->HandleMessage(native,WM_DRAWITEM,id,reinterpret_cast<LPARAM>(item),handled,result));
			}
			break;
		}
		hr = S_OK;
	Cleanup:
		return hr;
	}
}
