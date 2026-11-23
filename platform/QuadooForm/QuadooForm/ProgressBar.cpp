#include "ProgressBar.h"

namespace QF
{
	static const DWORD Stored = QuadooForm::PropertySerializable;
	static const PropertySpec properties[] =
	{
		{L"Width",QuadooProperty::Int,Stored | QuadooForm::PropertyAffectsLayout,150,3},
		{L"Height",QuadooProperty::Int,Stored | QuadooForm::PropertyAffectsLayout,20,4},
		{L"Min",QuadooProperty::Int,Stored,0,24,NULL,0,NULL,LONG_MIN,LONG_MAX,TRUE},
		{L"Max",QuadooProperty::Int,Stored,100,25,NULL,0,NULL,LONG_MIN,LONG_MAX,TRUE},
		{L"Position",QuadooProperty::Int,Stored,0,26,NULL,0,NULL,LONG_MIN,LONG_MAX,TRUE},
		{L"Step",QuadooProperty::Int,Stored,10,27,NULL,0,NULL,LONG_MIN,LONG_MAX,TRUE}
	};
	const TypeSpec* CProgressBar::Type()
	{
		static const TypeSpec type = {L"ProgressBar",NativeWindowType(),properties,ARRAYSIZE(properties),NULL,0,IDI_PALETTE_PROGRESSBAR};
		return &type;
	}
	HRESULT RegisterProgressBarFactory(IQuadooControlRegistry* registry)
	{
		HRESULT hr;
		TStackRef<CFactory> factory;
		factory.Attach(__new TFactory<CProgressBar>);
		CheckAlloc(factory);
		Check(factory->Initialize());
		Check(registry->RegisterFactory(factory));
	Cleanup:
		return hr;
	}
	CProgressBar::CProgressBar(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) :
		Super(factory,context,parent)
	{
	}
	HRESULT CProgressBar::CreateNative()
	{
		HRESULT hr;
		INITCOMMONCONTROLSEX controls = {sizeof(controls),ICC_PROGRESS_CLASS};
		TStackRef<IJSONValue> position;
		INT minimum = Integer(L"Min");
		INT maximum = Integer(L"Max");
		INT current = Integer(L"Position");
		CheckIf(minimum > maximum,E_INVALIDARG);
		INT clamped = max(minimum,min(maximum,current));
		if(current != clamped)
		{
			Check(JSONCreateInteger(clamped,&position));
			Check(SetStoredValue(L"Position",position));
			Check(Dirty(L"Position"));
		}
		CheckIf(!InitCommonControlsEx(&controls),HrEnsureLastError());
		Check(Super::CreateNative());
		SynchronizeProgress();
		Check(ApplyNative(L"BackColor"));
		Check(ApplyNative(L"ForeColor"));
	Cleanup:
		return hr;
	}
	VOID CProgressBar::NativeStyle(PCWSTR* name, DWORD* style, DWORD* extended)
	{
		Super::NativeStyle(name,style,extended);
		*name = PROGRESS_CLASSW;
		*style |= PBS_SMOOTH;
	}
	VOID CProgressBar::SynchronizeProgress()
	{
		if(m_hwnd)
		{
			SendMessageW(m_hwnd,PBM_SETRANGE32,Integer(L"Min"),Integer(L"Max"));
			SendMessageW(m_hwnd,PBM_SETSTEP,Integer(L"Step"),0);
			SendMessageW(m_hwnd,PBM_SETPOS,Integer(L"Position"),0);
		}
	}
	HRESULT CProgressBar::ApplyNative(PCWSTR name)
	{
		HRESULT hr;
		Check(Super::ApplyNative(name));
		if(m_hwnd)
		{
			if(!lstrcmpW(name,L"BackColor"))
			{
				SendMessageW(m_hwnd,PBM_SETBKCOLOR,0,ResolveQuadooColor(static_cast<DWORD>(Integer(name))));
			}
			else if(!lstrcmpW(name,L"ForeColor"))
			{
				SendMessageW(m_hwnd,PBM_SETBARCOLOR,0,ResolveQuadooColor(static_cast<DWORD>(Integer(name))));
			}
		}
	Cleanup:
		return hr;
	}
	HRESULT CProgressBar::SetValue(UINT property, QuadooProperty::Type type, IJSONValue* value)
	{
		HRESULT hr;
		TStackRef<CControl> keepAlive(this);
		TStackRef<IJSONValue> normalized;
		TStackRef<IJSONValue> clampedValue;
		INT number;
		Check(CheckAlive());
		CheckIf(property >= static_cast<UINT>(m_values.Length()),DISP_E_MEMBERNOTFOUND);
		PCWSTR name = m_values[property].spec->name;
		if(lstrcmpW(name,L"Min") && lstrcmpW(name,L"Max") && lstrcmpW(name,L"Position") && lstrcmpW(name,L"Step"))
		{
			Check(Super::SetValue(property,type,value));
			goto Cleanup;
		}
		CheckIf(type != QuadooProperty::Int,DISP_E_TYPEMISMATCH);
		Check(NormalizeValue(m_values[property].spec,value,&normalized));
		Check(normalized->GetInteger(&number));
		INT minimum = !lstrcmpW(name,L"Min") ? number : Integer(L"Min");
		INT maximum = !lstrcmpW(name,L"Max") ? number : Integer(L"Max");
		CheckIf(minimum > maximum,E_INVALIDARG);
		INT position = Integer(L"Position");
		INT next = !lstrcmpW(name,L"Position") ? number : position;
		next = max(minimum,min(maximum,next));
		if(!lstrcmpW(name,L"Position") && next != number)
		{
			normalized.Release();
			Check(JSONCreateInteger(next,&normalized));
		}
		BOOL changed = JSONCompare(m_values[property].value,normalized) != 0;
		BOOL clamp = lstrcmpW(name,L"Position") && next != position;
		if(clamp)
		{
			Check(JSONCreateInteger(next,&clampedValue));
		}
		CheckIfIgnore(!changed && !clamp,S_FALSE);
		// Allocate and validate everything before publishing the new range and
		// position together. Observers always see a coherent state, including
		// when they edit or destroy the control during notification.
		if(changed)
		{
			Check(SetStoredValue(name,normalized));
			Check(Dirty(name));
		}
		if(clamp)
		{
			Check(SetStoredValue(L"Position",clampedValue));
			Check(Dirty(L"Position"));
		}
		SynchronizeProgress();
		Check(CheckAlive());
		Check(FlushNotifications());
	Cleanup:
		return hr;
	}
	LRESULT CProgressBar::Message(HWND window, UINT message, WPARAM wp, LPARAM lp, BOOL content, BOOL* handled)
	{
		if(message == WM_SYSCOLORCHANGE || message == WM_THEMECHANGED)
		{
			SendMessageW(window,PBM_SETBKCOLOR,0,ResolveQuadooColor(static_cast<DWORD>(Integer(L"BackColor"))));
			SendMessageW(window,PBM_SETBARCOLOR,0,ResolveQuadooColor(static_cast<DWORD>(Integer(L"ForeColor"))));
		}
		return Super::Message(window,message,wp,lp,content,handled);
	}
	HRESULT CProgressBar::ReadInteger(PCWSTR name, INT* value)
	{
		HRESULT hr;
		TStackRef<IJSONValue> stored;
		CheckIf(!value,E_POINTER);
		*value = 0;
		Check(CheckAlive());
		Check(StoredValue(name,&stored));
		Check(stored->GetInteger(value));
	Cleanup:
		return hr;
	}
	HRESULT CProgressBar::WriteInteger(PCWSTR name, INT value)
	{
		HRESULT hr;
		TStackRef<IJSONValue> stored;
		Check(CheckAlive());
		Check(JSONCreateInteger(value,&stored));
		Check(SetNamedValue(name,QuadooProperty::Int,stored));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CProgressBar::GetPosition(INT* value)
	{
		return ReadInteger(L"Position",value);
	}
	STDMETHODIMP CProgressBar::SetPosition(INT value)
	{
		return WriteInteger(L"Position",value);
	}
	STDMETHODIMP CProgressBar::GetMin(INT* value)
	{
		return ReadInteger(L"Min",value);
	}
	STDMETHODIMP CProgressBar::SetMin(INT value)
	{
		return WriteInteger(L"Min",value);
	}
	STDMETHODIMP CProgressBar::GetMax(INT* value)
	{
		return ReadInteger(L"Max",value);
	}
	STDMETHODIMP CProgressBar::SetMax(INT value)
	{
		return WriteInteger(L"Max",value);
	}
	STDMETHODIMP CProgressBar::GetStep(INT* value)
	{
		return ReadInteger(L"Step",value);
	}
	STDMETHODIMP CProgressBar::SetStep(INT value)
	{
		return WriteInteger(L"Step",value);
	}
	STDMETHODIMP CProgressBar::StepIt()
	{
		HRESULT hr;
		Check(CheckAlive());
		// Wide arithmetic permits the complete signed 32-bit logical range.
		LONGLONG minimum = Integer(L"Min");
		LONGLONG span = static_cast<LONGLONG>(Integer(L"Max")) - minimum;
		LONGLONG offset = static_cast<LONGLONG>(Integer(L"Position")) - minimum + Integer(L"Step");
		if(!span)
		{
			offset = 0;
		}
		else if(offset < 0)
		{
			// Native negative steps that cross an exact multiple land at Max.
			offset = span + offset % span;
		}
		else if(offset > span)
		{
			offset %= span;
		}
		Check(SetPosition(static_cast<INT>(minimum + offset)));
	Cleanup:
		return hr;
	}
}
